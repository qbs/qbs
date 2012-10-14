/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "buildgraph.h"
#include "artifact.h"
#include "command.h"
#include "rulegraph.h"
#include "transformer.h"

#include <language/loader.h>
#include <language/qbsengine.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/scannerpluginmanager.h>
#include <tools/logger.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>

#include <QCache>
#include <QDir>
#include <QDirIterator>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutex>
#include <QScriptProgram>
#include <QScriptValueIterator>
#include <QThread>

namespace qbs {

BuildProduct::BuildProduct()
    : project(0)
{
}

BuildProduct::~BuildProduct()
{
    qDeleteAll(artifacts);
}

static void internalDump(const BuildProduct *product, Artifact *artifact, QByteArray indent)
{
    Artifact *artifactInProduct = product->lookupArtifact(artifact->filePath());
    if (artifactInProduct && artifactInProduct != artifact) {
        qFatal("\ntree corrupted. %p ('%s') resolves to %p ('%s')\n",
                artifact,  qPrintable(artifact->filePath()), product->lookupArtifact(artifact->filePath()),
                qPrintable(product->lookupArtifact(artifact->filePath())->filePath()));

    }
    printf("%s", indent.constData());
    printf("Artifact (%p) ", artifact);
    printf("%s%s %s [%s]",
           qPrintable(QString(toString(artifact->buildState).at(0))),
           artifactInProduct ? "" : " SBS",     // SBS == side-by-side artifact from other product
           qPrintable(artifact->filePath()),
           qPrintable(QStringList(artifact->fileTags.toList()).join(",")));
    printf("\n");
    indent.append("  ");
    foreach (Artifact *child, artifact->children) {
        internalDump(product, child, indent);
    }
}

void BuildProduct::dump() const
{
    foreach (Artifact *artifact, artifacts)
        if (artifact->parents.isEmpty())
            internalDump(this, artifact, QByteArray());
}

const QList<Rule::ConstPtr> &BuildProduct::topSortedRules() const
{
    if (m_topSortedRules.isEmpty()) {
        RuleGraph ruleGraph;
        ruleGraph.build(rProduct->rules, rProduct->fileTags);
//        ruleGraph.dump();
        m_topSortedRules = ruleGraph.topSorted();
//        int i=0;
//        foreach (Rule::Ptr r, m_topSortedRules)
//            qDebug() << ++i << r->toString() << (void*)r.data();
    }
    return m_topSortedRules;
}

Artifact *BuildProduct::lookupArtifact(const QString &dirPath, const QString &fileName) const
{
    QList<Artifact *> artifacts = project->lookupArtifacts(dirPath, fileName);
    for (QList<Artifact *>::const_iterator it = artifacts.constBegin(); it != artifacts.constEnd(); ++it)
        if ((*it)->product == this)
            return *it;
    return 0;
}

Artifact *BuildProduct::lookupArtifact(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(dirPath, fileName);
}

BuildGraph::BuildGraph(QbsEngine *engine)
    : m_progressObserver(0)
    , m_engine(engine)
    , m_initEngineCalls(0)
{
    m_prepareScriptScope = m_engine->newObject();
    ProcessCommand::setupForJavaScript(m_prepareScriptScope);
    JavaScriptCommand::setupForJavaScript(m_prepareScriptScope);
}

BuildGraph::~BuildGraph()
{
}

void BuildGraph::insert(BuildProduct::Ptr product, Artifact *n) const
{
    insert(product.data(), n);
}

void BuildGraph::insert(BuildProduct *product, Artifact *n) const
{
    Q_ASSERT(n->product == 0);
    Q_ASSERT(!n->filePath().isEmpty());
    Q_ASSERT(!product->artifacts.contains(n));
#ifdef QT_DEBUG
    foreach (BuildProduct::Ptr otherProduct, product->project->buildProducts()) {
        if (otherProduct->lookupArtifact(n->filePath())) {
            if (n->artifactType == Artifact::Generated) {
                QString pl;
                pl.append(QString("  - %1 \n").arg(product->rProduct->name));
                foreach (BuildProduct::Ptr p, product->project->buildProducts()) {
                    if (p->lookupArtifact(n->filePath())) {
                        pl.append(QString("  - %1 \n").arg(p->rProduct->name));
                    }
                }
                throw Error(QString ("BUG: already inserted in this project: %1\n%2"
                            )
                        .arg(n->filePath())
                        .arg(pl)
                        );
            }
        }
    }
#endif
    product->artifacts.insert(n);
    n->product = product;
    product->project->insertIntoArtifactLookupTable(n);
    product->project->markDirty();

    if (qbsLogLevel(LoggerTrace))
        qbsTrace("[BG] insert artifact '%s'", qPrintable(n->filePath()));
}

void BuildGraph::setupScriptEngineForProduct(QbsEngine *engine,
                                             const ResolvedProduct::ConstPtr &product,
                                             Rule::ConstPtr rule,
                                             QScriptValue targetObject)
{
    const ResolvedProject *lastSetupProject = reinterpret_cast<ResolvedProject *>(engine->property("lastSetupProject").toULongLong());
    const ResolvedProduct *lastSetupProduct = reinterpret_cast<ResolvedProduct *>(engine->property("lastSetupProduct").toULongLong());

    if (lastSetupProject != product->project) {
        engine->setProperty("lastSetupProject",
                QVariant(reinterpret_cast<qulonglong>(product->project)));
        QScriptValue projectScriptValue;
        projectScriptValue = engine->newObject();
        projectScriptValue.setProperty("filePath", product->project->qbsFile);
        projectScriptValue.setProperty("path", FileInfo::path(product->project->qbsFile));
        targetObject.setProperty("project", projectScriptValue);
    }

    QScriptValue productScriptValue;
    if (lastSetupProduct != product.data()) {
        engine->setProperty("lastSetupProduct",
                QVariant(reinterpret_cast<qulonglong>(product.data())));
        productScriptValue = product->configuration->toScriptValue(engine);
        productScriptValue.setProperty("name", product->name);
        QString destinationDirectory = product->destinationDirectory;
        if (destinationDirectory.isEmpty())
            destinationDirectory = ".";
        productScriptValue.setProperty("destinationDirectory", destinationDirectory);
        targetObject.setProperty("product", productScriptValue);
    } else {
        productScriptValue = targetObject.property("product");
    }

    // If the Rule is in a Module, set up the 'module' property
    if (!rule->module->name.isEmpty())
        productScriptValue.setProperty("module", productScriptValue.property("modules").property(rule->module->name));

    engine->import(rule->jsImports, targetObject, targetObject);
}

void BuildGraph::setupScriptEngineForArtifact(BuildProduct *product, Artifact *artifact)
{
    QString inFileName = artifact->fileName();
    QString inBaseName = FileInfo::baseName(artifact->filePath());
    QString inCompleteBaseName = FileInfo::completeBaseName(artifact->filePath());

    QString basedir;
    if (artifact->artifactType == Artifact::SourceFile) {
        QDir sourceDir(product->rProduct->sourceDirectory);
        basedir = FileInfo::path(sourceDir.relativeFilePath(artifact->filePath()));
    } else {
        QDir buildDir(product->project->buildGraph()->buildDirectoryRoot() + product->project->resolvedProject()->id());
        basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
    }

    QScriptValue modulesScriptValue = artifact->configuration->toScriptValue(m_engine);
    modulesScriptValue = modulesScriptValue.property("modules");

    // expose per file properties we want to use in an Artifact within a Rule
    QScriptValue scriptValue = m_engine->newObject();
    scriptValue.setProperty("fileName", inFileName);
    scriptValue.setProperty("baseName", inBaseName);
    scriptValue.setProperty("completeBaseName", inCompleteBaseName);
    scriptValue.setProperty("baseDir", basedir);
    scriptValue.setProperty("modules", modulesScriptValue);

    m_scope.setProperty("input", scriptValue);
    Q_ASSERT_X(scriptValue.strictlyEquals(m_engine->evaluate("input")),
               "BG", "The input object is not in current scope.");
}

void BuildGraph::applyRules(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag)
{
    EngineInitializer engineInitializer(this);
    foreach (Rule::ConstPtr rule, product->topSortedRules())
        applyRule(product, artifactsPerFileTag, rule);
}

/*!
  * Runs a cycle detection on the BG and throws an exception if there is one.
  */
void BuildGraph::detectCycle(BuildProject *project)
{
    QElapsedTimer *t = 0;
    if (qbsLogLevel(LoggerTrace)) {
        t = new QElapsedTimer;
        t->start();
        qbsTrace() << "[BG] running cycle detection on project '" + project->resolvedProject()->id() + "'";
    }

    foreach (BuildProduct::Ptr product, project->buildProducts())
        foreach (Artifact *artifact, product->targetArtifacts)
            detectCycle(artifact);

    if (qbsLogLevel(LoggerTrace)) {
        qint64 elapsed = t->elapsed();
        qbsTrace() << "[BG] cycle detection for project '" + project->resolvedProject()->id() + "' took " << elapsed << " ms";
        delete t;
    }
}

void BuildGraph::detectCycle(Artifact *a)
{
    QSet<Artifact *> done, currentBranch;
    detectCycle(a, done, currentBranch);
}

void BuildGraph::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

void BuildGraph::detectCycle(Artifact *v, QSet<Artifact *> &done, QSet<Artifact *> &currentBranch)
{
    currentBranch += v;
    for (ArtifactList::const_iterator it = v->children.begin(); it != v->children.end(); ++it) {
        Artifact *u = *it;
        if (currentBranch.contains(u))
            throw Error("Cycle in build graph detected.");
        if (!done.contains(u))
            detectCycle(u, done, currentBranch);
    }
    currentBranch -= v;
    done += v;
}

static AbstractCommand *createCommandFromScriptValue(const QScriptValue &scriptValue, const CodeLocation &codeLocation)
{
    if (scriptValue.isUndefined() || !scriptValue.isValid())
        return 0;
    AbstractCommand *cmdBase = 0;
    QString className = scriptValue.property("className").toString();
    if (className == "Command")
        cmdBase = new ProcessCommand;
    else if (className == "JavaScriptCommand")
        cmdBase = new JavaScriptCommand;
    if (cmdBase)
        cmdBase->fillFromScriptValue(&scriptValue, codeLocation);
    return cmdBase;
}

void BuildGraph::applyRule(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag,
                           Rule::ConstPtr rule)
{
    setupScriptEngineForProduct(m_engine, product->rProduct, rule, m_scope);
    Q_ASSERT_X(m_scope.property("product").strictlyEquals(m_engine->evaluate("product")),
               "BG", "Product object is not in current scope.");

    if (rule->isMultiplexRule()) {
        // apply the rule once for a set of inputs

        QSet<Artifact*> inputArtifacts;
        foreach (const QString &fileTag, rule->inputs)
            inputArtifacts.unite(artifactsPerFileTag.value(fileTag));

        if (!inputArtifacts.isEmpty())
            applyRule(product, artifactsPerFileTag, rule, inputArtifacts);
    } else {
        // apply the rule once for each input

        QSet<Artifact*> inputArtifacts;
        foreach (const QString &fileTag, rule->inputs) {
            foreach (Artifact *inputArtifact, artifactsPerFileTag.value(fileTag)) {
                inputArtifacts.insert(inputArtifact);
                applyRule(product, artifactsPerFileTag, rule, inputArtifacts);
                inputArtifacts.clear();
            }
        }
    }
}

void BuildGraph::createOutputArtifact(
        BuildProduct *product,
        const Rule::ConstPtr &rule, const RuleArtifact::ConstPtr &ruleArtifact,
        const QSet<Artifact *> &inputArtifacts,
        QList< QPair<const RuleArtifact*, Artifact *> > *ruleArtifactArtifactMap,
        QList<Artifact *> *outputArtifacts,
        QSharedPointer<Transformer> &transformer)
{
    QScriptValue scriptValue = m_engine->evaluate(ruleArtifact->fileScript);
    if (scriptValue.isError() || m_engine->hasUncaughtException())
        throw Error("Error in Rule.Artifact fileName: " + scriptValue.toString());
    QString outputPath = scriptValue.toString();
    outputPath.replace("..", "dotdot");     // don't let the output artifact "escape" its build dir
    outputPath = resolveOutPath(outputPath, product);

    Artifact *outputArtifact = product->lookupArtifact(outputPath);
    if (outputArtifact) {
        if (outputArtifact->transformer && outputArtifact->transformer != transformer) {
            // This can happen when applying rules after scanning for additional file tags.
            // We just regenerate the transformer.
            if (qbsLogLevel(LoggerTrace))
                qbsTrace("[BG] regenerating transformer for '%s'", qPrintable(fileName(outputArtifact)));
            transformer = outputArtifact->transformer;
            transformer->inputs += inputArtifacts;

            if (transformer->inputs.count() > 1 && !rule->isMultiplexRule()) {
                QString th = "[" + QStringList(outputArtifact->fileTags.toList()).join(", ") + "]";
                QString e = tr("Conflicting rules for producing %1 %2 \n").arg(outputArtifact->filePath(), th);
                th = "[" + rule->inputs.join(", ")
                   + "] -> [" + QStringList(outputArtifact->fileTags.toList()).join(", ") + "]";

                e += QString("  while trying to apply:   %1:%2:%3  %4\n")
                    .arg(rule->script->location.fileName)
                    .arg(rule->script->location.line)
                    .arg(rule->script->location.column)
                    .arg(th);

                e += QString("  was already defined in:  %1:%2:%3  %4\n")
                    .arg(outputArtifact->transformer->rule->script->location.fileName)
                    .arg(outputArtifact->transformer->rule->script->location.line)
                    .arg(outputArtifact->transformer->rule->script->location.column)
                    .arg(th);
                throw Error(e);
            }
        }
        outputArtifact->fileTags += ruleArtifact->fileTags.toSet();
    } else {
        outputArtifact = new Artifact(product->project);
        outputArtifact->artifactType = Artifact::Generated;
        outputArtifact->setFilePath(outputPath);
        outputArtifact->fileTags = ruleArtifact->fileTags.toSet();
        insert(product, outputArtifact);
    }

    if (outputArtifact->fileTags.isEmpty())
        outputArtifact->fileTags = product->rProduct->fileTagsForFileName(outputArtifact->fileName());

    if (rule->isMultiplexRule())
        outputArtifact->configuration = product->rProduct->configuration;
    else
        outputArtifact->configuration = (*inputArtifacts.constBegin())->configuration;

    foreach (Artifact *inputArtifact, inputArtifacts) {
        Q_ASSERT(outputArtifact != inputArtifact);
        loggedConnect(outputArtifact, inputArtifact);
    }
    ruleArtifactArtifactMap->append(qMakePair<const RuleArtifact *, Artifact *>(ruleArtifact.data(), outputArtifact));
    outputArtifacts->append(outputArtifact);

    // create transformer if not already done so
    if (!transformer) {
        transformer = Transformer::create();
        transformer->rule = rule;
        transformer->inputs = inputArtifacts;
    }
    outputArtifact->transformer = transformer;
}

void BuildGraph::applyRule(BuildProduct *product,
        QMap<QString, QSet<Artifact *> > &artifactsPerFileTag, const Rule::ConstPtr &rule,
        const QSet<Artifact *> &inputArtifacts)
{
    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] apply rule " << rule->toString() << " " << toStringList(inputArtifacts).join(",\n            ");

    QList<QPair<const RuleArtifact *, Artifact *> > ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    QSet<Artifact *> usingArtifacts;
    foreach (BuildProduct *dep, product->usings) {
        foreach (Artifact *targetArtifact, dep->targetArtifacts) {
            ArtifactList sbsArtifacts = targetArtifact->sideBySideArtifacts;
            sbsArtifacts.insert(targetArtifact);
            foreach (Artifact *artifact, sbsArtifacts) {
                QString matchingTag;
                foreach (const QString &tag, rule->usings) {
                    if (artifact->fileTags.contains(tag)) {
                        matchingTag = tag;
                        break;
                    }
                }
                if (matchingTag.isEmpty())
                    continue;
                usingArtifacts.insert(artifact);
            }
        }
    }

    // create the output artifacts from the set of input artifacts
    QSharedPointer<Transformer> transformer;
    foreach (const RuleArtifact::ConstPtr &ruleArtifact, rule->artifacts) {
        if (!rule->isMultiplexRule()) {
            foreach (Artifact *inputArtifact, inputArtifacts) {
                setupScriptEngineForArtifact(product, inputArtifact);
                QSet<Artifact *> oneInputArtifact;
                oneInputArtifact.insert(inputArtifact);
                createOutputArtifact(product, rule, ruleArtifact, oneInputArtifact,
                                 &ruleArtifactArtifactMap, &outputArtifacts, transformer);
            }
        } else {
            createOutputArtifact(product, rule, ruleArtifact, inputArtifacts,
                             &ruleArtifactArtifactMap, &outputArtifacts, transformer);
        }
    }

    foreach (Artifact *outputArtifact, outputArtifacts) {
        // insert the output artifacts into the pool of artifacts
        foreach (const QString &fileTag, outputArtifact->fileTags)
            artifactsPerFileTag[fileTag].insert(outputArtifact);

        // connect artifacts that match the file tags in explicitlyDependsOn
        foreach (const QString &fileTag, rule->explicitlyDependsOn)
            foreach (Artifact *dependency, artifactsPerFileTag.value(fileTag))
                loggedConnect(outputArtifact, dependency);

        // Transformer setup
        transformer->outputs.insert(outputArtifact);
        for (QSet<Artifact *>::const_iterator it = usingArtifacts.constBegin(); it != usingArtifacts.constEnd(); ++it) {
            Artifact *dep = *it;
            loggedConnect(outputArtifact, dep);
            transformer->inputs.insert(dep);
            foreach (Artifact *sideBySideDep, dep->sideBySideArtifacts) {
                loggedConnect(outputArtifact, sideBySideDep);
                transformer->inputs.insert(sideBySideDep);
            }
        }

        m_artifactsThatMustGetNewTransformers -= outputArtifact;
    }

    // setup side-by-side artifacts
    if (outputArtifacts.count() > 1)
        foreach (Artifact *sbs1, outputArtifacts)
            foreach (Artifact *sbs2, outputArtifacts)
                if (sbs1 != sbs2)
                    sbs1->sideBySideArtifacts.insert(sbs2);

    transformer->setupInputs(m_engine, m_scope);

    // change the transformer outputs according to the bindings in Artifact
    QScriptValue scriptValue;
    for (int i=ruleArtifactArtifactMap.count(); --i >= 0;) {
        const RuleArtifact *ra = ruleArtifactArtifactMap.at(i).first;
        if (ra->bindings.isEmpty())
            continue;

        // expose attributes of this artifact
        Artifact *outputArtifact = ruleArtifactArtifactMap.at(i).second;
        outputArtifact->configuration = Configuration::create(*outputArtifact->configuration);

        // ### clean m_engine first?
        m_scope.setProperty("fileName", m_engine->toScriptValue(outputArtifact->filePath()));
        m_scope.setProperty("fileTags", toScriptValue(m_engine, outputArtifact->fileTags));

        QVariantMap artifactModulesCfg = outputArtifact->configuration->value().value("modules").toMap();
        for (int i=0; i < ra->bindings.count(); ++i) {
            const RuleArtifact::Binding &binding = ra->bindings.at(i);
            scriptValue = m_engine->evaluate(binding.code);
            if (scriptValue.isError()) {
                QString msg = QLatin1String("evaluating rule binding '%1': %2");
                throw Error(msg.arg(binding.name.join(QLatin1String(".")), scriptValue.toString()), binding.location);
            }
            setConfigProperty(artifactModulesCfg, binding.name, scriptValue.toVariant());
        }
        QVariantMap outputArtifactConfiguration = outputArtifact->configuration->value();
        outputArtifactConfiguration.insert("modules", artifactModulesCfg);
        outputArtifact->configuration->setValue(outputArtifactConfiguration);
    }

    transformer->setupOutputs(m_engine, m_scope);

    // setup transform properties
    {
        const QVariantMap overriddenTransformProperties = product->rProduct->configuration->value().value("modules").toMap().value(rule->module->name).toMap().value(rule->objectId).toMap();
        /*
          overriddenTransformProperties contains the rule's transform properties that have been overridden in the project file.
          For example, if you set cpp.compiler.defines in your project file, that property appears here.
          */

        QMap<QString, QScriptProgram>::const_iterator it = rule->transformProperties.begin();
        for (; it != rule->transformProperties.end(); ++it)
        {
            const QString &propertyName = it.key();
            QScriptValue sv;
            if (overriddenTransformProperties.contains(propertyName)) {
                sv = m_engine->toScriptValue(overriddenTransformProperties.value(propertyName));
            } else {
                const QScriptProgram &myProgram = it.value();
                sv = m_engine->evaluate(myProgram);
                if (m_engine->hasUncaughtException()) {
                    CodeLocation errorLocation;
                    errorLocation.fileName = m_engine->uncaughtExceptionBacktrace().join("\n");
                    errorLocation.line = m_engine->uncaughtExceptionLineNumber();
                    throw Error(QLatin1String("transform property evaluation: ") + m_engine->uncaughtException().toString(), errorLocation);
                } else if (sv.isError()) {
                    CodeLocation errorLocation(myProgram.fileName(), myProgram.firstLineNumber());
                    throw Error(QLatin1String("transform property evaluation: ") + sv.toString(), errorLocation);
                }
            }
            m_scope.setProperty(propertyName, sv);
        }
    }

    createTransformerCommands(rule->script, transformer.data());
    if (transformer->commands.isEmpty())
        throw Error(QString("There's a rule without commands: %1.").arg(rule->toString()), rule->script->location);
}

void BuildGraph::createTransformerCommands(const RuleScript::ConstPtr &script, Transformer *transformer)
{
    QScriptProgram &scriptProgram = m_scriptProgramCache[script->script];
    if (scriptProgram.isNull())
        scriptProgram = QScriptProgram(script->script);

    m_engine->currentContext()->pushScope(m_prepareScriptScope);
    QScriptValue scriptValue = m_engine->evaluate(scriptProgram);
    m_engine->currentContext()->popScope();
    if (m_engine->hasUncaughtException())
        throw Error("evaluating prepare script: " + m_engine->uncaughtException().toString(),
                    CodeLocation(script->location.fileName,
                                 script->location.line + m_engine->uncaughtExceptionLineNumber() - 1));

    QList<AbstractCommand*> commands;
    if (scriptValue.isArray()) {
        const int count = scriptValue.property("length").toInt32();
        for (qint32 i=0; i < count; ++i) {
            QScriptValue item = scriptValue.property(i);
            if (item.isValid() && !item.isUndefined()) {
                AbstractCommand *cmd = createCommandFromScriptValue(item, script->location);
                if (cmd)
                    commands += cmd;
            }
        }
    } else {
        AbstractCommand *cmd = createCommandFromScriptValue(scriptValue, script->location);
        if (cmd)
            commands += cmd;
    }

    transformer->commands = commands;
}

QString BuildGraph::buildDirectoryRoot() const
{
    Q_ASSERT(!m_outputDirectoryRoot.isEmpty());
    QString path = FileInfo::resolvePath(m_outputDirectoryRoot, QLatin1String("build"));
    if (!path.endsWith('/'))
        path.append(QLatin1Char('/'));
    return path;
}

/*
 *  c must be built before p
 *  p ----> c
 *  p.children = c
 *  c.parents = p
 *
 * also:  children means i depend on or i am produced by
 *        parent means "produced by me" or "depends on me"
 */
void BuildGraph::connect(Artifact *p, Artifact *c)
{
    Q_ASSERT(p != c);
    p->children.insert(c);
    c->parents.insert(p);
    p->project->markDirty();
}

void BuildGraph::loggedConnect(Artifact *u, Artifact *v)
{
    Q_ASSERT(u != v);
    if (qbsLogLevel(LoggerTrace))
        qbsTrace("[BG] connect '%s' -> '%s'",
                 qPrintable(fileName(u)),
                 qPrintable(fileName(v)));
    connect(u, v);
}

static bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path)
{
    if (u == v) {
        path.append(v);
        return true;
    }

    for (ArtifactList::const_iterator it = u->children.begin(); it != u->children.end(); ++it) {
        if (findPath(*it, v, path)) {
            path.prepend(u);
            return true;
        }
    }

    return false;
}

static bool existsPath(Artifact *u, Artifact *v)
{
    if (u == v)
        return true;

    for (ArtifactList::const_iterator it = u->children.begin(); it != u->children.end(); ++it)
        if (existsPath(*it, v))
            return true;

    return false;
}

bool BuildGraph::safeConnect(Artifact *u, Artifact *v)
{
    Q_ASSERT(u != v);
    if (qbsLogLevel(LoggerTrace))
        qbsTrace("[BG] safeConnect: '%s' '%s'",
                 qPrintable(fileName(u)),
                 qPrintable(fileName(v)));

    if (existsPath(v, u)) {
        QList<Artifact *> circle;
        findPath(v, u, circle);
        qbsTrace() << "[BG] safeConnect: circle detected " << toStringList(circle);
        return false;
    }

    connect(u, v);
    return true;
}

void BuildGraph::disconnect(Artifact *u, Artifact *v)
{
    u->children.remove(v);
    v->parents.remove(u);
}

void BuildGraph::disconnectChildren(Artifact *u)
{
    foreach (Artifact *child, u->children)
        child->parents.remove(u);
    u->children.clear();
}

void BuildGraph::disconnectParents(Artifact *u)
{
    foreach (Artifact *parent, u->parents)
        parent->children.remove(u);
    u->parents.clear();
}

void BuildGraph::disconnectAll(Artifact *u)
{
    disconnectChildren(u);
    disconnectParents(u);
}

void BuildGraph::initEngine()
{
    if (m_initEngineCalls++ > 0)
        return;

    m_engine->setProperty("lastSetupProject", QVariant());
    m_engine->setProperty("lastSetupProduct", QVariant());

    m_engine->clearImportsCache();
    m_engine->pushContext();
    m_scope = m_engine->newObject();
    m_engine->currentContext()->pushScope(m_scope);
}

void BuildGraph::cleanupEngine()
{
    Q_ASSERT(m_initEngineCalls > 0);
    if (--m_initEngineCalls > 0)
        return;

    m_scope = QScriptValue();
    m_engine->currentContext()->popScope();
    m_engine->popContext();
}

void BuildGraph::remove(Artifact *artifact) const
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[BG] remove artifact " << fileName(artifact);

    removeGeneratedArtifactFromDisk(artifact);
    artifact->product->artifacts.remove(artifact);
    artifact->project->removeFromArtifactLookupTable(artifact);
    artifact->product->targetArtifacts.remove(artifact);
    foreach (Artifact *parent, artifact->parents) {
        parent->children.remove(artifact);
        if (parent->transformer) {
            parent->transformer->inputs.remove(artifact);
            m_artifactsThatMustGetNewTransformers += parent;
        }
    }
    foreach (Artifact *child, artifact->children) {
        child->parents.remove(artifact);
    }
    artifact->children.clear();
    artifact->parents.clear();
    artifact->project->markDirty();
}

/**
  * Removes the artifact and all the artifacts that depend exclusively on it.
  * Example: if you remove a cpp artifact then the obj artifact is removed but
  * not the resulting application (if there's more then one cpp artifact).
  */
void BuildGraph::removeArtifactAndExclusiveDependents(Artifact *artifact, QList<Artifact*> *removedArtifacts)
{
    if (removedArtifacts)
        removedArtifacts->append(artifact);
    foreach (Artifact *parent, artifact->parents) {
        bool removeParent = false;
        disconnect(parent, artifact);
        if (parent->children.isEmpty()) {
            removeParent = true;
        } else if (parent->transformer) {
            m_artifactsThatMustGetNewTransformers += parent;
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.isEmpty();
        }
        if (removeParent)
            removeArtifactAndExclusiveDependents(parent, removedArtifacts);
    }
    remove(artifact);
}

void BuildGraph::removeGeneratedArtifactFromDisk(Artifact *artifact)
{
    if (artifact->artifactType != Artifact::Generated)
        return;

    QFile file(artifact->filePath());
    if (!file.exists())
        return;

    qbsDebug() << "removing " << artifact->fileName();
    if (!file.remove())
        qbsWarning("Cannot remove '%s'.", qPrintable(artifact->filePath()));
}

BuildProject::Ptr BuildGraph::resolveProject(ResolvedProject::Ptr rProject)
{
    BuildProject::Ptr project = BuildProject::Ptr(new BuildProject(this));
    project->setResolvedProject(rProject);
    foreach (ResolvedProduct::Ptr rProduct, rProject->products) {
        resolveProduct(project.data(), rProduct);
    }
    detectCycle(project.data());
    return project;
}

BuildProduct::Ptr BuildGraph::resolveProduct(BuildProject *project, ResolvedProduct::Ptr rProduct)
{
    BuildProduct::Ptr product = m_productCache.value(rProduct);
    if (product)
        return product;

    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();

    product = BuildProduct::create();
    m_productCache.insert(rProduct, product);
    product->project = project;
    product->rProduct = rProduct;
    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;

    foreach (ResolvedProduct::Ptr t2, rProduct->uses) {
        if (t2 == rProduct) {
            throw Error(tr("circular using"));
        }
        BuildProduct::Ptr referencedProduct = resolveProduct(project, t2);
        product->usings.append(referencedProduct.data());
    }

    //add qbsFile artifact
    Artifact *qbsFileArtifact = product->lookupArtifact(rProduct->qbsFile);
    if (!qbsFileArtifact) {
        qbsFileArtifact = new Artifact(project);
        qbsFileArtifact->artifactType = Artifact::SourceFile;
        qbsFileArtifact->setFilePath(rProduct->qbsFile);
        qbsFileArtifact->configuration = rProduct->configuration;
        insert(product, qbsFileArtifact);
    }
    qbsFileArtifact->fileTags.insert("qbs");
    artifactsPerFileTag["qbs"].insert(qbsFileArtifact);

    // read sources
    foreach (const SourceArtifact::ConstPtr &sourceArtifact, rProduct->sources) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (product->lookupArtifact(filePath)) {
            // ignore duplicate artifacts
            continue;
        }

        Artifact *artifact = createArtifact(product, sourceArtifact);

        foreach (const QString &fileTag, artifact->fileTags)
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    // read manually added transformers
    foreach (const ResolvedTransformer::Ptr rtrafo, rProduct->transformers) {
        QList<Artifact *> inputArtifacts;
        foreach (const QString &inputFileName, rtrafo->inputs) {
            Artifact *artifact = product->lookupArtifact(inputFileName);
            if (!artifact)
                throw Error(QString("Can't find artifact '%0' in the list of source files.").arg(inputFileName));
            inputArtifacts += artifact;
        }
        Transformer::Ptr transformer = Transformer::create();
        transformer->inputs = inputArtifacts.toSet();
        const Rule::Ptr rule = Rule::create();
        rule->inputs = rtrafo->inputs;
        rule->jsImports = rtrafo->jsImports;
        ResolvedModule::Ptr module = ResolvedModule::create();
        module->name = rtrafo->module->name;
        rule->module = module;
        rule->script = rtrafo->transform;
        foreach (const SourceArtifact::ConstPtr &sourceArtifact, rtrafo->outputs) {
            Artifact *outputArtifact = createArtifact(product, sourceArtifact);
            outputArtifact->artifactType = Artifact::Generated;
            outputArtifact->transformer = transformer;
            transformer->outputs += outputArtifact;
            product->targetArtifacts += outputArtifact;
            foreach (Artifact *inputArtifact, inputArtifacts)
                safeConnect(outputArtifact, inputArtifact);
            foreach (const QString &fileTag, outputArtifact->fileTags)
                artifactsPerFileTag[fileTag].insert(outputArtifact);

            RuleArtifact::Ptr ruleArtifact = RuleArtifact::create();
            ruleArtifact->fileScript = outputArtifact->filePath();
            ruleArtifact->fileTags = outputArtifact->fileTags.toList();
            rule->artifacts += ruleArtifact;
        }
        transformer->rule = rule;
        QScriptValue oldScope = m_scope;
        m_scope = m_engine->newObject();
        m_engine->currentContext()->pushScope(m_scope);
        try {
            setupScriptEngineForProduct(m_engine, rProduct, transformer->rule, m_scope);
            transformer->setupInputs(m_engine, m_scope);
            transformer->setupOutputs(m_engine, m_scope);
            createTransformerCommands(rtrafo->transform, transformer.data());
        } catch (const Error &) {
            m_engine->currentContext()->popScope();
            m_scope = oldScope;
            throw;
        }
        m_engine->currentContext()->popScope();
        m_scope = oldScope;
        if (transformer->commands.isEmpty())
            throw Error(QString("There's a transformer without commands."), rtrafo->transform->location);
    }

    applyRules(product.data(), artifactsPerFileTag);

    QSet<Artifact *> productArtifactCandidates;
    for (int i=0; i < product->rProduct->fileTags.count(); ++i)
        foreach (Artifact *artifact, artifactsPerFileTag.value(product->rProduct->fileTags.at(i)))
            if (artifact->artifactType == Artifact::Generated)
                productArtifactCandidates += artifact;

    if (productArtifactCandidates.isEmpty()) {
        QString msg = QLatin1String("No artifacts generated for product '%1'.");
        throw Error(msg.arg(product->rProduct->name));
    }

    product->targetArtifacts += productArtifactCandidates;
    project->addBuildProduct(product);
    return product;
}

void BuildGraph::onProductChanged(BuildProduct::Ptr product, ResolvedProduct::Ptr changedProduct, bool *discardStoredProject)
{
    qbsDebug() << "[BG] product '" << product->rProduct->name << "' changed.";

    *discardStoredProject = false;
    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;
    QSet<SourceArtifact::ConstPtr> addedSourceArtifacts;
    QList<Artifact *> addedArtifacts, artifactsToRemove;
    QHash<QString, SourceArtifact::ConstPtr> oldArtifacts, newArtifacts;

    // Update list of found files by patterns
    product->rProduct->groups = changedProduct->groups;

    foreach (const SourceArtifact::ConstPtr &a, product->rProduct->sources)
        oldArtifacts.insert(a->absoluteFilePath, a);
    foreach (const SourceArtifact::Ptr &a, changedProduct->sources) {
        newArtifacts.insert(a->absoluteFilePath, a);
        if (!oldArtifacts.contains(a->absoluteFilePath)) {
            // artifact added
            qbsDebug() << "[BG] artifact '" << a->absoluteFilePath << "' added to product " << product->rProduct->name;
            product->rProduct->sources.insert(a);
            addedArtifacts += createArtifact(product, a);
            addedSourceArtifacts += a;
        }
    }
    QList<SourceArtifact::Ptr> sourceArtifactsToRemove;
    foreach (const SourceArtifact::Ptr &a, product->rProduct->sources) {
        const SourceArtifact::ConstPtr changedArtifact = newArtifacts.value(a->absoluteFilePath);
        if (!changedArtifact) {
            // artifact removed
            qbsDebug() << "[BG] artifact '" << a->absoluteFilePath << "' removed from product " << product->rProduct->name;
            Artifact *artifact = product->lookupArtifact(a->absoluteFilePath);
            Q_ASSERT(artifact);
            sourceArtifactsToRemove += a;
            removeArtifactAndExclusiveDependents(artifact, &artifactsToRemove);
            continue;
        }
        if (!addedSourceArtifacts.contains(changedArtifact)
            && changedArtifact->configuration->value() != a->configuration->value())
        {
            qbsInfo("Some properties changed. Regenerating build graph.");
            qbsDebug("Artifact with changed properties: %s", qPrintable(changedArtifact->absoluteFilePath));
            *discardStoredProject = true;
            return;
        }
        if (changedArtifact->fileTags != a->fileTags) {
            // artifact's filetags have changed
            qbsDebug() << "[BG] filetags have changed for artifact '" << a->absoluteFilePath
                       << "' from " << a->fileTags << " to " << changedArtifact->fileTags;
            Artifact *artifact = product->lookupArtifact(a->absoluteFilePath);
            Q_ASSERT(artifact);

            // handle added filetags
            foreach (const QString &addedFileTag, changedArtifact->fileTags - a->fileTags)
                artifactsPerFileTag[addedFileTag] += artifact;

            // handle removed filetags
            foreach (const QString &removedFileTag, a->fileTags - changedArtifact->fileTags) {
                artifact->fileTags -= removedFileTag;
                foreach (Artifact *parent, artifact->parents) {
                    if (parent->transformer && parent->transformer->rule->inputs.contains(removedFileTag)) {
                        // this parent has been created because of the removed filetag
                        removeArtifactAndExclusiveDependents(parent, &artifactsToRemove);
                    }
                }
            }
        }
    }

    // remove all source artifacts from the product that have been removed from the project file
    foreach (const SourceArtifact::Ptr &sourceArtifact, sourceArtifactsToRemove)
        product->rProduct->sources.remove(sourceArtifact);

    // apply rules for new artifacts
    foreach (Artifact *artifact, addedArtifacts)
        foreach (const QString &ft, artifact->fileTags)
            artifactsPerFileTag[ft] += artifact;
    applyRules(product.data(), artifactsPerFileTag);

    // parents of removed artifacts must update their transformers
    foreach (Artifact *removedArtifact, artifactsToRemove)
        foreach (Artifact *parent, removedArtifact->parents)
            m_artifactsThatMustGetNewTransformers += parent;
    updateNodesThatMustGetNewTransformer();

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, artifactsToRemove) {
        removeGeneratedArtifactFromDisk(artifact);
        delete artifact;
    }
}

void BuildGraph::updateNodesThatMustGetNewTransformer()
{
    EngineInitializer engineInitializer(this);
    foreach (Artifact *artifact, m_artifactsThatMustGetNewTransformers)
        updateNodeThatMustGetNewTransformer(artifact);
    m_artifactsThatMustGetNewTransformers.clear();
}

void BuildGraph::updateNodeThatMustGetNewTransformer(Artifact *artifact)
{
    Q_CHECK_PTR(artifact->transformer);

    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] updating transformer for " << fileName(artifact);

    removeGeneratedArtifactFromDisk(artifact);

    const Rule::ConstPtr rule = artifact->transformer->rule;
    artifact->product->project->markDirty();
    artifact->transformer = QSharedPointer<Transformer>();

    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;
    foreach (Artifact *input, artifact->children)
        foreach (const QString &fileTag, input->fileTags)
            artifactsPerFileTag[fileTag] += input;

    applyRule(artifact->product, artifactsPerFileTag, rule);
}

Artifact *BuildGraph::createArtifact(BuildProduct::Ptr product, SourceArtifact::ConstPtr sourceArtifact)
{
    Artifact *artifact = new Artifact(product->project);
    artifact->artifactType = Artifact::SourceFile;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->fileTags = sourceArtifact->fileTags;
    artifact->configuration = sourceArtifact->configuration;
    insert(product, artifact);
    return artifact;
}

QString BuildGraph::resolveOutPath(const QString &path, BuildProduct *product) const
{
    QString result;
    QString buildDir = product->rProduct->buildDirectory;
    result = FileInfo::resolvePath(buildDir, path);
    result = QDir::cleanPath(result);
    return result;
}

void Transformer::load(PersistentPool &pool, QDataStream &s)
{
    rule = pool.idLoadS<Rule>(s);
    loadContainer(inputs, s, pool);
    loadContainer(outputs, s, pool);
    int count, cmdType;
    s >> count;
    commands.reserve(count);
    while (--count >= 0) {
        s >> cmdType;
        AbstractCommand *cmd = AbstractCommand::createByType(static_cast<AbstractCommand::CommandType>(cmdType));
        cmd->load(s);
        commands += cmd;
    }
}

void Transformer::store(PersistentPool &pool, QDataStream &s) const
{
    pool.store(rule);
    storeContainer(inputs, s, pool);
    storeContainer(outputs, s, pool);
    s << commands.count();
    foreach (AbstractCommand *cmd, commands) {
        s << int(cmd->type());
        cmd->store(s);
    }
}

void BuildProduct::load(PersistentPool &pool, QDataStream &s)
{
    // artifacts
    int i;
    s >> i;
    artifacts.clear();
    for (; --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>(s);
        artifacts.insert(artifact);
        artifact->product = this;
    }

    // edges
    for (i = artifacts.count(); --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>(s);
        int k;
        s >> k;
        artifact->parents.clear();
        artifact->parents.reserve(k);
        for (; --k >= 0;)
            artifact->parents.insert(pool.idLoad<Artifact>(s));

        s >> k;
        artifact->children.clear();
        artifact->children.reserve(k);
        for (; --k >= 0;)
            artifact->children.insert(pool.idLoad<Artifact>(s));

        s >> k;
        artifact->fileDependencies.clear();
        artifact->fileDependencies.reserve(k);
        for (; --k >= 0;)
            artifact->fileDependencies.insert(pool.idLoad<Artifact>(s));

        s >> k;
        artifact->sideBySideArtifacts.clear();
        artifact->sideBySideArtifacts.reserve(k);
        for (; --k >= 0;)
            artifact->sideBySideArtifacts.insert(pool.idLoad<Artifact>(s));
    }

    // other data
    rProduct = pool.idLoadS<ResolvedProduct>(s);
    loadContainer(targetArtifacts, s, pool);

    s >> i;
    usings.clear();
    usings.reserve(i);
    for (; --i >= 0;)
        usings += pool.idLoadS<BuildProduct>(s).data();
}

void BuildProduct::store(PersistentPool &pool, QDataStream &s) const
{
    s << artifacts.count();

    //artifacts
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++)
        pool.store(*i);

    // edges
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++) {
        Artifact * artifact = *i;
        pool.store(artifact);

        s << artifact->parents.count();
        foreach (Artifact * n, artifact->parents)
            pool.store(n);
        s << artifact->children.count();
        foreach (Artifact * n, artifact->children)
            pool.store(n);
        s << artifact->fileDependencies.count();
        foreach (Artifact * n, artifact->fileDependencies)
            pool.store(n);
        s << artifact->sideBySideArtifacts.count();
        foreach (Artifact *n, artifact->sideBySideArtifacts)
            pool.store(n);
    }

    // other data
    pool.store(rProduct);
    storeContainer(targetArtifacts, s, pool);
    storeContainer(usings, s, pool);
}

BuildProject::BuildProject(BuildGraph *bg)
    : m_buildGraph(bg)
    , m_dirty(false)
{
}

BuildProject::~BuildProject()
{
    qDeleteAll(m_dependencyArtifacts);
}

void BuildProject::restoreBuildGraph(const QString &buildGraphFilePath,
                                     BuildGraph *bg,
                                     const FileTime &minTimeStamp,
                                     Configuration::Ptr cfg,
                                     const QStringList &loaderSearchPaths,
                                     LoadResult *loadResult)
{
    if (buildGraphFilePath.isNull()) {
        qbsDebug() << "[BG] No stored build graph found that's compatible to the desired build configuration.";
        return;
    }

    PersistentPool pool;
    BuildProject::Ptr project;
    qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    FileInfo bgfi(buildGraphFilePath);
    if (!bgfi.exists()) {
        qbsDebug() << "[BG] stored build graph file does not exist";
        return;
    }
    if (!pool.load(buildGraphFilePath))
        throw Error("Cannot load stored build graph.");
    project = BuildProject::Ptr(new BuildProject(bg));
    project->load(pool, pool.stream());
    project->resolvedProject()->setConfiguration(pool.headData().projectConfig);
    loadResult->loadedProject = project;
    qbsDebug() << "[BG] stored project loaded.";

    bool projectFileChanged = false;
    if (bgfi.lastModified() < minTimeStamp) {
        projectFileChanged = true;
    }

    bool referencedProductRemoved = false;
    QList<BuildProduct::Ptr> changedProducts;
    foreach (BuildProduct::Ptr product, project->buildProducts()) {
        const ResolvedProduct::ConstPtr &resolvedProduct = product->rProduct;
        FileInfo pfi(resolvedProduct->qbsFile);
        if (!pfi.exists()) {
            referencedProductRemoved = true;
        } else if (bgfi.lastModified() < pfi.lastModified()) {
            changedProducts += product;
        } else if (!resolvedProduct->groups.isEmpty()) {
            foreach (const Group::ConstPtr &group, resolvedProduct->groups) {
                QSet<QString> files = Loader::resolveFiles(group, resolvedProduct->sourceDirectory);
                if (files != group->files) {
                    changedProducts += product;
                    break;
                }
            }
        }
    }

    if (projectFileChanged || referencedProductRemoved || !changedProducts.isEmpty()) {

        Loader ldr(bg->engine());
        ldr.setSearchPaths(loaderSearchPaths);
        ProjectFile::Ptr projectFile = ldr.loadProject(project->resolvedProject()->qbsFile);
        ResolvedProject::Ptr changedProject = ldr.resolveProject(projectFile, bg->buildDirectoryRoot(), cfg);
        if (!changedProject) {
            QString msg("Trying to load '%1' failed.");
            throw Error(msg.arg(project->resolvedProject()->qbsFile));
        }
        loadResult->changedResolvedProject = changedProject;

        QMap<QString, ResolvedProduct::Ptr> changedProductsMap;
        foreach (BuildProduct::Ptr product, changedProducts) {
            if (changedProductsMap.isEmpty())
                foreach (ResolvedProduct::Ptr cp, changedProject->products)
                    changedProductsMap.insert(cp->name, cp);
            ResolvedProduct::Ptr changedProduct = changedProductsMap.value(product->rProduct->name);
            if (!changedProduct)
                continue;
            bg->onProductChanged(product, changedProduct, &loadResult->discardLoadedProject);
            if (loadResult->discardLoadedProject)
                return;
        }

        QSet<QString> oldProductNames, newProductNames;
        foreach (const BuildProduct::Ptr &product, project->buildProducts())
            oldProductNames += product->rProduct->name;
        foreach (const ResolvedProduct::ConstPtr &product, changedProject->products)
            newProductNames += product->name;
        QSet<QString> addedProductNames = newProductNames - oldProductNames;
        if (!addedProductNames.isEmpty()) {
            loadResult->discardLoadedProject = true;
            return;
        }
        QSet<QString> removedProductsNames = oldProductNames - newProductNames;
        if (!removedProductsNames.isEmpty()) {
            foreach (const BuildProduct::Ptr &product, project->buildProducts())
                if (removedProductsNames.contains(product->rProduct->name))
                    project->onProductRemoved(product);
        }

        BuildGraph::detectCycle(project.data());
    }
}

static bool isConfigCompatible(const QVariantMap &userCfg, const QVariantMap &projectCfg)
{
    QVariantMap::const_iterator it = userCfg.begin();
    for (; it != userCfg.end(); ++it) {
        if (it.value().type() == QVariant::Map) {
            if (!isConfigCompatible(it.value().toMap(), projectCfg.value(it.key()).toMap()))
                return false;
        } else {
            QVariant value = projectCfg.value(it.key());
            if (!value.isNull() && value != it.value()) {
                return false;
            }
        }
    }
    return true;
}

BuildProject::LoadResult BuildProject::load(BuildGraph *bg, const FileTime &minTimeStamp, Configuration::Ptr cfg, const QStringList &loaderSearchPaths)
{
    LoadResult result;
    result.discardLoadedProject = false;

    PersistentPool pool;
    QString fileName;
    QStringList bgFiles = storedProjectFiles(bg);
    foreach (const QString &fn, bgFiles) {
        if (!pool.load(fn))
            continue;
        PersistentPool::HeadData headData = pool.headData();
        if (isConfigCompatible(cfg->value(), headData.projectConfig)) {
            fileName = fn;
            break;
        }
    }

    restoreBuildGraph(fileName, bg, minTimeStamp, cfg, loaderSearchPaths, &result);
    return result;
}

void BuildProject::store()
{
    if (!dirty()) {
        qbsDebug() << "[BG] build graph is unchanged in project " << resolvedProject()->id() << ".";
        return;
    }
    const QString fileName = storedProjectFilePath(buildGraph(), resolvedProject()->id());
    qbsDebug() << "[BG] storing: " << fileName;
    PersistentPool pool;
    PersistentPool::HeadData headData;
    headData.projectConfig = resolvedProject()->configuration();
    pool.setHeadData(headData);
    pool.setupWriteStream(fileName);
    store(pool, pool.stream());
}

QString BuildProject::storedProjectFilePath(BuildGraph *bg, const QString &projectId)
{
    return bg->buildDirectoryRoot() + projectId + ".bg";
}

QStringList BuildProject::storedProjectFiles(BuildGraph *bg)
{
    QStringList result;
    QDirIterator dirit(bg->buildDirectoryRoot(), QStringList() << "*.bg", QDir::Files);
    while (dirit.hasNext())
        result += dirit.next();
    return result;
}

void BuildProject::load(PersistentPool &pool, QDataStream &s)
{
    m_resolvedProject = pool.idLoadS<ResolvedProject>(s);

    int count;
    s >> count;
    for (; --count >= 0;) {
        BuildProduct::Ptr product = pool.idLoadS<BuildProduct>(s);
        product->project = this;
        foreach (Artifact *artifact, product->artifacts) {
            artifact->project = this;
            insertIntoArtifactLookupTable(artifact);
        }
        addBuildProduct(product);
    }

    s >> count;
    m_dependencyArtifacts.clear();
    m_dependencyArtifacts.reserve(count);
    for (; --count >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>(s);
        artifact->project = this;
        insertFileDependency(artifact);
    }
}

void BuildProject::store(PersistentPool &pool, QDataStream &s) const
{
    pool.store(m_resolvedProject);
    storeContainer(m_buildProducts, s, pool);
    storeContainer(m_dependencyArtifacts, s, pool);
}

char **createCFileTags(const QSet<QString> &fileTags)
{
    if (fileTags.isEmpty())
        return 0;

    char **buf = new char*[fileTags.count()];
    size_t i = 0;
    foreach (const QString &fileTag, fileTags) {
        buf[i] = qstrdup(fileTag.toLocal8Bit().data());
        ++i;
    }
    return buf;
}

void freeCFileTags(char **cFileTags, int numFileTags)
{
    if (!cFileTags)
        return;
    for (int i = numFileTags; --i >= 0;)
        delete[] cFileTags[i];
    delete[] cFileTags;
}

BuildGraph * BuildProject::buildGraph() const
{
    return m_buildGraph;
}

ResolvedProject::Ptr BuildProject::resolvedProject() const
{
    return m_resolvedProject;
}

QSet<BuildProduct::Ptr> BuildProject::buildProducts() const
{
    return m_buildProducts;
}

bool BuildProject::dirty() const
{
    return m_dirty;
}

void BuildProject::insertIntoArtifactLookupTable(Artifact *artifact)
{
    QList<Artifact *> &lst = m_artifactLookupTable[artifact->fileName()][artifact->dirPath()];
    if (!lst.contains(artifact))
        lst.append(artifact);
}

void BuildProject::removeFromArtifactLookupTable(Artifact *artifact)
{
    m_artifactLookupTable[artifact->fileName()][artifact->dirPath()].removeOne(artifact);
}

QList<Artifact *> BuildProject::lookupArtifacts(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifacts(dirPath, fileName);
}

QList<Artifact *> BuildProject::lookupArtifacts(const QString &dirPath, const QString &fileName) const
{
    QList<Artifact *> result = m_artifactLookupTable.value(fileName).value(dirPath);
    return result;
}

void BuildProject::insertFileDependency(Artifact *artifact)
{
    Q_ASSERT(artifact->artifactType == Artifact::FileDependency);
    m_dependencyArtifacts += artifact;
    insertIntoArtifactLookupTable(artifact);
}

void BuildProject::onProductRemoved(const BuildProduct::Ptr &product)
{
    qbsDebug() << "[BG] product '" << product->rProduct->name << "' removed.";

    m_dirty = true;
    m_buildProducts.remove(product);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, product->artifacts) {
        BuildGraph::disconnectAll(artifact);
        BuildGraph::removeGeneratedArtifactFromDisk(artifact);
    }
}

/**
 * Copies dependencies between artifacts from the other project to this project.
 */
void BuildProject::rescueDependencies(const BuildProject::Ptr &other)
{
    QHash<QString, BuildProduct::Ptr> otherProductsByName;
    foreach (const BuildProduct::Ptr &product, other->m_buildProducts)
        otherProductsByName.insert(product->rProduct->name, product);

    foreach (const BuildProduct::Ptr &product, m_buildProducts) {
        BuildProduct::Ptr otherProduct = otherProductsByName.value(product->rProduct->name);
        if (!otherProduct)
            continue;

        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[BG] rescue dependencies of product '%s'", qPrintable(product->rProduct->name));

        foreach (Artifact *artifact, product->artifacts) {
            if (qbsLogLevel(LoggerTrace))
                qbsTrace("[BG]    artifact '%s'", qPrintable(artifact->fileName()));

            Artifact *otherArtifact = otherProduct->lookupArtifact(artifact->dirPath(), artifact->fileName());
            if (!otherArtifact || !otherArtifact->transformer)
                continue;

            foreach (Artifact *otherChild, otherArtifact->children) {
                // skip transform edges
                if (otherArtifact->transformer->inputs.contains(otherChild))
                    continue;

                QList<Artifact *> children = lookupArtifacts(otherChild->dirPath(), otherChild->fileName());
                foreach (Artifact *child, children) {
                    if (!artifact->children.contains(child))
                        BuildGraph::safeConnect(artifact, child);
                }
            }
        }
    }
}

void BuildProject::markDirty()
{
    m_dirty = true;
}

void BuildProject::addBuildProduct(const BuildProduct::Ptr &product)
{
    m_buildProducts.insert(product);
}

void BuildProject::setResolvedProject(const ResolvedProject::Ptr &resolvedProject)
{
    m_resolvedProject = resolvedProject;
}

QString fileName(Artifact *n)
{
    class BuildGraph *bg = n->project->buildGraph();
    QString str = n->filePath();
    if (str.startsWith(bg->outputDirectoryRoot()))
        str.remove(0, bg->outputDirectoryRoot().count());
    if (str.startsWith('/'))
        str.remove(0, 1);
    return str;
}

BuildGraph::EngineInitializer::EngineInitializer(BuildGraph *bg)
    : buildGraph(bg)
{
    buildGraph->initEngine();
}

BuildGraph::EngineInitializer::~EngineInitializer()
{
    buildGraph->cleanupEngine();
}

} // namespace qbs
