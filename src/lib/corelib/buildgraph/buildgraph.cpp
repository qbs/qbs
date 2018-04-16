/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "buildgraph.h"

#include "artifact.h"
#include "cycledetector.h"
#include "projectbuilddata.h"
#include "productbuilddata.h"
#include "rulenode.h"
#include "transformer.h"

#include <jsextensions/jsextensions.h>
#include <jsextensions/moduleproperties.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <algorithm>

namespace qbs {
namespace Internal {

static QString artifactsProperty() { return QStringLiteral("artifacts"); }
static QString dependenciesProperty() { return QStringLiteral("dependencies"); }
static QString parametersProperty() { return QStringLiteral("parameters"); }

static QScriptValue setupProjectScriptValue(ScriptEngine *engine,
        const ResolvedProjectConstPtr &project, PrepareScriptObserver *observer)
{
    QScriptValue obj = engine->newObject();
    obj.setProperty(StringConstants::filePathProperty(), project->location.filePath());
    obj.setProperty(StringConstants::pathProperty(), FileInfo::path(project->location.filePath()));
    const QVariantMap &projectProperties = project->projectProperties();
    for (QVariantMap::const_iterator it = projectProperties.begin();
            it != projectProperties.end(); ++it) {
        engine->setObservedProperty(obj, it.key(), engine->toScriptValue(it.value()), observer);
    }
    return obj;
}

static void setupProductScriptValue(ScriptEngine *engine, QScriptValue &productScriptValue,
                                    const ResolvedProduct *product,
                                    PrepareScriptObserver *observer);

enum BuildGraphScriptValueCommonPropertyKeys : quint32 {
    CachedValueKey,
    FileTagKey
};

static bool isRelevantArtifact(const ResolvedProduct *, const Artifact *artifact)
{
    return !artifact->isTargetOfModule();
}
static bool isRelevantArtifact(const ResolvedModule *module, const Artifact *artifact)
{
    return artifact->targetOfModule == module->name;
}

static ProductBuildData::ArtifactSetByFileTag &artifactsMap(const ResolvedProduct *product)
{
    return product->buildData->artifactsByFileTag;
}

static ProductBuildData::ArtifactSetByFileTag &artifactsMap(const ResolvedModule *module)
{
    return artifactsMap(module->product);
}

template<class ProductOrModule> static QScriptValue js_artifactsForFileTag(
        QScriptContext *ctx, ScriptEngine *engine, const ProductOrModule *productOrModule)
{
    QScriptValue result = ctx->callee().property(CachedValueKey);
    if (result.isArray())
        return result;
    const FileTag fileTag = FileTag(ctx->callee().property(FileTagKey).toString().toUtf8());
    auto artifacts = artifactsMap(productOrModule).value(fileTag);
    const auto filter = [productOrModule](const Artifact *a) {
        return !isRelevantArtifact(productOrModule, a);
    };
    artifacts.erase(std::remove_if(artifacts.begin(), artifacts.end(), filter), artifacts.end());
    result = engine->newArray(uint(artifacts.size()));
    ctx->callee().setProperty(CachedValueKey, result);
    int k = 0;
    for (const Artifact * const artifact : artifacts)
        result.setProperty(k++, Transformer::translateFileConfig(engine, artifact, QString()));
    return result;
}

template<class ProductOrModule> static QScriptValue js_artifacts(
        QScriptContext *ctx, ScriptEngine *engine, const ProductOrModule *productOrModule)
{
    QScriptValue artifactsObj = ctx->callee().property(CachedValueKey);
    if (artifactsObj.isObject())
        return artifactsObj;
    artifactsObj = engine->newObject();
    ctx->callee().setProperty(CachedValueKey, artifactsObj);
    const auto &map = artifactsMap(productOrModule);
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const auto filter = [productOrModule](const Artifact *a) {
            return isRelevantArtifact(productOrModule, a);
        };
        if (std::none_of(it.value().cbegin(), it.value().cend(), filter))
            continue;
        QScriptValue fileTagFunc = engine->newFunction(&js_artifactsForFileTag<ProductOrModule>,
                                                       productOrModule);
        const QString fileTag = it.key().toString();
        fileTagFunc.setProperty(FileTagKey, fileTag);
        artifactsObj.setProperty(fileTag, fileTagFunc,
                                 QScriptValue::ReadOnly | QScriptValue::Undeletable
                                 | QScriptValue::PropertyGetter);
    }
    return artifactsObj;
}

class DependenciesFunction
{
public:
    DependenciesFunction(ScriptEngine *engine)
        : m_engine(engine)
    {
    }

    void init(QScriptValue &productScriptValue, const ResolvedProduct *product,
              PrepareScriptObserver *observer)
    {
        QScriptValue depfunc = m_engine->newFunction(&js_productDependencies, product);
        setObserver(depfunc, observer);
        productScriptValue.setProperty(dependenciesProperty(), depfunc,
                                       QScriptValue::ReadOnly | QScriptValue::Undeletable
                                       | QScriptValue::PropertyGetter);
    }

private:
    static QScriptValue js_productDependencies(QScriptContext *ctx, ScriptEngine *engine,
                                               const ResolvedProduct * const product)
    {
        QScriptValue result = engine->newArray();
        quint32 idx = 0;
        QList<ResolvedProductPtr> productDeps = product->dependencies.toList();
        std::sort(productDeps.begin(), productDeps.end(),
                  [](const ResolvedProductPtr &p1, const ResolvedProductPtr &p2) {
                          return p1->name < p2->name;
                  });
        PrepareScriptObserver *observer = getObserver(ctx->callee());
        for (const ResolvedProductPtr &dependency : qAsConst(productDeps)) {
            QScriptValue obj = engine->newObject();
            setupProductScriptValue(static_cast<ScriptEngine *>(engine), obj, dependency.get(), 0);

            const QVariantMap &params = product->dependencyParameters.value(dependency);
            obj.setProperty(parametersProperty(), params.empty()
                            ? engine->newObject() : toScriptValue(engine, observer, params,
                                                                  dependency->name));

            result.setProperty(idx++, obj);
        }
        for (const ResolvedModuleConstPtr &dependency : qAsConst(product->modules)) {
            if (dependency->isProduct)
                continue;
            QScriptValue obj = engine->newObject();
            setupModuleScriptValue(static_cast<ScriptEngine *>(engine), obj, dependency.get());
            const QVariantMap &params = product->moduleParameters.value(dependency);
            obj.setProperty(parametersProperty(), params.empty()
                            ? engine->newObject() : toScriptValue(engine, observer, params,
                                                                  dependency->name));

            result.setProperty(idx++, obj);
        }
        return result;
    }

    static QScriptValue js_moduleDependencies(QScriptContext *ctx, ScriptEngine *engine,
                                              const ResolvedModule *module)
    {
        QScriptValue result = engine->newArray();
        quint32 idx = 0;
        PrepareScriptObserver * const observer = getObserver(ctx->callee());
        for (const QString &depName : qAsConst(module->moduleDependencies)) {
            QScriptValue obj = engine->newObject();
            obj.setProperty(StringConstants::nameProperty(), depName);
            for (const ResolvedModuleConstPtr &dep : qAsConst(module->product->modules)) {
                if (dep->name == depName) {
                    setupModuleScriptValue(static_cast<ScriptEngine *>(engine), obj, dep.get());
                    const QVariantMap &params = module->product->moduleParameters.value(dep);
                    obj.setProperty(parametersProperty(), params.empty()
                                    ? engine->newObject() : toScriptValue(engine, observer, params,
                                                                          dep->name));
                    result.setProperty(idx++, obj);
                    break;
                }
            }
        }
        QBS_ASSERT(idx == quint32(module->moduleDependencies.size()),;);
        return result;
    }

    static void setupModuleScriptValue(ScriptEngine *engine, QScriptValue &moduleScriptValue,
                                       const ResolvedModule *module)
    {
        const QVariantMap propMap = module->product->moduleProperties->value()
                .value(module->name).toMap();
        for (QVariantMap::ConstIterator it = propMap.constBegin(); it != propMap.constEnd(); ++it) {
            const QVariant &value = it.value();
            if (value.isValid())
                moduleScriptValue.setProperty(it.key(), engine->toScriptValue(value));
        }
        QScriptValue depfunc = engine->newFunction<const ResolvedModule *>(&js_moduleDependencies,
                                                                           module);
        moduleScriptValue.setProperty(dependenciesProperty(), depfunc,
                                      QScriptValue::ReadOnly | QScriptValue::Undeletable
                                      | QScriptValue::PropertyGetter);
        QScriptValue artifactsFunc = engine->newFunction(&js_artifacts<ResolvedModule>, module);
        moduleScriptValue.setProperty(artifactsProperty(), artifactsFunc,
                                       QScriptValue::ReadOnly | QScriptValue::Undeletable
                                       | QScriptValue::PropertyGetter);
    }

    static void setObserver(QScriptValue scriptValue, PrepareScriptObserver *observer)
    {
        attachPointerTo(scriptValue, observer);
    }

    static PrepareScriptObserver *getObserver(const QScriptValue &scriptValue)
    {
        return attachedPointer<PrepareScriptObserver>(scriptValue);
    }

    static QScriptValue toScriptValue(QScriptEngine *qScriptEngine, PrepareScriptObserver *observer,
                                      const QVariantMap &v, const QString &depName)
    {
        return toScriptValue(static_cast<ScriptEngine *>(qScriptEngine), observer, v, depName);
    }

    static QScriptValue toScriptValue(ScriptEngine *engine, PrepareScriptObserver *observer,
                                      const QVariantMap &v, const QString &depName,
                                      const QualifiedId &moduleName = QualifiedId())
    {
        QScriptValue obj = engine->newObject();
        bool objIdAddedToObserver = false;
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (it.value().type() == QVariant::Map) {
                obj.setProperty(it.key(), toScriptValue(engine, observer, it.value().toMap(),
                        depName, QualifiedId(moduleName) << it.key()));
            } else {
                if (observer && !objIdAddedToObserver) {
                    objIdAddedToObserver = true;
                    observer->addParameterObjectId(obj.objectId(), depName, moduleName);
                }
                engine->setObservedProperty(obj, it.key(), engine->toScriptValue(it.value()),
                                            observer);
            }
        }
        return obj;
    }

    ScriptEngine *m_engine;
};

static void setupProductScriptValue(ScriptEngine *engine, QScriptValue &productScriptValue,
                                    const ResolvedProduct *product,
                                    PrepareScriptObserver *observer)
{
    ModuleProperties::init(productScriptValue, product);

    QScriptValue artifactsFunc = engine->newFunction(&js_artifacts<ResolvedProduct>, product);
    productScriptValue.setProperty(artifactsProperty(), artifactsFunc,
                                   QScriptValue::ReadOnly | QScriptValue::Undeletable
                                   | QScriptValue::PropertyGetter);

    DependenciesFunction(engine).init(productScriptValue, product, observer);
    if (observer)
        observer->setProductObjectId(productScriptValue.objectId());
    const QVariantMap &propMap = product->productProperties;
    for (QVariantMap::ConstIterator it = propMap.constBegin(); it != propMap.constEnd(); ++it) {
        engine->setObservedProperty(productScriptValue, it.key(), engine->toScriptValue(it.value()),
                                    observer);
    }
}

void setupScriptEngineForFile(ScriptEngine *engine, const FileContextBaseConstPtr &fileContext,
        QScriptValue targetObject, const ObserveMode &observeMode)
{
    engine->import(fileContext, targetObject, observeMode);
    JsExtensions::setupExtensions(fileContext->jsExtensions(), targetObject);
}

void setupScriptEngineForProduct(ScriptEngine *engine, ResolvedProduct *product,
                                 const ResolvedModule *module, QScriptValue targetObject,
                                 PrepareScriptObserver *observer, bool setBuildEnvironment)
{
    QScriptValue projectScriptValue = setupProjectScriptValue(engine, product->project.lock(),
                                                              observer);
    targetObject.setProperty(StringConstants::projectVar(), projectScriptValue);
    if (observer)
        observer->setProjectObjectId(projectScriptValue.objectId());

    if (setBuildEnvironment) {
        QVariant v;
        v.setValue<void*>(&product->buildEnvironment);
        engine->setProperty(StringConstants::qbsProcEnvVarInternal(), v);
    }
    QScriptValue productScriptValue = engine->newObject();
    setupProductScriptValue(engine, productScriptValue, product, observer);
    targetObject.setProperty(StringConstants::productVar(), productScriptValue);

    // If the Rule is in a Module, set up the 'moduleName' property
    productScriptValue.setProperty(StringConstants::moduleNameProperty(),
            module->name.isEmpty() ? QScriptValue() : module->name);
}

bool findPath(BuildGraphNode *u, BuildGraphNode *v, QList<BuildGraphNode *> &path)
{
    if (u == v) {
        path.push_back(v);
        return true;
    }

    for (BuildGraphNode * const childNode : qAsConst(u->children)) {
        if (findPath(childNode, v, path)) {
            path.prepend(u);
            return true;
        }
    }

    return false;
}

/*
 * Creates the build graph edge p -> c, which represents the dependency "c must be built before p".
 */
void connect(BuildGraphNode *p, BuildGraphNode *c)
{
    QBS_CHECK(p != c);
    qCDebug(lcBuildGraph) << "connect" << p->toString() << "->" << c->toString();
    if (c->type() == BuildGraphNode::ArtifactNodeType) {
        Artifact * const ac = static_cast<Artifact *>(c);
        for (const Artifact *child : filterByType<Artifact>(p->children)) {
            if (child == ac)
                return;
            const bool filePathsMustBeDifferent = child->artifactType == Artifact::Generated
                    || child->product == ac->product || child->artifactType != ac->artifactType;
            if (filePathsMustBeDifferent && child->filePath() == ac->filePath()) {
                throw ErrorInfo(QString::fromLatin1("%1 already has a child artifact %2 as "
                                                    "different object.").arg(p->toString(),
                                                                             ac->filePath()),
                                CodeLocation(), true);
            }
        }
    }
    p->children.insert(c);
    c->parents.insert(p);
    p->product->topLevelProject()->buildData->isDirty = true;
}

static bool existsPath_impl(BuildGraphNode *u, BuildGraphNode *v, NodeSet *seen)
{
    if (u == v)
        return true;

    if (!seen->insert(u).second)
        return false;

    for (BuildGraphNode * const childNode : qAsConst(u->children)) {
        if (existsPath_impl(childNode, v, seen))
            return true;
    }

    return false;
}

static bool existsPath(BuildGraphNode *u, BuildGraphNode *v)
{
    NodeSet seen;
    return existsPath_impl(u, v, &seen);
}

static QStringList toStringList(const QList<BuildGraphNode *> &path)
{
    QStringList lst;
    for (BuildGraphNode *node : path)
        lst << node->toString();
    return lst;
}

bool safeConnect(Artifact *u, Artifact *v)
{
    QBS_CHECK(u != v);
    qCDebug(lcBuildGraph) << "safeConnect:" << relativeArtifactFileName(u)
                          << "->" << relativeArtifactFileName(v);

    if (existsPath(v, u)) {
        QList<BuildGraphNode *> circle;
        findPath(v, u, circle);
        qCDebug(lcBuildGraph) << "safeConnect: circle detected " << toStringList(circle);
        return false;
    }

    connect(u, v);
    return true;
}

void disconnect(BuildGraphNode *u, BuildGraphNode *v)
{
    qCDebug(lcBuildGraph) << "disconnect:" << u->toString() << v->toString();
    u->children.remove(v);
    v->parents.remove(u);
    u->onChildDisconnected(v);
}

void removeGeneratedArtifactFromDisk(Artifact *artifact, const Logger &logger)
{
    if (artifact->artifactType != Artifact::Generated)
        return;
    removeGeneratedArtifactFromDisk(artifact->filePath(), logger);
}

void removeGeneratedArtifactFromDisk(const QString &filePath, const Logger &logger)
{
    QFile file(filePath);
    if (!file.exists())
        return;
    logger.qbsDebug() << "removing " << filePath;
    if (!file.remove())
        logger.qbsWarning() << QString::fromLatin1("Cannot remove '%1'.").arg(filePath);
}

QString relativeArtifactFileName(const Artifact *artifact)
{
    const QString &buildDir = artifact->product->topLevelProject()->buildDirectory;
    QString str = artifact->filePath();
    if (str.startsWith(buildDir))
        str.remove(0, buildDir.size());
    if (str.startsWith(QLatin1Char('/')))
        str.remove(0, 1);
    return str;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product,
        const ProjectBuildData *projectBuildData, const QString &dirPath, const QString &fileName,
        bool compareByName)
{
    const QList<FileResourceBase *> lookupResults
            = projectBuildData->lookupFiles(dirPath, fileName);
    for (QList<FileResourceBase *>::const_iterator it = lookupResults.constBegin();
            it != lookupResults.constEnd(); ++it) {
        if ((*it)->fileType() != FileResourceBase::FileTypeArtifact)
            continue;
        Artifact *artifact = static_cast<Artifact *>(*it);
        if (compareByName
                ? artifact->product->uniqueName() == product->uniqueName()
                : artifact->product == product) {
            return artifact;
        }
    }
    return nullptr;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &dirPath,
                         const QString &fileName, bool compareByName)
{
    return lookupArtifact(product, product->topLevelProject()->buildData.get(), dirPath, fileName,
                          compareByName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &filePath,
                         bool compareByName)
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(product, dirPath, fileName, compareByName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const ProjectBuildData *buildData,
                         const QString &filePath, bool compareByName)
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(product, buildData, dirPath, fileName, compareByName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const Artifact *artifact,
                         bool compareByName)
{
    return lookupArtifact(product, artifact->dirPath(), artifact->fileName(), compareByName);
}

Artifact *createArtifact(const ResolvedProductPtr &product,
                         const SourceArtifactConstPtr &sourceArtifact)
{
    Artifact *artifact = new Artifact;
    artifact->artifactType = Artifact::SourceFile;
    artifact->targetOfModule = sourceArtifact->targetOfModule;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->setFileTags(sourceArtifact->fileTags);
    artifact->properties = sourceArtifact->properties;
    insertArtifact(product, artifact);
    return artifact;
}

void insertArtifact(const ResolvedProductPtr &product, Artifact *artifact)
{
    qCDebug(lcBuildGraph) << "insert artifact" << artifact->filePath();
    QBS_CHECK(!artifact->product);
    QBS_CHECK(!artifact->filePath().isEmpty());
    QBS_CHECK(!product->buildData->nodes.contains(artifact));
    artifact->product = product;
    product->topLevelProject()->buildData->insertIntoLookupTable(artifact);
    product->topLevelProject()->buildData->isDirty = true;
    product->buildData->nodes.insert(artifact);
    addArtifactToSet(artifact, product->buildData->artifactsByFileTag);
}

static void doSanityChecksForProduct(const ResolvedProductConstPtr &product,
        const Set<ResolvedProductPtr> &allProducts, const Logger &logger)
{
    qCDebug(lcBuildGraph) << "Sanity checking product" << product->uniqueName();
    CycleDetector cycleDetector(logger);
    cycleDetector.visitProduct(product);
    const ProductBuildData * const buildData = product->buildData.get();
    for (const ResolvedModuleConstPtr &m : qAsConst(product->modules))
        QBS_CHECK(m->product == product.get());
    qCDebug(lcBuildGraph) << "enabled:" << product->enabled << "build data:" << buildData;
    QBS_CHECK(!!product->enabled == !!buildData);
    if (!product->enabled)
        return;
    for (BuildGraphNode * const node : qAsConst(buildData->roots)) {
        qCDebug(lcBuildGraph) << "Checking root node" << node->toString();
        QBS_CHECK(buildData->nodes.contains(node));
    }
    Set<QString> filePaths;
    for (BuildGraphNode * const node : qAsConst(buildData->nodes)) {
        qCDebug(lcBuildGraph) << "Sanity checking node" << node->toString();
        QBS_CHECK(node->product == product);
        for (const BuildGraphNode * const parent : qAsConst(node->parents))
            QBS_CHECK(parent->children.contains(node));
        for (BuildGraphNode * const child : qAsConst(node->children)) {
            QBS_CHECK(child->parents.contains(node));
            QBS_CHECK(!child->product.expired());
            QBS_CHECK(child->product->buildData);
            QBS_CHECK(child->product->buildData->nodes.contains(child));
            QBS_CHECK(allProducts.contains(child->product.lock()));
        }

        Artifact * const artifact = node->type() == BuildGraphNode::ArtifactNodeType
                ? static_cast<Artifact *>(node) : nullptr;
        if (!artifact) {
            QBS_CHECK(node->type() == BuildGraphNode::RuleNodeType);
            RuleNode * const ruleNode = static_cast<RuleNode *>(node);
            QBS_CHECK(ruleNode->rule());
            QBS_CHECK(ruleNode->rule()->product);
            QBS_CHECK(ruleNode->rule()->product == ruleNode->product.get());
            QBS_CHECK(ruleNode->rule()->product == product.get());
            QBS_CHECK(product->rules.contains(std::const_pointer_cast<Rule>(ruleNode->rule())));
            continue;
        }

        QBS_CHECK(artifact->artifactType == Artifact::SourceFile ||
                  !filePaths.contains(artifact->filePath()));
        filePaths << artifact->filePath();

        for (Artifact * const child : qAsConst(artifact->childrenAddedByScanner))
            QBS_CHECK(artifact->children.contains(child));
        const TransformerConstPtr transformer = artifact->transformer;
        if (artifact->artifactType == Artifact::SourceFile)
            continue;

        QBS_CHECK(transformer);
        QBS_CHECK(transformer->rule);
        QBS_CHECK(transformer->rule->product);
        QBS_CHECK(transformer->rule->product == artifact->product.get());
        QBS_CHECK(transformer->rule->product == product.get());
        QBS_CHECK(transformer->outputs.contains(artifact));
        QBS_CHECK(product->rules.contains(std::const_pointer_cast<Rule>(transformer->rule)));
        qCDebug(lcBuildGraph)
                << "The transformer has" << transformer->outputs.size() << "outputs.";
        ArtifactSet transformerOutputChildren;
        for (const Artifact * const output : qAsConst(transformer->outputs)) {
            QBS_CHECK(output->transformer == transformer);
            transformerOutputChildren.unite(ArtifactSet::filtered(output->children));
            Set<QString> childFilePaths;
            for (const Artifact *a : filterByType<Artifact>(output->children)) {
                if (!childFilePaths.insert(a->filePath()).second) {
                    throw ErrorInfo(QString::fromLatin1("There is more than one artifact for "
                        "file '%1' in the child list for output '%2'.")
                        .arg(a->filePath(), output->filePath()), CodeLocation(), true);
                }
            }
        }
        if (lcBuildGraph().isDebugEnabled()) {
            qCDebug(lcBuildGraph) << "The transformer output children are:";
            for (const Artifact * const a : qAsConst(transformerOutputChildren))
                qCDebug(lcBuildGraph) << "\t" << a->fileName();
            qCDebug(lcBuildGraph) << "The transformer inputs are:";
            for (const Artifact * const a : qAsConst(transformer->inputs))
                qCDebug(lcBuildGraph) << "\t" << a->fileName();
        }
        QBS_CHECK(transformer->inputs.size() <= transformerOutputChildren.size());
        for (Artifact * const transformerInput : qAsConst(transformer->inputs))
            QBS_CHECK(transformerOutputChildren.contains(transformerInput));
    }
}

static void doSanityChecks(const ResolvedProjectPtr &project,
                           const Set<ResolvedProductPtr> &allProducts, Set<QString> &productNames,
                           const Logger &logger)
{
    logger.qbsDebug() << "Sanity checking project '" << project->name << "'";
    for (const ResolvedProjectPtr &subProject : qAsConst(project->subProjects))
        doSanityChecks(subProject, allProducts, productNames, logger);

    for (const ResolvedProductConstPtr &product : qAsConst(project->products)) {
        QBS_CHECK(product->project == project);
        QBS_CHECK(product->topLevelProject() == project->topLevelProject());
        doSanityChecksForProduct(product, allProducts, logger);
        QBS_CHECK(!productNames.contains(product->uniqueName()));
        productNames << product->uniqueName();
    }
}

void doSanityChecks(const ResolvedProjectPtr &project, const Logger &logger)
{
    if (qEnvironmentVariableIsEmpty("QBS_SANITY_CHECKS"))
        return;
    Set<QString> productNames;
    const Set<ResolvedProductPtr> allProducts
            = Set<ResolvedProductPtr>::fromList(project->allProducts());
    doSanityChecks(project, allProducts, productNames, logger);
}

} // namespace Internal
} // namespace qbs
