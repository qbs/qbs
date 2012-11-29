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
#include "cycledetector.h"
#include "command.h"
#include "rulegraph.h"
#include "transformer.h"

#include <language/language.h>
#include <language/loader.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/scannerpluginmanager.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>

#include <QCache>
#include <QDir>
#include <QDirIterator>
#include <QDataStream>
#include <QFileInfo>
#include <QMutex>
#include <QScriptProgram>
#include <QScriptValueIterator>
#include <QThread>

namespace qbs {
namespace Internal {

BuildProduct::BuildProduct()
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

const QList<RuleConstPtr> &BuildProduct::topSortedRules() const
{
    if (m_topSortedRules.isEmpty()) {
        QStringList fileTags;
        fileTags << rProduct->fileTags << rProduct->additionalFileTags;
        fileTags.sort();
        std::unique(fileTags.begin(), fileTags.end());
        RuleGraph ruleGraph;
        ruleGraph.build(rProduct->rules, fileTags);
//        ruleGraph.dump();
        m_topSortedRules = ruleGraph.topSorted();
//        int i=0;
//        foreach (RulePtr r, m_topSortedRules)
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

Artifact *BuildProduct::createArtifact(const SourceArtifactConstPtr &sourceArtifact)
{
    Artifact *artifact = new Artifact(project);
    artifact->artifactType = Artifact::SourceFile;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->fileTags = sourceArtifact->fileTags;
    artifact->properties = sourceArtifact->properties;
    insertArtifact(artifact);
    return artifact;
}

void BuildProduct::insertArtifact(Artifact *artifact)
{
    Q_ASSERT(!artifact->product);
    Q_ASSERT(!artifact->filePath().isEmpty());
    Q_ASSERT(!artifacts.contains(artifact));
#ifdef QT_DEBUG
    foreach (const BuildProductPtr &otherProduct, project->buildProducts()) {
        if (otherProduct->lookupArtifact(artifact->filePath())) {
            if (artifact->artifactType == Artifact::Generated) {
                QString pl;
                pl.append(QString("  - %1 \n").arg(rProduct->name));
                foreach (const BuildProductPtr &p, project->buildProducts()) {
                    if (p->lookupArtifact(artifact->filePath()))
                        pl.append(QString("  - %1 \n").arg(p->rProduct->name));
                }
                throw Error(QString ("BUG: already inserted in this project: %1\n%2")
                            .arg(artifact->filePath()).arg(pl));
            }
        }
    }
#endif
    artifacts.insert(artifact);
    artifact->product = this;
    project->insertIntoArtifactLookupTable(artifact);
    project->markDirty();

    if (qbsLogLevel(LoggerTrace))
        qbsTrace("[BG] insert artifact '%s'", qPrintable(artifact->filePath()));
}

void BuildProduct::applyRules(ArtifactsPerFileTagMap &artifactsPerFileTag, ScriptEngine *engine,
                              ProgressObserver *observer)
{
    BuildGraph::EngineInitializer engineInitializer(project->buildGraph());
    RulesApplicator rulesApplier(this, artifactsPerFileTag, engine, observer);
    rulesApplier.applyAllRules();
}


BuildGraph::BuildGraph()
    : m_progressObserver(0)
    , m_engine(0)
    , m_initEngineCalls(0)
{
}

BuildGraph::~BuildGraph()
{
}

void BuildGraph::setEngine(ScriptEngine *engine)
{
    m_engine = engine;
    m_prepareScriptScope = m_engine->newObject();
    ProcessCommand::setupForJavaScript(m_prepareScriptScope);
    JavaScriptCommand::setupForJavaScript(m_prepareScriptScope);
}

void BuildGraph::setupScriptEngineForProduct(ScriptEngine *engine,
                                             const ResolvedProductConstPtr &product,
                                             RuleConstPtr rule,
                                             QScriptValue targetObject)
{
    const ResolvedProject *lastSetupProject = reinterpret_cast<ResolvedProject *>(engine->property("lastSetupProject").toULongLong());
    const ResolvedProduct *lastSetupProduct = reinterpret_cast<ResolvedProduct *>(engine->property("lastSetupProduct").toULongLong());

    if (lastSetupProject != product->project) {
        engine->setProperty("lastSetupProject",
                QVariant(reinterpret_cast<qulonglong>(product->project.data())));
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
        productScriptValue = product->properties->toScriptValue(engine);
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

void BuildGraph::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

bool BuildGraph::findPath(Artifact *u, Artifact *v, QList<Artifact*> &path)
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
    if (qbsLogLevel(LoggerTrace))
        qbsTrace("[BG] disconnect: '%s' '%s'", qPrintable(fileName(u)), qPrintable(fileName(v)));
    u->children.remove(v);
    v->parents.remove(u);
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
    m_scope.setPrototype(m_prepareScriptScope);
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

void Transformer::load(PersistentPool &pool)
{
    rule = pool.idLoadS<Rule>();
    pool.loadContainer(inputs);
    pool.loadContainer(outputs);
    int count, cmdType;
    pool.stream() >> count;
    commands.reserve(count);
    while (--count >= 0) {
        pool.stream() >> cmdType;
        AbstractCommand *cmd = AbstractCommand::createByType(static_cast<AbstractCommand::CommandType>(cmdType));
        cmd->load(pool.stream());
        commands += cmd;
    }
}

void Transformer::store(PersistentPool &pool) const
{
    pool.store(rule);
    pool.storeContainer(inputs);
    pool.storeContainer(outputs);
    pool.stream() << commands.count();
    foreach (AbstractCommand *cmd, commands) {
        pool.stream() << int(cmd->type());
        cmd->store(pool.stream());
    }
}

void BuildProduct::load(PersistentPool &pool)
{
    // artifacts
    int i;
    pool.stream() >> i;
    artifacts.clear();
    for (; --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        artifacts.insert(artifact);
        artifact->product = this;
    }

    // edges
    for (i = artifacts.count(); --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        int k;
        pool.stream() >> k;
        artifact->parents.clear();
        artifact->parents.reserve(k);
        for (; --k >= 0;)
            artifact->parents.insert(pool.idLoad<Artifact>());

        pool.stream() >> k;
        artifact->children.clear();
        artifact->children.reserve(k);
        for (; --k >= 0;)
            artifact->children.insert(pool.idLoad<Artifact>());

        pool.stream() >> k;
        artifact->fileDependencies.clear();
        artifact->fileDependencies.reserve(k);
        for (; --k >= 0;)
            artifact->fileDependencies.insert(pool.idLoad<Artifact>());
    }

    // other data
    rProduct = pool.idLoadS<ResolvedProduct>();
    pool.loadContainer(targetArtifacts);

    pool.stream() >> i;
    dependencies.clear();
    dependencies.reserve(i);
    for (; --i >= 0;)
        dependencies += pool.idLoadS<BuildProduct>();
}

void BuildProduct::store(PersistentPool &pool) const
{
    pool.stream() << artifacts.count();

    //artifacts
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++)
        pool.store(*i);

    // edges
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++) {
        Artifact * artifact = *i;
        pool.store(artifact);

        pool.stream() << artifact->parents.count();
        foreach (Artifact * n, artifact->parents)
            pool.store(n);
        pool.stream() << artifact->children.count();
        foreach (Artifact * n, artifact->children)
            pool.store(n);
        pool.stream() << artifact->fileDependencies.count();
        foreach (Artifact * n, artifact->fileDependencies)
            pool.store(n);
    }

    // other data
    pool.store(rProduct);
    pool.storeContainer(targetArtifacts);
    pool.storeContainer(dependencies);
}

BuildProject::BuildProject(BuildGraph *bg)
    : m_buildGraph(bg)
    , m_dirty(false)
{
}

BuildProject::~BuildProject()
{
    delete m_buildGraph;
    qDeleteAll(m_dependencyArtifacts);
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

void BuildProject::store() const
{
    if (!dirty()) {
        qbsDebug() << "[BG] build graph is unchanged in project " << resolvedProject()->id() << ".";
        return;
    }
    const QString fileName = buildGraphFilePath();
    qbsDebug() << "[BG] storing: " << fileName;
    PersistentPool pool;
    PersistentPool::HeadData headData;
    headData.projectConfig = resolvedProject()->buildConfiguration();
    pool.setHeadData(headData);
    pool.setupWriteStream(fileName);
    store(pool);
    m_dirty = false;
}

QString BuildProject::deriveBuildGraphFilePath(const QString &buildDir, const QString projectId)
{
    return buildDir + QLatin1Char('/') + projectId + QLatin1String(".bg");
}

QString BuildProject::buildGraphFilePath() const
{
    return deriveBuildGraphFilePath(resolvedProject()->buildDirectory, resolvedProject()->id());
}

void BuildProject::load(PersistentPool &pool)
{
    TimedActivityLogger logger(QLatin1String("Loading project from disk."));
    m_resolvedProject = pool.idLoadS<ResolvedProject>();
    foreach (const ResolvedProductPtr &product, m_resolvedProject->products)
        product->project = m_resolvedProject;

    int count;
    pool.stream() >> count;
    for (; --count >= 0;) {
        BuildProductPtr product = pool.idLoadS<BuildProduct>();
        foreach (Artifact *artifact, product->artifacts) {
            artifact->project = this;
            insertIntoArtifactLookupTable(artifact);
        }
        addBuildProduct(product);
    }

    pool.stream() >> count;
    m_dependencyArtifacts.clear();
    m_dependencyArtifacts.reserve(count);
    for (; --count >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        artifact->project = this;
        insertFileDependency(artifact);
    }
}

void BuildProject::store(PersistentPool &pool) const
{
    pool.store(m_resolvedProject);
    pool.storeContainer(m_buildProducts);
    pool.storeContainer(m_dependencyArtifacts);
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

ResolvedProjectPtr BuildProject::resolvedProject() const
{
    return m_resolvedProject;
}

QSet<BuildProductPtr> BuildProject::buildProducts() const
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

/**
 * Copies dependencies between artifacts from the other project to this project.
 */
void BuildProject::rescueDependencies(const BuildProjectPtr &other)
{
    QHash<QString, BuildProductPtr> otherProductsByName;
    foreach (const BuildProductPtr &product, other->m_buildProducts)
        otherProductsByName.insert(product->rProduct->name, product);

    foreach (const BuildProductPtr &product, m_buildProducts) {
        BuildProductPtr otherProduct = otherProductsByName.value(product->rProduct->name);
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

void BuildProject::addBuildProduct(const BuildProductPtr &product)
{
    m_buildProducts.insert(product);
}

void BuildProject::setResolvedProject(const ResolvedProjectPtr &resolvedProject)
{
    m_resolvedProject = resolvedProject;
}

void BuildProject::removeArtifact(Artifact *artifact)
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[BG] remove artifact " << fileName(artifact);

    BuildGraph::removeGeneratedArtifactFromDisk(artifact);
    artifact->product->artifacts.remove(artifact);
    removeFromArtifactLookupTable(artifact);
    artifact->product->targetArtifacts.remove(artifact);
    foreach (Artifact *parent, artifact->parents) {
        parent->children.remove(artifact);
        if (parent->transformer) {
            parent->transformer->inputs.remove(artifact);
            m_artifactsThatMustGetNewTransformers += parent;
        }
    }
    foreach (Artifact *child, artifact->children)
        child->parents.remove(artifact);
    artifact->children.clear();
    artifact->parents.clear();
    markDirty();
}

void BuildProject::updateNodesThatMustGetNewTransformer(ScriptEngine *engine,
                                                        ProgressObserver *observer)
{
    BuildGraph::EngineInitializer engineInitializer(buildGraph());
    foreach (Artifact *artifact, m_artifactsThatMustGetNewTransformers)
        updateNodeThatMustGetNewTransformer(artifact, engine, observer);
    m_artifactsThatMustGetNewTransformers.clear();
}

void BuildProject::updateNodeThatMustGetNewTransformer(Artifact *artifact, ScriptEngine *engine,
                                                       ProgressObserver *observer)
{
    Q_ASSERT(artifact->transformer);

    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] updating transformer for " << fileName(artifact);

    BuildGraph::removeGeneratedArtifactFromDisk(artifact);

    const RuleConstPtr rule = artifact->transformer->rule;
    markDirty();
    artifact->transformer = TransformerPtr();

    ArtifactsPerFileTagMap artifactsPerFileTag;
    foreach (Artifact *input, artifact->children) {
        foreach (const QString &fileTag, input->fileTags)
            artifactsPerFileTag[fileTag] += input;
    }
    RulesApplicator rulesApplier(artifact->product, artifactsPerFileTag, engine, observer);
    rulesApplier.applyRule(rule);
}


QString fileName(Artifact *n)
{
    const QString &buildDir = n->project->resolvedProject()->buildDirectory;
    QString str = n->filePath();
    if (str.startsWith(buildDir))
        str.remove(0, buildDir.count());
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

RulesApplicator::RulesApplicator(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag,
                                 ScriptEngine *engine, ProgressObserver *observer)
    : m_buildProduct(product)
    , m_artifactsPerFileTag(artifactsPerFileTag)
    , m_engine(engine)
    , m_observer(observer)
{
}

void RulesApplicator::applyAllRules()
{
    foreach (const RuleConstPtr &rule, m_buildProduct->topSortedRules())
        applyRule(rule);
}

void RulesApplicator::applyRule(const RuleConstPtr &rule)
{
    m_rule = rule;
    BuildGraph::setupScriptEngineForProduct(m_engine, m_buildProduct->rProduct, m_rule, scope());
    Q_ASSERT_X(scope().property("product").strictlyEquals(m_engine->evaluate("product")),
               "BG", "Product object is not in current scope.");

    ArtifactList inputArtifacts;
    foreach (const QString &fileTag, m_rule->inputs)
        inputArtifacts.unite(m_artifactsPerFileTag.value(fileTag));
    if (m_rule->multiplex) { // apply the rule once for a set of inputs
        if (!inputArtifacts.isEmpty())
            doApply(inputArtifacts);
    } else { // apply the rule once for each input
        ArtifactList lst;
        foreach (Artifact * const inputArtifact, inputArtifacts) {
            setupScriptEngineForArtifact(inputArtifact);
            lst += inputArtifact;
            doApply(lst);
            lst.clear();
        }
    }
}

void RulesApplicator::doApply(const ArtifactList &inputArtifacts)
{
    if (m_observer && m_observer->canceled())
        throw Error(Tr::tr("Build canceled."));

    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] apply rule " << m_rule->toString() << " "
                   << toStringList(inputArtifacts).join(",\n            ");

    QList<QPair<const RuleArtifact *, Artifact *> > ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    ArtifactList usingArtifacts;
    if (!m_rule->usings.isEmpty()) {
        const QSet<QString> usingsFileTags = m_rule->usings.toSet();
        foreach (const BuildProductPtr &dep, m_buildProduct->dependencies) {
            ArtifactList artifactsToCheck;
            foreach (Artifact *targetArtifact, dep->targetArtifacts)
                artifactsToCheck.unite(targetArtifact->transformer->outputs);
            foreach (Artifact *artifact, artifactsToCheck) {
                QSet<QString> matchingFileTags = artifact->fileTags;
                matchingFileTags.intersect(usingsFileTags);
                if (!matchingFileTags.isEmpty())
                    usingArtifacts.insert(artifact);
            }
        }
    }

    m_transformer.clear();
    // create the output artifacts from the set of input artifacts
    foreach (const RuleArtifactConstPtr &ruleArtifact, m_rule->artifacts) {
        Artifact * const outputArtifact = createOutputArtifact(ruleArtifact, inputArtifacts);
        outputArtifacts << outputArtifact;
        ruleArtifactArtifactMap << qMakePair(ruleArtifact.data(), outputArtifact);
    }

    foreach (Artifact *outputArtifact, outputArtifacts) {
        // insert the output artifacts into the pool of artifacts
        foreach (const QString &fileTag, outputArtifact->fileTags)
            m_artifactsPerFileTag[fileTag].insert(outputArtifact);

        // connect artifacts that match the file tags in explicitlyDependsOn
        foreach (const QString &fileTag, m_rule->explicitlyDependsOn)
            foreach (Artifact *dependency, m_artifactsPerFileTag.value(fileTag))
                BuildGraph::loggedConnect(outputArtifact, dependency);

        // Transformer setup
        for (ArtifactList::const_iterator it = usingArtifacts.constBegin();
             it != usingArtifacts.constEnd(); ++it)
        {
            Artifact *dep = *it;
            BuildGraph::loggedConnect(outputArtifact, dep);
            m_transformer->inputs.insert(dep);
        }
        m_transformer->outputs.insert(outputArtifact);

        m_buildProduct->project->removeFromArtifactsThatMustGetNewTransformers(outputArtifact);
    }

    m_transformer->setupInputs(m_engine, scope());

    // change the transformer outputs according to the bindings in Artifact
    QScriptValue scriptValue;
    for (int i = ruleArtifactArtifactMap.count(); --i >= 0;) {
        const RuleArtifact *ra = ruleArtifactArtifactMap.at(i).first;
        if (ra->bindings.isEmpty())
            continue;

        // expose attributes of this artifact
        Artifact *outputArtifact = ruleArtifactArtifactMap.at(i).second;
        outputArtifact->properties = outputArtifact->properties->clone();

        scope().setProperty("fileName", m_engine->toScriptValue(outputArtifact->filePath()));
        scope().setProperty("fileTags", toScriptValue(m_engine, outputArtifact->fileTags));

        QVariantMap artifactModulesCfg = outputArtifact->properties->value().value("modules").toMap();
        for (int i=0; i < ra->bindings.count(); ++i) {
            const RuleArtifact::Binding &binding = ra->bindings.at(i);
            scriptValue = m_engine->evaluate(binding.code);
            if (scriptValue.isError()) {
                QString msg = QLatin1String("evaluating rule binding '%1': %2");
                throw Error(msg.arg(binding.name.join(QLatin1String(".")), scriptValue.toString()), binding.location);
            }
            setConfigProperty(artifactModulesCfg, binding.name, scriptValue.toVariant());
        }
        QVariantMap outputArtifactConfig = outputArtifact->properties->value();
        outputArtifactConfig.insert("modules", artifactModulesCfg);
        outputArtifact->properties->setValue(outputArtifactConfig);
    }

    m_transformer->setupOutputs(m_engine, scope());
    m_transformer->createCommands(m_rule->script, m_engine);
    if (m_transformer->commands.isEmpty())
        throw Error(QString("There's a rule without commands: %1.").arg(m_rule->toString()), m_rule->script->location);
}

void RulesApplicator::setupScriptEngineForArtifact(Artifact *artifact)
{
    QString inFileName = artifact->fileName();
    QString inBaseName = FileInfo::baseName(artifact->filePath());
    QString inCompleteBaseName = FileInfo::completeBaseName(artifact->filePath());

    QString basedir;
    if (artifact->artifactType == Artifact::SourceFile) {
        QDir sourceDir(m_buildProduct->rProduct->sourceDirectory);
        basedir = FileInfo::path(sourceDir.relativeFilePath(artifact->filePath()));
    } else {
        QDir buildDir(m_buildProduct->project->resolvedProject()->buildDirectory);
        basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
    }

    QScriptValue modulesScriptValue = artifact->properties->toScriptValue(m_engine);
    modulesScriptValue = modulesScriptValue.property("modules");

    // expose per file properties we want to use in an Artifact within a Rule
    QScriptValue scriptValue = m_engine->newObject();
    scriptValue.setProperty("fileName", inFileName);
    scriptValue.setProperty("baseName", inBaseName);
    scriptValue.setProperty("completeBaseName", inCompleteBaseName);
    scriptValue.setProperty("baseDir", basedir);
    scriptValue.setProperty("modules", modulesScriptValue);

    scope().setProperty("input", scriptValue);
    Q_ASSERT_X(scriptValue.strictlyEquals(m_engine->evaluate("input")),
               "BG", "The input object is not in current scope.");
}

Artifact *RulesApplicator::createOutputArtifact(const RuleArtifactConstPtr &ruleArtifact,
        const ArtifactList &inputArtifacts)
{
    QScriptValue scriptValue = m_engine->evaluate(ruleArtifact->fileName);
    if (scriptValue.isError() || m_engine->hasUncaughtException())
        throw Error("Error in Rule.Artifact fileName: " + scriptValue.toString());
    QString outputPath = scriptValue.toString();
    outputPath.replace("..", "dotdot");     // don't let the output artifact "escape" its build dir
    outputPath = resolveOutPath(outputPath);

    Artifact *outputArtifact = m_buildProduct->lookupArtifact(outputPath);
    if (outputArtifact) {
        if (outputArtifact->transformer && outputArtifact->transformer != m_transformer) {
            // This can happen when applying rules after scanning for additional file tags.
            // We just regenerate the transformer.
            if (qbsLogLevel(LoggerTrace))
                qbsTrace("[BG] regenerating transformer for '%s'", qPrintable(fileName(outputArtifact)));
            m_transformer = outputArtifact->transformer;
            m_transformer->inputs.unite(inputArtifacts);

            if (m_transformer->inputs.count() > 1 && !m_rule->multiplex) {
                QString th = "[" + QStringList(outputArtifact->fileTags.toList()).join(", ") + "]";
                QString e = Tr::tr("Conflicting rules for producing %1 %2 \n").arg(outputArtifact->filePath(), th);
                th = "[" + m_rule->inputs.join(", ")
                   + "] -> [" + QStringList(outputArtifact->fileTags.toList()).join(", ") + "]";

                e += QString("  while trying to apply:   %1:%2:%3  %4\n")
                    .arg(m_rule->script->location.fileName)
                    .arg(m_rule->script->location.line)
                    .arg(m_rule->script->location.column)
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
        outputArtifact = new Artifact(m_buildProduct->project);
        outputArtifact->artifactType = Artifact::Generated;
        outputArtifact->setFilePath(outputPath);
        outputArtifact->fileTags = ruleArtifact->fileTags.toSet();
        outputArtifact->alwaysUpdated = ruleArtifact->alwaysUpdated;
        m_buildProduct->insertArtifact(outputArtifact);
    }

    if (outputArtifact->fileTags.isEmpty())
        outputArtifact->fileTags = m_buildProduct->rProduct->fileTagsForFileName(outputArtifact->fileName());

    if (m_rule->multiplex)
        outputArtifact->properties = m_buildProduct->rProduct->properties;
    else
        outputArtifact->properties= (*inputArtifacts.constBegin())->properties;

    foreach (Artifact *inputArtifact, inputArtifacts) {
        Q_ASSERT(outputArtifact != inputArtifact);
        BuildGraph::loggedConnect(outputArtifact, inputArtifact);
    }

    // create transformer if not already done so
    if (!m_transformer) {
        m_transformer = Transformer::create();
        m_transformer->rule = m_rule;
        m_transformer->inputs = inputArtifacts;
    }
    outputArtifact->transformer = m_transformer;

    return outputArtifact;
}

QString RulesApplicator::resolveOutPath(const QString &path) const
{
    QString buildDir = m_buildProduct->project->resolvedProject()->buildDirectory;
    QString result = FileInfo::resolvePath(buildDir, path);
    result = QDir::cleanPath(result);
    return result;
}

QScriptValue RulesApplicator::scope() const
{
    return m_engine->currentContext()->scopeChain().first();
}


BuildProjectPtr BuildProjectResolver::resolveProject(const ResolvedProjectPtr &resolvedProject,
        BuildGraph *buildgraph, ProgressObserver *observer)
{
    m_productCache.clear();
    m_observer = observer;
    m_project = BuildProjectPtr(new BuildProject(buildgraph));
    m_project->setResolvedProject(resolvedProject);
    if (m_observer)
        m_observer->initialize(Tr::tr("Resolving project"), resolvedProject->products.count());
    foreach (ResolvedProductPtr rProduct, resolvedProject->products) {
        resolveProduct(rProduct);
        if (m_observer)
            m_observer->incrementProgressValue();
    }
    CycleDetector().visitProject(m_project);
    return m_project;
}

BuildProductPtr BuildProjectResolver::resolveProduct(const ResolvedProductPtr &rProduct)
{
    BuildProductPtr product = m_productCache.value(rProduct);
    if (product)
        return product;

    if (m_observer && m_observer->canceled())
        throw Error(Tr::tr("Build canceled."));

    product = BuildProduct::create();
    m_productCache.insert(rProduct, product);
    product->project = m_project;
    product->rProduct = rProduct;
    ArtifactsPerFileTagMap artifactsPerFileTag;

    foreach (ResolvedProductPtr dependency, rProduct->dependencies) {
        if (dependency == rProduct)
            throw Error(Tr::tr("circular using"));
        BuildProductPtr referencedProduct = resolveProduct(dependency);
        product->dependencies.append(referencedProduct);
    }

    //add qbsFile artifact
    Artifact *qbsFileArtifact = product->lookupArtifact(rProduct->qbsFile);
    if (!qbsFileArtifact) {
        qbsFileArtifact = new Artifact(m_project.data());
        qbsFileArtifact->artifactType = Artifact::SourceFile;
        qbsFileArtifact->setFilePath(rProduct->qbsFile);
        qbsFileArtifact->properties = rProduct->properties;
        product->insertArtifact(qbsFileArtifact);
    }
    qbsFileArtifact->fileTags.insert("qbs");
    artifactsPerFileTag["qbs"].insert(qbsFileArtifact);

    // read sources
    foreach (const SourceArtifactConstPtr &sourceArtifact, rProduct->allFiles()) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (product->lookupArtifact(filePath))
            continue; // ignore duplicate artifacts

        Artifact *artifact = product->createArtifact(sourceArtifact);
        foreach (const QString &fileTag, artifact->fileTags)
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    // read manually added transformers
    foreach (const ResolvedTransformer::Ptr rtrafo, rProduct->transformers) {
        ArtifactList inputArtifacts;
        foreach (const QString &inputFileName, rtrafo->inputs) {
            Artifact *artifact = product->lookupArtifact(inputFileName);
            if (!artifact)
                throw Error(QString("Can't find artifact '%0' in the list of source files.").arg(inputFileName));
            inputArtifacts += artifact;
        }
        TransformerPtr transformer = Transformer::create();
        transformer->inputs = inputArtifacts;
        const RulePtr rule = Rule::create();
        rule->inputs = rtrafo->inputs;
        rule->jsImports = rtrafo->jsImports;
        ResolvedModulePtr module = ResolvedModule::create();
        module->name = rtrafo->module->name;
        rule->module = module;
        rule->script = rtrafo->transform;
        foreach (const SourceArtifactConstPtr &sourceArtifact, rtrafo->outputs) {
            Artifact *outputArtifact = product->createArtifact(sourceArtifact);
            outputArtifact->artifactType = Artifact::Generated;
            outputArtifact->transformer = transformer;
            transformer->outputs += outputArtifact;
            product->targetArtifacts += outputArtifact;
            foreach (Artifact *inputArtifact, inputArtifacts)
                buildGraph()->safeConnect(outputArtifact, inputArtifact);
            foreach (const QString &fileTag, outputArtifact->fileTags)
                artifactsPerFileTag[fileTag].insert(outputArtifact);

            RuleArtifactPtr ruleArtifact = RuleArtifact::create();
            ruleArtifact->fileName = outputArtifact->filePath();
            ruleArtifact->fileTags = outputArtifact->fileTags.toList();
            rule->artifacts += ruleArtifact;
        }
        transformer->rule = rule;

        BuildGraph::EngineInitializer initializer(buildGraph());
        buildGraph()->setupScriptEngineForProduct(engine(), rProduct, transformer->rule, scope());
        transformer->setupInputs(engine(), scope());
        transformer->setupOutputs(engine(), scope());
        transformer->createCommands(rtrafo->transform, buildGraph()->engine());
        if (transformer->commands.isEmpty())
            throw Error(QString("There's a transformer without commands."), rtrafo->transform->location);
    }

    product->applyRules(artifactsPerFileTag, buildGraph()->engine(), m_observer);

    QSet<Artifact *> productArtifactCandidates;
    for (int i = 0; i < product->rProduct->fileTags.count(); ++i)
        foreach (Artifact *artifact, artifactsPerFileTag.value(product->rProduct->fileTags.at(i)))
            if (artifact->artifactType == Artifact::Generated)
                productArtifactCandidates += artifact;

    if (productArtifactCandidates.isEmpty()) {
        QString msg = QLatin1String("No artifacts generated for product '%1'.");
        throw Error(msg.arg(product->rProduct->name));
    }

    product->targetArtifacts += productArtifactCandidates;
    m_project->addBuildProduct(product);
    return product;
}

QScriptValue BuildProjectResolver::scope() const
{
    return engine()->currentContext()->scopeChain().first();
}

BuildProjectLoader::LoadResult BuildProjectLoader::load(const QString &projectFilePath,
        BuildGraph *bg, const QString &buildRoot, const QVariantMap &cfg,
        const QStringList &loaderSearchPaths)
{
    m_result = LoadResult();
    m_engine = bg->engine();
    m_observer = bg->observer();

    const QString projectId = ResolvedProject::deriveId(cfg);
    const QString buildDir = ResolvedProject::deriveBuildDirectory(buildRoot, projectId);
    const QString buildGraphFilePath = BuildProject::deriveBuildGraphFilePath(buildDir, projectId);

    PersistentPool pool;
    qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    if (!pool.load(buildGraphFilePath))
        return m_result;
    if (!isConfigCompatible(cfg, pool.headData().projectConfig)) {
        qbsDebug() << "[BG] Cannot use stored build graph: Incompatible project configuration.";
        return m_result;
    }

    const BuildProjectPtr project = BuildProjectPtr(new BuildProject(bg));
    TimedActivityLogger loadLogger(QLatin1String("Loading build graph"), QLatin1String("[BG] "));
    project->load(pool);
    foreach (const BuildProductPtr &bp, project->buildProducts())
        bp->project = project;
    project->resolvedProject()->qbsFile = projectFilePath;
    project->resolvedProject()->setBuildConfiguration(pool.headData().projectConfig);
    project->resolvedProject()->buildDirectory = buildDir;
    m_result.loadedProject = project;
    loadLogger.finishActivity();

    const FileInfo bgfi(buildGraphFilePath);
    const bool projectFileChanged = bgfi.lastModified() < FileInfo(projectFilePath).lastModified();

    bool referencedProductRemoved = false;
    QList<BuildProductPtr> changedProducts;
    foreach (BuildProductPtr product, project->buildProducts()) {
        const ResolvedProductConstPtr &resolvedProduct = product->rProduct;
        const FileInfo pfi(resolvedProduct->qbsFile);
        if (!pfi.exists()) {
            referencedProductRemoved = true;
        } else if (bgfi.lastModified() < pfi.lastModified()) {
            changedProducts += product;
        } else {
            foreach (const ResolvedGroup::Ptr &group, resolvedProduct->groups) {
                if (!group->wildcards)
                    continue;
                const QSet<QString> files
                        = group->wildcards->expandPatterns(resolvedProduct->sourceDirectory);
                QSet<QString> wcFiles;
                foreach (const SourceArtifactConstPtr &sourceArtifact, group->wildcards->files)
                    wcFiles += sourceArtifact->absoluteFilePath;
                if (files == wcFiles)
                    continue;
                changedProducts += product;
                break;
            }
        }
    }

    if (projectFileChanged || referencedProductRemoved || !changedProducts.isEmpty()) {
        Loader ldr(bg->engine());
        ldr.setSearchPaths(loaderSearchPaths);
        const ResolvedProjectPtr changedProject
                = ldr.loadProject(project->resolvedProject()->qbsFile, buildRoot, cfg);
        m_result.changedResolvedProject = changedProject;

        QMap<QString, ResolvedProductPtr> changedProductsMap;
        foreach (BuildProductPtr product, changedProducts) {
            if (changedProductsMap.isEmpty())
                foreach (ResolvedProductPtr cp, changedProject->products)
                    changedProductsMap.insert(cp->name, cp);
            ResolvedProductPtr changedProduct = changedProductsMap.value(product->rProduct->name);
            if (!changedProduct)
                continue;
            onProductChanged(product, changedProduct);
            if (m_result.discardLoadedProject)
                return m_result;
        }

        QSet<QString> oldProductNames, newProductNames;
        foreach (const BuildProductPtr &product, project->buildProducts())
            oldProductNames += product->rProduct->name;
        foreach (const ResolvedProductConstPtr &product, changedProject->products)
            newProductNames += product->name;
        QSet<QString> addedProductNames = newProductNames - oldProductNames;
        if (!addedProductNames.isEmpty()) {
            m_result.discardLoadedProject = true;
            return m_result;
        }
        QSet<QString> removedProductsNames = oldProductNames - newProductNames;
        if (!removedProductsNames.isEmpty()) {
            foreach (const BuildProductPtr &product, project->buildProducts()) {
                if (removedProductsNames.contains(product->rProduct->name))
                    onProductRemoved(product);
            }
        }

        CycleDetector().visitProject(project);
    }

    return m_result;
}

void BuildProjectLoader::onProductRemoved(const BuildProductPtr &product)
{
    qbsDebug() << "[BG] product '" << product->rProduct->name << "' removed.";

    product->project->markDirty();
    product->project->m_buildProducts.remove(product);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, product->artifacts) {
        artifact->disconnectAll();
        BuildGraph::removeGeneratedArtifactFromDisk(artifact);
    }
}

void BuildProjectLoader::onProductChanged(const BuildProductPtr &product,
                                          const ResolvedProductPtr &changedProduct)
{
    qbsDebug() << "[BG] product '" << product->rProduct->name << "' changed.";

    ArtifactsPerFileTagMap artifactsPerFileTag;
    QSet<SourceArtifactConstPtr> addedSourceArtifacts;
    QList<Artifact *> addedArtifacts, artifactsToRemove;
    QHash<QString, SourceArtifactConstPtr> oldArtifacts, newArtifacts;

    const QList<SourceArtifactPtr> oldProductAllFiles = product->rProduct->allFiles();
    foreach (const SourceArtifactConstPtr &a, oldProductAllFiles)
        oldArtifacts.insert(a->absoluteFilePath, a);
    foreach (const SourceArtifactPtr &a, changedProduct->allFiles()) {
        newArtifacts.insert(a->absoluteFilePath, a);
        if (!oldArtifacts.contains(a->absoluteFilePath)) {
            // artifact added
            qbsDebug() << "[BG] artifact '" << a->absoluteFilePath << "' added to product " << product->rProduct->name;
            addedArtifacts += product->createArtifact(a);
            addedSourceArtifacts += a;
        }
    }

    foreach (const SourceArtifactPtr &a, oldProductAllFiles) {
        const SourceArtifactConstPtr changedArtifact = newArtifacts.value(a->absoluteFilePath);
        if (!changedArtifact) {
            // artifact removed
            qbsDebug() << "[BG] artifact '" << a->absoluteFilePath << "' removed from product " << product->rProduct->name;
            Artifact *artifact = product->lookupArtifact(a->absoluteFilePath);
            Q_ASSERT(artifact);
            removeArtifactAndExclusiveDependents(artifact, &artifactsToRemove);
            continue;
        }
        if (!addedSourceArtifacts.contains(changedArtifact)
            && changedArtifact->properties->value() != a->properties->value())
        {
            qbsInfo("Some properties changed. Regenerating build graph.");
            qbsDebug("Artifact with changed properties: %s", qPrintable(changedArtifact->absoluteFilePath));
            m_result.discardLoadedProject = true;
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

    // Discard groups of the old product. Use the groups of the new one.
    product->rProduct->groups = changedProduct->groups;
    product->rProduct->properties = changedProduct->properties;

    // apply rules for new artifacts
    foreach (Artifact *artifact, addedArtifacts)
        foreach (const QString &ft, artifact->fileTags)
            artifactsPerFileTag[ft] += artifact;
    product->applyRules(artifactsPerFileTag, m_engine, m_observer);

    // parents of removed artifacts must update their transformers
    foreach (Artifact *removedArtifact, artifactsToRemove)
        foreach (Artifact *parent, removedArtifact->parents)
            product->project->addToArtifactsThatMustGetNewTransformers(parent);
    product->project->updateNodesThatMustGetNewTransformer(m_engine, m_observer);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, artifactsToRemove) {
        BuildGraph::removeGeneratedArtifactFromDisk(artifact);
        delete artifact;
    }
}

/**
  * Removes the artifact and all the artifacts that depend exclusively on it.
  * Example: if you remove a cpp artifact then the obj artifact is removed but
  * not the resulting application (if there's more then one cpp artifact).
  */
void BuildProjectLoader::removeArtifactAndExclusiveDependents(Artifact *artifact,
                                                              QList<Artifact*> *removedArtifacts)
{
    if (removedArtifacts)
        removedArtifacts->append(artifact);
    foreach (Artifact *parent, artifact->parents) {
        bool removeParent = false;
        BuildGraph::disconnect(parent, artifact);
        if (parent->children.isEmpty()) {
            removeParent = true;
        } else if (parent->transformer) {
            artifact->project->addToArtifactsThatMustGetNewTransformers(parent);
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.isEmpty();
        }
        if (removeParent)
            removeArtifactAndExclusiveDependents(parent, removedArtifacts);
    }
    artifact->project->removeArtifact(artifact);
}

} // namespace Internal
} // namespace qbs
