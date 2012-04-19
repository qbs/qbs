/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "buildgraph.h"
#include "artifact.h"
#include "command.h"
#include "rulegraph.h"
#include "transformer.h"

#include <language/loader.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/scannerpluginmanager.h>
#include <tools/logger.h>
#include <tools/scripttools.h>

#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QtCore/QDirIterator>
#include <QDataStream>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>
#include <QtScript/QScriptProgram>
#include <QtScript/QScriptValueIterator>
#include <QtCore/QCache>
#include <QtCore/QThread>

namespace qbs {

BuildProduct::BuildProduct()
    : project(0)
{
}

BuildProduct::~BuildProduct()
{
    qDeleteAll(artifacts);
}

const QList<Rule::Ptr> &BuildProduct::topSortedRules() const
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

BuildGraph::BuildGraph()
{
}

BuildGraph::~BuildGraph()
{
    qDeleteAll(m_scriptEnginePerThread.values());
}

static void internalDump(BuildProduct *product, Artifact *n, QByteArray indent)
{
    Artifact *artifactInProduct = product->lookupArtifact(n->filePath());
    if (artifactInProduct && artifactInProduct != n) {
        fprintf(stderr,"\ntree corrupted. %p ('%s') resolves to %p ('%s')\n",
                n,  qPrintable(n->filePath()), product->lookupArtifact(n->filePath()),
                qPrintable(product->lookupArtifact(n->filePath())->filePath()));

        abort();
    }
    printf("%s", indent.constData());
    printf("Artifact (%p) ", n);
    printf("%s%s %s [%s]",
           qPrintable(QString(toString(n->buildState).at(0))),
           artifactInProduct ? "" : " SBS",     // SBS == side-by-side artifact from other product
           qPrintable(n->filePath()),
           qPrintable(QStringList(n->fileTags.toList()).join(",")));
    printf("\n");
    indent.append("  ");
    foreach (Artifact *child, n->children) {
        internalDump(product, child, indent);
    }
}

void BuildGraph::dump(BuildProduct::Ptr product) const
{
    foreach (Artifact *n, product->artifacts)
        if (n->parents.isEmpty())
            internalDump(product.data(), n, QByteArray());
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

void BuildGraph::setupScriptEngineForProduct(QScriptEngine *scriptEngine, ResolvedProduct::Ptr product, Rule::Ptr rule, BuildGraph *bg)
{
    ResolvedProject *lastSetupProject = (ResolvedProject *)scriptEngine->property("lastSetupProject").toULongLong();
    ResolvedProduct *lastSetupProduct = (ResolvedProduct *)scriptEngine->property("lastSetupProduct").toULongLong();

    if (lastSetupProject != product->project) {
        scriptEngine->setProperty("lastSetupProject", QVariant((qulonglong)product->project));
        QScriptValue projectScriptValue;
        projectScriptValue = scriptEngine->newObject();
        projectScriptValue.setProperty("filePath", product->project->qbsFile);
        projectScriptValue.setProperty("path", FileInfo::path(product->project->qbsFile));
        scriptEngine->globalObject().setProperty("project", projectScriptValue, QScriptValue::ReadOnly);
    }

    QScriptValue productScriptValue;
    if (lastSetupProduct != product.data()) {
        scriptEngine->setProperty("lastSetupProduct", QVariant((qulonglong)product.data()));
        productScriptValue = scriptEngine->toScriptValue(product->configuration->value());
        productScriptValue.setProperty("name", product->name);
        QString destinationDirectory = product->destinationDirectory;
        if (destinationDirectory.isEmpty())
            destinationDirectory = ".";
        productScriptValue.setProperty("destinationDirectory", destinationDirectory);
        scriptEngine->globalObject().setProperty("product", productScriptValue, QScriptValue::ReadOnly);
    } else {
        productScriptValue = scriptEngine->globalObject().property("product");
    }

    // If the Rule is in a Module, set up the 'module' property
    if (!rule->module->name.isEmpty())
        productScriptValue.setProperty("module", productScriptValue.property("modules").property(rule->module->name));

    if (rule) {
        for (JsImports::const_iterator it = rule->jsImports.begin(); it != rule->jsImports.end(); ++it) {
            foreach (const QString &fileName, it.value()) {
                QScriptValue jsImportValue;
                if (bg)
                    jsImportValue = bg->m_jsImportCache.value(fileName, scriptEngine->undefinedValue());
                if (jsImportValue.isUndefined()) {
//                    qDebug() << "CACHE MISS" << fileName;
                    QFile file(fileName);
                    if (!file.open(QFile::ReadOnly))
                        throw Error(QString("Cannot open '%1'.").arg(fileName));
                    const QString sourceCode = QTextStream(&file).readAll();
                    QScriptProgram program(sourceCode, fileName);
                    addJSImport(scriptEngine, program, jsImportValue);
                    addJSImport(scriptEngine, jsImportValue, it.key());
                    if (bg)
                        bg->m_jsImportCache.insert(fileName, jsImportValue);
                } else {
//                    qDebug() << "CACHE HIT" << fileName;
                    addJSImport(scriptEngine, jsImportValue, it.key());
                }
            }
        }
    } else {
        // ### TODO remove the imports we added before
    }
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
        QDir buildDir(product->project->buildGraph()->buildDirectoryRoot() + product->project->resolvedProject()->id);
        basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
    }

    QScriptValue modulesScriptValue = artifact->configuration->cachedScriptValue(scriptEngine());
    if (!modulesScriptValue.isValid()) {
        modulesScriptValue = scriptEngine()->toScriptValue(artifact->configuration->value());
        artifact->configuration->cacheScriptValue(scriptEngine(), modulesScriptValue);
    }
    modulesScriptValue = modulesScriptValue.property("modules");

    // expose per file properties we want to use in an Artifact within a Rule
    QScriptValue scriptValue = scriptEngine()->newObject();
    scriptValue.setProperty("fileName", inFileName);
    scriptValue.setProperty("baseName", inBaseName);
    scriptValue.setProperty("completeBaseName", inCompleteBaseName);
    scriptValue.setProperty("baseDir", basedir);
    scriptValue.setProperty("modules", modulesScriptValue);

    QScriptValue globalObj = scriptEngine()->globalObject();
    globalObj.setProperty("input", scriptValue);
}

void BuildGraph::applyRules(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag)
{
    foreach (Rule::Ptr rule, product->topSortedRules())
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
        qbsTrace() << "[BG] running cycle detection on project '" + project->resolvedProject()->id + "'";
    }

    foreach (BuildProduct::Ptr product, project->buildProducts())
        foreach (Artifact *artifact, product->targetArtifacts)
            detectCycle(artifact);

    if (qbsLogLevel(LoggerTrace)) {
        qint64 elapsed = t->elapsed();
        qbsTrace() << "[BG] cycle detection for project '" + project->resolvedProject()->id + "' took " << elapsed << " ms";
        delete t;
    }
}

void BuildGraph::detectCycle(Artifact *a)
{
    QSet<Artifact *> done, currentBranch;
    detectCycle(a, done, currentBranch);
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
                           Rule::Ptr rule)
{
    setupScriptEngineForProduct(scriptEngine(), product->rProduct, rule, this);

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
        const Rule::Ptr &rule, const RuleArtifact::Ptr &ruleArtifact,
        const QSet<Artifact *> &inputArtifacts,
        QList< QPair<RuleArtifact*, Artifact *> > *ruleArtifactArtifactMap,
        QList<Artifact *> *outputArtifacts,
        QSharedPointer<Transformer> &transformer)
{
    QScriptValue scriptValue = scriptEngine()->evaluate(ruleArtifact->fileScript);
    if (scriptValue.isError() || scriptEngine()->hasUncaughtException())
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
    ruleArtifactArtifactMap->append(qMakePair(ruleArtifact.data(), outputArtifact));
    outputArtifacts->append(outputArtifact);

    // create transformer if not already done so
    if (!transformer) {
        transformer = QSharedPointer<Transformer>(new Transformer);
        transformer->rule = rule;
        transformer->inputs = inputArtifacts;
    }
    outputArtifact->transformer = transformer;
}

void BuildGraph::applyRule(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag, Rule::Ptr rule, const QSet<Artifact *> &inputArtifacts)
{
    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] apply rule " << rule->toString() << " " << toStringList(inputArtifacts).join(",\n            ");

    QList< QPair<RuleArtifact*, Artifact *> > ruleArtifactArtifactMap;
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
    foreach (RuleArtifact::Ptr ruleArtifact, rule->artifacts) {
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

    transformer->setupInputs(scriptEngine(), scriptEngine()->globalObject());

    // change the transformer outputs according to the bindings in Artifact
    QScriptValue scriptValue;
    for (int i=ruleArtifactArtifactMap.count(); --i >= 0;) {
        RuleArtifact *ra = ruleArtifactArtifactMap.at(i).first;
        if (ra->bindings.isEmpty())
            continue;

        // expose attributes of this artifact
        Artifact *outputArtifact = ruleArtifactArtifactMap.at(i).second;
        outputArtifact->configuration = Configuration::Ptr(new Configuration(*outputArtifact->configuration));

        // ### clean scriptEngine() first?
        scriptEngine()->globalObject().setProperty("fileName", scriptEngine()->toScriptValue(outputArtifact->filePath()), QScriptValue::ReadOnly);
        scriptEngine()->globalObject().setProperty("fileTags", toScriptValue(scriptEngine(), outputArtifact->fileTags), QScriptValue::ReadOnly);

        QVariantMap artifactModulesCfg = outputArtifact->configuration->value().value("modules").toMap();
        for (int i=0; i < ra->bindings.count(); ++i) {
            const RuleArtifact::Binding &binding = ra->bindings.at(i);
            scriptValue = scriptEngine()->evaluate(binding.code);
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

    transformer->setupOutputs(scriptEngine(), scriptEngine()->globalObject());

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
                sv = scriptEngine()->toScriptValue(overriddenTransformProperties.value(propertyName));
            } else {
                const QScriptProgram &myProgram = it.value();
                sv = scriptEngine()->evaluate(myProgram);
                if (scriptEngine()->hasUncaughtException()) {
                    CodeLocation errorLocation;
                    errorLocation.fileName = scriptEngine()->uncaughtExceptionBacktrace().join("\n");
                    errorLocation.line = scriptEngine()->uncaughtExceptionLineNumber();
                    throw Error(QLatin1String("transform property evaluation: ") + scriptEngine()->uncaughtException().toString(), errorLocation);
                } else if (sv.isError()) {
                    CodeLocation errorLocation(myProgram.fileName(), myProgram.firstLineNumber());
                    throw Error(QLatin1String("transform property evaluation: ") + sv.toString(), errorLocation);
                }
            }
            scriptEngine()->globalObject().setProperty(propertyName, sv);
        }
    }

    createTransformerCommands(rule->script, transformer.data());
    if (transformer->commands.isEmpty())
        throw Error(QString("There's a rule without commands: %1.").arg(rule->toString()), rule->script->location);
}

void BuildGraph::createTransformerCommands(RuleScript::Ptr script, Transformer *transformer)
{
    QScriptProgram &scriptProgram = m_scriptProgramCache[script->script];
    if (scriptProgram.isNull())
        scriptProgram = QScriptProgram(script->script);

    QScriptValue scriptValue = scriptEngine()->evaluate(scriptProgram);
    if (scriptEngine()->hasUncaughtException())
        throw Error("evaluating prepare script: " + scriptEngine()->uncaughtException().toString(),
                    script->location);

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

void BuildGraph::remove(Artifact *artifact) const
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[BG] remove artifact " << fileName(artifact);

    if (artifact->artifactType == Artifact::Generated)
        QFile::remove(artifact->filePath());
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
        if (parent->children.count() == 1)
            removeArtifactAndExclusiveDependents(parent, removedArtifacts);
    }
    remove(artifact);
}

BuildProject::Ptr BuildGraph::resolveProject(ResolvedProject::Ptr rProject, QFutureInterface<bool> &futureInterface)
{
    BuildProject::Ptr project = BuildProject::Ptr(new BuildProject(this));
    project->setResolvedProject(rProject);
    foreach (ResolvedProduct::Ptr rProduct, rProject->products) {
        resolveProduct(project.data(), rProduct, futureInterface);
    }
    detectCycle(project.data());
    return project;
}

BuildProduct::Ptr BuildGraph::resolveProduct(BuildProject *project, ResolvedProduct::Ptr rProduct, QFutureInterface<bool> &futureInterface)
{
    BuildProduct::Ptr product = m_productCache.value(rProduct);
    if (product)
        return product;

    futureInterface.setProgressValue(futureInterface.progressValue() + 1);
    product = BuildProduct::Ptr(new BuildProduct);
    m_productCache.insert(rProduct, product);
    product->project = project;
    product->rProduct = rProduct;
    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;

    foreach (ResolvedProduct::Ptr t2, rProduct->uses) {
        if (t2 == rProduct) {
            throw Error(tr("circular using"));
        }
        BuildProduct::Ptr referencedProduct = resolveProduct(project, t2, futureInterface);
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
    foreach (SourceArtifact::Ptr sourceArtifact, rProduct->sources) {
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
    QList<Artifact *> transformerOutputs;
    foreach (const ResolvedTransformer::Ptr rtrafo, rProduct->transformers) {
        QList<Artifact *> inputArtifacts;
        foreach (const QString &inputFileName, rtrafo->inputs) {
            Artifact *artifact = product->lookupArtifact(inputFileName);
            if (!artifact)
                throw Error(QString("Can't find artifact '%0' in the list of source files.").arg(inputFileName));
            inputArtifacts += artifact;
        }
        QSharedPointer<Transformer> transformer(new Transformer);
        transformer->inputs = inputArtifacts.toSet();
        transformer->rule = Rule::Ptr(new Rule);
        transformer->rule->inputs = rtrafo->inputs;
        transformer->rule->jsImports = rtrafo->jsImports;
        transformer->rule->module = ResolvedModule::Ptr(new ResolvedModule);
        transformer->rule->module->name = rtrafo->module->name;
        transformer->rule->script = rtrafo->transform;
        foreach (SourceArtifact::Ptr sourceArtifact, rtrafo->outputs) {
            Artifact *outputArtifact = createArtifact(product, sourceArtifact);
            outputArtifact->artifactType = Artifact::Generated;
            outputArtifact->transformer = transformer;
            transformer->outputs += outputArtifact;
            transformerOutputs += outputArtifact;
            foreach (Artifact *inputArtifact, inputArtifacts)
                safeConnect(outputArtifact, inputArtifact);
            foreach (const QString &fileTag, outputArtifact->fileTags)
                artifactsPerFileTag[fileTag].insert(outputArtifact);

            RuleArtifact::Ptr ruleArtifact(new RuleArtifact);
            ruleArtifact->fileScript = outputArtifact->filePath();
            ruleArtifact->fileTags = outputArtifact->fileTags.toList();
            transformer->rule->artifacts += ruleArtifact;
        }
        setupScriptEngineForProduct(scriptEngine(), rProduct, transformer->rule, this);
        transformer->setupInputs(scriptEngine(), scriptEngine()->globalObject());
        transformer->setupOutputs(scriptEngine(), scriptEngine()->globalObject());
        createTransformerCommands(rtrafo->transform, transformer.data());
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

    foreach (Artifact *productArtifact, productArtifactCandidates) {
        product->targetArtifacts.insert(productArtifact);
        project->addBuildProduct(product);

        foreach (Artifact *trafoOutputArtifact, transformerOutputs)
            if (productArtifact != trafoOutputArtifact)
                loggedConnect(productArtifact, trafoOutputArtifact);
    }

    return product;
}

void BuildGraph::onProductChanged(BuildProduct::Ptr product, ResolvedProduct::Ptr changedProduct, bool *discardStoredProject)
{
    qbsDebug() << "[BG] product '" << product->rProduct->name << "' changed.";

    *discardStoredProject = false;
    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;
    QSet<SourceArtifact::Ptr> addedSourceArtifacts;
    QList<Artifact *> addedArtifacts, artifactsToRemove;
    QHash<QString, SourceArtifact::Ptr> oldArtifacts, newArtifacts;
    foreach (SourceArtifact::Ptr a, product->rProduct->sources)
        oldArtifacts.insert(a->absoluteFilePath, a);
    foreach (SourceArtifact::Ptr a, changedProduct->sources) {
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
    foreach (SourceArtifact::Ptr a, product->rProduct->sources) {
        SourceArtifact::Ptr changedArtifact = newArtifacts.value(a->absoluteFilePath);
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
    foreach (SourceArtifact::Ptr sourceArtifact, sourceArtifactsToRemove)
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
        if (artifact->artifactType == Artifact::Generated) {
            qbsDebug() << "[BG] deleting stale artifact " << artifact->filePath();
            QFile::remove(artifact->filePath());
        }
        delete artifact;
    }
}

void BuildGraph::updateNodesThatMustGetNewTransformer()
{
    foreach (Artifact *artifact, m_artifactsThatMustGetNewTransformers)
        updateNodeThatMustGetNewTransformer(artifact);
    m_artifactsThatMustGetNewTransformers.clear();
}

void BuildGraph::updateNodeThatMustGetNewTransformer(Artifact *artifact)
{
    Q_CHECK_PTR(artifact->transformer);

    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] updating transformer for " << fileName(artifact);

    QFile::remove(artifact->filePath());

    Rule::Ptr rule = artifact->transformer->rule;
    artifact->product->project->markDirty();
    artifact->transformer = QSharedPointer<Transformer>();

    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;
    foreach (Artifact *input, artifact->children)
        foreach (const QString &fileTag, input->fileTags)
            artifactsPerFileTag[fileTag] += input;

    applyRule(artifact->product, artifactsPerFileTag, rule);
}

QScriptEngine *BuildGraph::scriptEngine()
{
    QThread *const currentThread = QThread::currentThread();
    QScriptEngine *engine = m_scriptEnginePerThread.value(currentThread);
    if (!engine) {
        engine = new QScriptEngine;
        m_scriptEnginePerThread.insert(currentThread, engine);

        ProcessCommand::setupForJavaScript(engine);
        JavaScriptCommand::setupForJavaScript(engine);
    }
    return engine;
}

Artifact *BuildGraph::createArtifact(BuildProduct::Ptr product, SourceArtifact::Ptr sourceArtifact)
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

void Transformer::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
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

void Transformer::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << pool.store(rule);
    storeContainer(inputs, s, pool);
    storeContainer(outputs, s, pool);
    s << commands.count();
    foreach (AbstractCommand *cmd, commands) {
        s << int(cmd->type());
        cmd->store(s);
    }
}

void BuildProduct::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);

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

void BuildProduct::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << artifacts.count();

    //artifacts
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++) {
        PersistentObjectId artifactId = pool.store(*i);
        s << artifactId;
    }

    // edges
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++) {
        Artifact * artifact = *i;
        s << pool.store(artifact);

        s << artifact->parents.count();
        foreach (Artifact * n, artifact->parents)
            s << pool.store(n);
        s << artifact->children.count();
        foreach (Artifact * n, artifact->children)
            s << pool.store(n);
        s << artifact->fileDependencies.count();
        foreach (Artifact * n, artifact->fileDependencies)
            s << pool.store(n);
        s << artifact->sideBySideArtifacts.count();
        foreach (Artifact *n, artifact->sideBySideArtifacts)
            s << pool.store(n);
    }

    // other data
    s << pool.store(rProduct);
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

BuildProject::Ptr BuildProject::restoreBuildGraph(const QString &buildGraphFilePath,
                                                         BuildGraph *bg,
                                                         const FileTime &minTimeStamp,
                                                         Configuration::Ptr cfg,
                                                         const QStringList &loaderSearchPaths)
{
    if (buildGraphFilePath.isNull()) {
        qbsDebug() << "[BG] No stored build graph found that's compatible to the desired build configuration.";
        return BuildProject::Ptr();
    }

    PersistentPool pool;
    BuildProject::Ptr project;
    qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    FileInfo bgfi(buildGraphFilePath);
    if (!bgfi.exists()) {
        qbsDebug() << "[BG] stored build graph file does not exist";
        return project;
    }
    if (!pool.load(buildGraphFilePath))
        throw Error("Cannot load stored build graph.");
    project = BuildProject::Ptr(new BuildProject(bg));
    PersistentObjectData data = pool.getData(0);
    project->load(pool, data);
    project->resolvedProject()->configuration = Configuration::Ptr(new Configuration);
    project->resolvedProject()->configuration->setValue(pool.headData().projectConfig);
    qbsDebug() << "[BG] stored project loaded.";

    bool projectFileChanged = false;
    if (bgfi.lastModified() < minTimeStamp) {
        projectFileChanged = true;
    }

    QList<BuildProduct::Ptr> changedProducts;
    foreach (BuildProduct::Ptr product, project->buildProducts()) {
        FileInfo pfi(product->rProduct->qbsFile);
        if (!pfi.exists())
            throw Error(QString("The product file '%1' is gone.").arg(product->rProduct->qbsFile));
        if (bgfi.lastModified() < pfi.lastModified())
            changedProducts += product;
    }

    if (projectFileChanged || !changedProducts.isEmpty()) {

        Loader ldr;
        ldr.setSearchPaths(loaderSearchPaths);
        ldr.loadProject(project->resolvedProject()->qbsFile);
        QFutureInterface<bool> dummyFutureInterface;
        ResolvedProject::Ptr changedProject = ldr.resolveProject(bg->buildDirectoryRoot(), cfg, dummyFutureInterface);
        if (!changedProject) {
            QString msg("Trying to load '%1' failed.");
            throw Error(msg.arg(project->resolvedProject()->qbsFile));
        }

        bool discardStoredProject;
        QMap<QString, ResolvedProduct::Ptr> changedProductsMap;
        foreach (BuildProduct::Ptr product, changedProducts) {
            if (changedProductsMap.isEmpty())
                foreach (ResolvedProduct::Ptr cp, changedProject->products)
                    changedProductsMap.insert(cp->name, cp);
            bg->onProductChanged(product, changedProductsMap.value(product->rProduct->name), &discardStoredProject);
            if (discardStoredProject)
                return BuildProject::Ptr();
        }

        BuildGraph::detectCycle(project.data());
    }

    return project;
}

BuildProject::Ptr BuildProject::loadBuildGraph(const QString &buildGraphFilePath,
                                               BuildGraph *buildGraph,
                                               const FileTime &minTimeStamp,
                                               const QStringList &loaderSearchPaths)
{
    static const qbs::Configuration::Ptr configuration(new qbs::Configuration);
    return restoreBuildGraph(buildGraphFilePath, buildGraph, minTimeStamp, configuration, loaderSearchPaths);
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

BuildProject::Ptr BuildProject::load(BuildGraph *bg, const FileTime &minTimeStamp, Configuration::Ptr cfg, const QStringList &loaderSearchPaths)
{
    PersistentPool pool;
    QString fileName;
    QStringList bgFiles = storedProjectFiles(bg);
    foreach (const QString &fn, bgFiles) {
        if (!pool.load(fn, PersistentPool::LoadHeadData))
            continue;
        PersistentPool::HeadData headData = pool.headData();
        if (isConfigCompatible(cfg->value(), headData.projectConfig)) {
            fileName = fn;
            break;
        }
    }


    return restoreBuildGraph(fileName, bg, minTimeStamp, cfg, loaderSearchPaths);
}

void BuildProject::store()
{
    if (!dirty()) {
        qbsDebug() << "[BG] build graph is unchanged in project " << resolvedProject()->id << ".";
        return;
    }
    const QString fileName = storedProjectFilePath(buildGraph(), resolvedProject()->id);
    qbsDebug() << "[BG] storing: " << fileName;
    PersistentPool pool;
    PersistentPool::HeadData headData;
    headData.projectConfig = resolvedProject()->configuration->value();
    pool.setHeadData(headData);
    PersistentObjectData data;
    store(pool, data);
    pool.setData(0, data);
    pool.store(fileName);
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

void BuildProject::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
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

void BuildProject::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);

    s << pool.store(m_resolvedProject);
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

} // namespace qbs
