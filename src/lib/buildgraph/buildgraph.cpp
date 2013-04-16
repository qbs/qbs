/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
#include "projectbuilddata.h"
#include "productbuilddata.h"
#include "cycledetector.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <jsextensions/moduleproperties.h>
#include <language/loader.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/propertyfinder.h>
#include <tools/qbsassert.h>
#include <tools/setupprojectparameters.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace qbs {
namespace Internal {

static void setupProductScriptValue(ScriptEngine *engine, QScriptValue &productScriptValue,
                                    const ResolvedProductConstPtr &product,
                                    ScriptPropertyObserver *observer);

class DependenciesFunction
{
public:
    DependenciesFunction(ScriptEngine *engine)
        : m_engine(engine)
    {
    }

    void init(QScriptValue &productScriptValue, const ResolvedProductConstPtr &product)
    {
        QScriptValue depfunc = m_engine->newFunction(&js_productDependencies);
        setProduct(depfunc, product.data());
        QScriptValue descriptor = m_engine->newObject();
        descriptor.setProperty(QLatin1String("get"), depfunc);
        descriptor.setProperty(QLatin1String("set"), m_engine->evaluate("(function(){})"));
        descriptor.setProperty(QLatin1String("enumerable"), true);
        m_engine->defineProperty(productScriptValue, QLatin1String("dependencies"), descriptor);
    }

private:
    static QScriptValue js_productDependencies(QScriptContext *context, QScriptEngine *engine)
    {
        QScriptValue callee = context->callee();
        const ResolvedProduct * const product = getProduct(callee);
        QScriptValue result = engine->newArray();
        quint32 idx = 0;
        foreach (const ResolvedProductPtr &dependency, product->dependencies) {
            QScriptValue obj = engine->newObject();
            setupProductScriptValue(static_cast<ScriptEngine *>(engine), obj, dependency, 0);
            result.setProperty(idx++, obj);
        }
        foreach (const ResolvedModuleConstPtr &dependency, product->modules) {
            QScriptValue obj = engine->newObject();
            setupModuleScriptValue(static_cast<ScriptEngine *>(engine), obj,
                                   product->properties->value(), dependency->name);
            result.setProperty(idx++, obj);
        }
        return result;
    }

    static QScriptValue js_moduleDependencies(QScriptContext *context, QScriptEngine *engine)
    {
        QScriptValue callee = context->callee();
        const QVariantMap modulesMap = callee.data().toVariant().toMap();
        QScriptValue result = engine->newArray();
        quint32 idx = 0;
        for (QVariantMap::const_iterator it = modulesMap.begin(); it != modulesMap.end(); ++it) {
            QScriptValue obj = engine->newObject();
            obj.setProperty(QLatin1String("name"), it.key());
            setupModuleScriptValue(static_cast<ScriptEngine *>(engine), obj, it.value().toMap(),
                                   it.key());
            result.setProperty(idx++, obj);
        }
        return result;
    }

    static void setupModuleScriptValue(ScriptEngine *engine, QScriptValue &moduleScriptValue,
                                       const QVariantMap &propertyMap,
                                       const QString &moduleName)
    {
        const QVariantMap propMap
                = propertyMap.value(QLatin1String("modules")).toMap().value(moduleName).toMap();
        for (QVariantMap::ConstIterator it = propMap.constBegin(); it != propMap.constEnd(); ++it) {
            const QVariant &value = it.value();
            if (value.isValid() && !value.canConvert<QVariantMap>())
                moduleScriptValue.setProperty(it.key(), engine->toScriptValue(value));
        }
        QScriptValue depfunc = engine->newFunction(&js_moduleDependencies);
        depfunc.setData(engine->toScriptValue(propMap.value(QLatin1String("modules"))));
        QScriptValue descriptor = engine->newObject();
        descriptor.setProperty(QLatin1String("get"), depfunc);
        descriptor.setProperty(QLatin1String("set"), engine->evaluate("(function(){})"));
        descriptor.setProperty(QLatin1String("enumerable"), true);
        engine->defineProperty(moduleScriptValue, QLatin1String("dependencies"), descriptor);
        moduleScriptValue.setProperty(QLatin1String("dependencies"), depfunc);
        moduleScriptValue.setProperty(QLatin1String("type"), QLatin1String("module"));
    }

    static void setProduct(QScriptValue scriptValue, const ResolvedProduct *product)
    {
        QVariant v;
        v.setValue<quintptr>(reinterpret_cast<quintptr>(product));
        scriptValue.setData(scriptValue.engine()->newVariant(v));
    }

    static const ResolvedProduct *getProduct(const QScriptValue &scriptValue)
    {
        const quintptr ptr = scriptValue.data().toVariant().value<quintptr>();
        return reinterpret_cast<const ResolvedProduct *>(ptr);
    }

    ScriptEngine *m_engine;
};

static void setupProductScriptValue(ScriptEngine *engine, QScriptValue &productScriptValue,
                                    const ResolvedProductConstPtr &product,
                                    ScriptPropertyObserver *observer)
{
    ModuleProperties::init(productScriptValue, product);
    DependenciesFunction(engine).init(productScriptValue, product);
    const QVariantMap &propMap = product->properties->value();
    for (QVariantMap::ConstIterator it = propMap.constBegin(); it != propMap.constEnd(); ++it) {
        const QVariant &value = it.value();
        if (value.isValid() && !value.canConvert<QVariantMap>())
            engine->setObservedProperty(productScriptValue, it.key(),
                                        engine->toScriptValue(value), observer);
    }
}

void setupScriptEngineForProduct(ScriptEngine *engine, const ResolvedProductConstPtr &product,
                                 const RuleConstPtr &rule, QScriptValue targetObject,
                                 ScriptPropertyObserver *observer)
{
    ScriptPropertyObserver *lastObserver = reinterpret_cast<ScriptPropertyObserver *>(
                engine->property("lastObserver").toULongLong());
    const ResolvedProject *lastSetupProject = 0;
    const ResolvedProduct *lastSetupProduct = 0;
    if (lastObserver == observer) {
        lastSetupProject = reinterpret_cast<ResolvedProject *>(
                    engine->property("lastSetupProject").toULongLong());
        lastSetupProduct = reinterpret_cast<ResolvedProduct *>(
                    engine->property("lastSetupProduct").toULongLong());
    } else {
        engine->setProperty("lastObserver", QVariant(reinterpret_cast<qulonglong>(observer)));
    }

    if (lastSetupProject != product->project) {
        engine->setProperty("lastSetupProject",
                QVariant(reinterpret_cast<qulonglong>(product->project.data())));
        QScriptValue projectScriptValue;
        projectScriptValue = engine->newObject();
        projectScriptValue.setProperty("filePath", product->project->location.fileName);
        projectScriptValue.setProperty("path", FileInfo::path(product->project->location.fileName));
        targetObject.setProperty("project", projectScriptValue);
    }

    QScriptValue productScriptValue;
    if (lastSetupProduct != product.data()) {
        engine->setProperty("lastSetupProduct",
                QVariant(reinterpret_cast<qulonglong>(product.data())));
        {
            QVariant v;
            v.setValue<void*>(&product->buildEnvironment);
            engine->setProperty("_qbs_procenv", v);
        }
        productScriptValue = engine->newObject();
        setupProductScriptValue(engine, productScriptValue, product, observer);
        targetObject.setProperty("product", productScriptValue);
    } else {
        productScriptValue = targetObject.property("product");
    }

    // If the Rule is in a Module, set up the 'moduleName' property
    if (!rule->module->name.isEmpty())
        productScriptValue.setProperty(QLatin1String("moduleName"), rule->module->name);

    engine->import(rule->jsImports, targetObject, targetObject);
}

bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path)
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
void connect(Artifact *p, Artifact *c)
{
    QBS_CHECK(p != c);
    p->children.insert(c);
    c->parents.insert(p);
    p->project->buildData->isDirty = true;
}

void loggedConnect(Artifact *u, Artifact *v, const Logger &logger)
{
    QBS_CHECK(u != v);
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] connect '%1' -> '%2'")
                             .arg(relativeArtifactFileName(u), relativeArtifactFileName(v));
    }
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

bool safeConnect(Artifact *u, Artifact *v, const Logger &logger)
{
    QBS_CHECK(u != v);
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] safeConnect: '%1' '%2'")
                             .arg(relativeArtifactFileName(u), relativeArtifactFileName(v));
    }

    if (existsPath(v, u)) {
        QList<Artifact *> circle;
        findPath(v, u, circle);
        logger.qbsTrace() << "[BG] safeConnect: circle detected " << toStringList(circle);
        return false;
    }

    connect(u, v);
    return true;
}

void disconnect(Artifact *u, Artifact *v, const Logger &logger)
{
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] disconnect: '%1' '%2'")
                             .arg(relativeArtifactFileName(u), relativeArtifactFileName(v));
    }
    u->children.remove(v);
    v->parents.remove(u);
}

void removeGeneratedArtifactFromDisk(Artifact *artifact, const Logger &logger)
{
    if (artifact->artifactType != Artifact::Generated)
        return;

    QFile file(artifact->filePath());
    if (!file.exists())
        return;

    logger.qbsDebug() << "removing " << artifact->fileName();
    if (!file.remove()) {
        logger.qbsWarning() << QString::fromLocal8Bit("Cannot remove '%1'.")
                               .arg(artifact->filePath());
    }
}

QString relativeArtifactFileName(const Artifact *n)
{
    const QString &buildDir = n->project->buildDirectory;
    QString str = n->filePath();
    if (str.startsWith(buildDir))
        str.remove(0, buildDir.count());
    if (str.startsWith('/'))
        str.remove(0, 1);
    return str;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &dirPath,
                         const QString &fileName)
{
    QList<Artifact *> artifacts
            = product->project->buildData->lookupArtifacts(dirPath, fileName);
    for (QList<Artifact *>::const_iterator it = artifacts.constBegin();
         it != artifacts.constEnd(); ++it) {
        if ((*it)->product == product)
            return *it;
    }
    return 0;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &filePath)
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(product, dirPath, fileName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const Artifact *artifact)
{
    return lookupArtifact(product, artifact->dirPath(), artifact->fileName());
}

Artifact *createArtifact(const ResolvedProductPtr &product,
                         const SourceArtifactConstPtr &sourceArtifact, const Logger &logger)
{
    Artifact *artifact = new Artifact(product->project);
    artifact->artifactType = Artifact::SourceFile;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->fileTags = sourceArtifact->fileTags;
    artifact->properties = sourceArtifact->properties;
    insertArtifact(product, artifact, logger);
    return artifact;
}

void insertArtifact(const ResolvedProductPtr &product, Artifact *artifact, const Logger &logger)
{
    QBS_CHECK(!artifact->product);
    QBS_CHECK(!artifact->filePath().isEmpty());
    QBS_CHECK(!product->buildData->artifacts.contains(artifact));
#ifdef QT_DEBUG
    foreach (const ResolvedProductConstPtr &otherProduct, product->project->products) {
        if (lookupArtifact(otherProduct, artifact->filePath())) {
            if (artifact->artifactType == Artifact::Generated) {
                QString pl;
                pl.append(QString("  - %1 \n").arg(product->name));
                foreach (const ResolvedProductConstPtr &p, product->project->products) {
                    if (lookupArtifact(p, artifact->filePath()))
                        pl.append(QString("  - %1 \n").arg(p->name));
                }
                throw Error(QString ("BUG: already inserted in this project: %1\n%2")
                            .arg(artifact->filePath()).arg(pl));
            }
        }
    }
#endif
    product->buildData->artifacts.insert(artifact);
    artifact->product = product;
    product->project->buildData->insertIntoArtifactLookupTable(artifact);
    product->project->buildData->isDirty = true;

    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] insert artifact '%1'")
                             .arg(artifact->filePath());
    }
}

static void dumpProductBuildDataInternal(const ResolvedProductConstPtr &product, Artifact *artifact,
                         QByteArray indent)
{
    Artifact *artifactInProduct = lookupArtifact(product, artifact->filePath());
    if (artifactInProduct && artifactInProduct != artifact) {
        qFatal("\ntree corrupted. %p ('%s') resolves to %p ('%s')\n",
                artifact,  qPrintable(artifact->filePath()), lookupArtifact(product, artifact->filePath()),
                qPrintable(lookupArtifact(product, artifact->filePath())->filePath()));

    }
    printf("%s", indent.constData());
    printf("Artifact (%p) ", artifact);
    printf("%s%s %s [%s]",
           qPrintable(QString(toString(artifact->buildState).at(0))),
           artifactInProduct ? "" : " SBS",     // SBS == side-by-side artifact from other product
           qPrintable(artifact->filePath()),
           qPrintable(artifact->fileTags.toStringList().join(QLatin1String(", "))));
    printf("\n");
    indent.append("  ");
    foreach (Artifact *child, artifact->children) {
        dumpProductBuildDataInternal(product, child, indent);
    }
}

void dumpProductBuildData(const ResolvedProductConstPtr &product)
{
    foreach (Artifact *artifact, product->buildData->artifacts)
        if (artifact->parents.isEmpty())
            dumpProductBuildDataInternal(product, artifact, QByteArray());
}


BuildGraphLoader::BuildGraphLoader(const Logger &logger) : m_logger(logger)
{
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

BuildGraphLoader::LoadResult BuildGraphLoader::load(const SetupProjectParameters &parameters,
        const RulesEvaluationContextPtr &evalContext)
{
    m_result = LoadResult();
    m_evalContext = evalContext;

    const QString projectId = ResolvedProject::deriveId(parameters.buildConfiguration);
    const QString buildDir = ResolvedProject::deriveBuildDirectory(parameters.buildRoot, projectId);
    const QString buildGraphFilePath
            = ProjectBuildData::deriveBuildGraphFilePath(buildDir, projectId);

    PersistentPool pool(m_logger);
    m_logger.qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    if (!pool.load(buildGraphFilePath))
        return m_result;
    if (!isConfigCompatible(parameters.buildConfiguration, pool.headData().projectConfig)) {
        m_logger.qbsDebug() << "[BG] Cannot use stored build graph: "
                               "Incompatible project configuration.";
        return m_result;
    }

    const ResolvedProjectPtr project = ResolvedProject::create();
    TimedActivityLogger loadLogger(m_logger, QLatin1String("Loading build graph"),
                                   QLatin1String("[BG] "));
    project->load(pool);
    project->buildData->evaluationContext = evalContext;
    foreach (Artifact * const a, project->buildData->dependencyArtifacts)
        a->project = project;
    foreach (const ResolvedProductPtr &product, project->products) {
        if (!product->buildData)
            continue;
        foreach (Artifact * const a, product->buildData->artifacts) {
            a->project = project;
            project->buildData->insertIntoArtifactLookupTable(a);
        }
    }

    if (QFileInfo(project->location.fileName) != QFileInfo(parameters.projectFilePath)) {
        QString errorMessage = Tr::tr("Stored build graph is for project file '%1', but "
                                      "input file is '%2'. ")
                .arg(QDir::toNativeSeparators(project->location.fileName),
                     QDir::toNativeSeparators(parameters.projectFilePath));
        if (!parameters.ignoreDifferentProjectFilePath) {
            errorMessage += Tr::tr("Aborting.");
            throw Error(errorMessage);
        }

        // Okay, let's assume it's the same project anyway (the source dir might have moved).
        errorMessage += Tr::tr("Ignoring.");
        m_logger.qbsWarning() << errorMessage;
    }
    foreach (const ResolvedProductPtr &p, project->products)
        p->project = project;
    project->location = CodeLocation(parameters.projectFilePath, 1, 1);
    project->setBuildConfiguration(pool.headData().projectConfig);
    project->buildDirectory = buildDir;
    m_result.loadedProject = project;
    loadLogger.finishActivity();
    trackProjectChanges(parameters, buildGraphFilePath, project);
    return m_result;
}

void BuildGraphLoader::trackProjectChanges(const SetupProjectParameters &parameters,
        const QString &buildGraphFilePath, const ResolvedProjectPtr &restoredProject)
{
    const FileInfo bgfi(buildGraphFilePath);
    const bool projectFileChanged
            = bgfi.lastModified() < FileInfo(parameters.projectFilePath).lastModified();
    if (projectFileChanged)
        m_logger.qbsTrace() << "Project file changed, must re-resolve project.";

    bool environmentChanged = false;
    for (QHash<QByteArray, QByteArray>::ConstIterator it = restoredProject->usedEnvironment.constBegin();
         !environmentChanged && it != restoredProject->usedEnvironment.constEnd(); ++it) {
        environmentChanged = qgetenv(it.key()) != it.value();
    }
    if (environmentChanged)
        m_logger.qbsTrace() << "A relevant environment variable changed, must re-resolve project.";

    bool referencedProductRemoved = false;
    QList<ResolvedProductPtr> changedProducts;
    foreach (const ResolvedProductPtr &product, restoredProject->products) {
        const FileInfo pfi(product->location.fileName);
        if (!pfi.exists()) {
            referencedProductRemoved = true;
        } else if (bgfi.lastModified() < pfi.lastModified()) {
            changedProducts += product;
        } else {
            foreach (const GroupPtr &group, product->groups) {
                if (!group->wildcards)
                    continue;
                const QSet<QString> files
                        = group->wildcards->expandPatterns(group, product->sourceDirectory);
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

    if (!environmentChanged && !projectFileChanged && !referencedProductRemoved
            && changedProducts.isEmpty()) {
        return;
    }

    Loader ldr(m_evalContext->engine(), m_logger);
    ldr.setSearchPaths(parameters.searchPaths);
    m_result.newlyResolvedProject = ldr.loadProject(parameters);

    QMap<QString, ResolvedProductPtr> freshProductsByName;
    foreach (const ResolvedProductPtr &cp, m_result.newlyResolvedProject->products)
        freshProductsByName.insert(cp->name, cp);

    QSet<TransformerPtr> seenTransformers;
    foreach (const ResolvedProductPtr &product, restoredProject->products) {
        foreach (Artifact *artifact, product->buildData->artifacts) {
            if (!artifact->transformer || seenTransformers.contains(artifact->transformer))
                continue;
            seenTransformers.insert(artifact->transformer);
            ResolvedProductPtr freshProduct = freshProductsByName.value(product->name);
            if (freshProduct && checkForPropertyChanges(artifact->transformer, freshProduct)) {
                m_result.discardLoadedProject = true;
                return;
            }
        }
    }

    foreach (const ResolvedProductPtr &product, changedProducts) {
        ResolvedProductPtr freshProduct = freshProductsByName.value(product->name);
        if (!freshProduct)
            continue;
        onProductChanged(product, freshProduct);
        if (m_result.discardLoadedProject)
            return;
    }

    QSet<QString> oldProductNames, newProductNames;
    foreach (const ResolvedProductConstPtr &product, restoredProject->products)
        oldProductNames += product->name;
    foreach (const ResolvedProductConstPtr &product, m_result.newlyResolvedProject->products)
        newProductNames += product->name;

    const QSet<QString> removedProductsNames = oldProductNames - newProductNames;
    if (!removedProductsNames.isEmpty()) {
        foreach (const ResolvedProductPtr &product, restoredProject->products) {
            if (removedProductsNames.contains(product->name))
                onProductRemoved(product);
        }
    }

    const QSet<QString> addedProductNames = newProductNames - oldProductNames;
    QList<ResolvedProductPtr> addedProducts;
    foreach (const QString &productName, addedProductNames) {
        const ResolvedProductPtr &freshProduct = freshProductsByName.value(productName);
        QBS_ASSERT(freshProduct, continue);
        addedProducts.append(freshProduct);
    }
    if (!addedProducts.isEmpty()) {
        foreach (const ResolvedProductPtr &product, addedProducts) {
            product->project = restoredProject;
            restoredProject->products.append(product);
        }
        BuildDataResolver bpr(m_logger);
        bpr.resolveProductBuildDataForExistingProject(restoredProject, addedProducts);
    }

    CycleDetector(m_logger).visitProject(restoredProject);
}

void BuildGraphLoader::onProductRemoved(const ResolvedProductPtr &product)
{
    m_logger.qbsDebug() << "[BG] product '" << product->name << "' removed.";

    product->project->buildData->isDirty = true;
    product->project->products.removeOne(product);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, product->buildData->artifacts) {
        artifact->disconnectAll(m_logger);
        removeGeneratedArtifactFromDisk(artifact, m_logger);
    }
}

void BuildGraphLoader::onProductChanged(const ResolvedProductPtr &product,
                                        const ResolvedProductPtr &changedProduct)
{
    m_logger.qbsDebug() << "[BG] product '" << product->name << "' changed.";

    ArtifactsPerFileTagMap artifactsPerFileTag;
    QList<Artifact *> addedArtifacts, artifactsToRemove;
    QHash<QString, SourceArtifactConstPtr> oldArtifacts, newArtifacts;

    const QList<SourceArtifactPtr> oldProductAllFiles = product->allEnabledFiles();
    foreach (const SourceArtifactConstPtr &a, oldProductAllFiles)
        oldArtifacts.insert(a->absoluteFilePath, a);
    foreach (const SourceArtifactPtr &a, changedProduct->allEnabledFiles()) {
        newArtifacts.insert(a->absoluteFilePath, a);
        if (!oldArtifacts.contains(a->absoluteFilePath)) {
            // artifact added
            m_logger.qbsDebug() << "[BG] artifact '" << a->absoluteFilePath
                                << "' added to product " << product->name;
            addedArtifacts += createArtifact(product, a, m_logger);
        }
    }

    foreach (const SourceArtifactPtr &a, oldProductAllFiles) {
        const SourceArtifactConstPtr changedArtifact = newArtifacts.value(a->absoluteFilePath);
        if (!changedArtifact) {
            // artifact removed
            m_logger.qbsDebug() << "[BG] artifact '" << a->absoluteFilePath
                                << "' removed from product " << product->name;
            Artifact *artifact = lookupArtifact(product, a->absoluteFilePath);
            QBS_CHECK(artifact);
            removeArtifactAndExclusiveDependents(artifact, &artifactsToRemove);
            continue;
        }
        if (changedArtifact->fileTags != a->fileTags) {
            // artifact's filetags have changed
            m_logger.qbsDebug() << "[BG] filetags have changed for artifact '"
                    << a->absoluteFilePath << "' from " << a->fileTags << " to "
                    << changedArtifact->fileTags;
            Artifact *artifact = lookupArtifact(product, a->absoluteFilePath);
            QBS_CHECK(artifact);

            // handle added filetags
            foreach (const FileTag &addedFileTag, changedArtifact->fileTags - a->fileTags)
                artifactsPerFileTag[addedFileTag] += artifact;

            // handle removed filetags
            foreach (const FileTag &removedFileTag, a->fileTags - changedArtifact->fileTags) {
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
    product->groups = changedProduct->groups;
    product->properties = changedProduct->properties;

    // apply rules for new artifacts
    foreach (Artifact *artifact, addedArtifacts)
        foreach (const FileTag &ft, artifact->fileTags)
            artifactsPerFileTag[ft] += artifact;
    RulesApplicator(product, artifactsPerFileTag, m_logger).applyAllRules();

    addTargetArtifacts(product, artifactsPerFileTag, m_logger);

    // parents of removed artifacts must update their transformers
    foreach (Artifact *removedArtifact, artifactsToRemove)
        foreach (Artifact *parent, removedArtifact->parents)
            product->project->buildData->artifactsThatMustGetNewTransformers += parent;
    product->project->buildData->updateNodesThatMustGetNewTransformer(m_logger);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, artifactsToRemove) {
        removeGeneratedArtifactFromDisk(artifact, m_logger);
        delete artifact;
    }
}

/**
  * Removes the artifact and all the artifacts that depend exclusively on it.
  * Example: if you remove a cpp artifact then the obj artifact is removed but
  * not the resulting application (if there's more then one cpp artifact).
  */
void BuildGraphLoader::removeArtifactAndExclusiveDependents(Artifact *artifact,
                                                            QList<Artifact*> *removedArtifacts)
{
    if (removedArtifacts)
        removedArtifacts->append(artifact);
    foreach (Artifact *parent, artifact->parents) {
        bool removeParent = false;
        disconnect(parent, artifact, m_logger);
        if (parent->children.isEmpty()) {
            removeParent = true;
        } else if (parent->transformer) {
            artifact->project->buildData->artifactsThatMustGetNewTransformers += parent;
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.isEmpty();
        }
        if (removeParent)
            removeArtifactAndExclusiveDependents(parent, removedArtifacts);
    }
    artifact->project->buildData->removeArtifact(artifact, m_logger);
}

bool BuildGraphLoader::checkForPropertyChanges(const TransformerPtr &restoredTrafo,
                                               const ResolvedProductPtr &freshProduct)
{
    PropertyFinder finder;
    foreach (const Property &property, restoredTrafo->modulePropertiesUsedInPrepareScript) {
        QVariant v;
        if (property.value.type() == QVariant::List) {
            v = finder.propertyValues(freshProduct->properties->value(), property.moduleName,
                                      property.propertyName);
        } else {
            v = finder.propertyValue(freshProduct->properties->value(), property.moduleName,
                                     property.propertyName);
        }
        if (property.value != v)
            return true;
    }
    return false;
}

void addTargetArtifacts(const ResolvedProductPtr &product,
                        ArtifactsPerFileTagMap &artifactsPerFileTag, const Logger &logger)
{
    foreach (const FileTag &fileTag, product->fileTags) {
        foreach (Artifact * const artifact, artifactsPerFileTag.value(fileTag)) {
            if (artifact->artifactType == Artifact::Generated)
                product->buildData->targetArtifacts += artifact;
        }
    }
    if (product->buildData->targetArtifacts.isEmpty()) {
        const QString msg = QString::fromLocal8Bit("No artifacts generated for product '%1'.");
        logger.qbsDebug() << msg.arg(product->name);
    }
}

} // namespace Internal
} // namespace qbs
