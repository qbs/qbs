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

#include "qbsengine.h"

#include "publicobjectsmap.h"

#include <buildgraph/artifact.h>
#include <buildgraph/artifactcleaner.h>
#include <buildgraph/executor.h>
#include <language/scriptengine.h>
#include <language/loader.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/platform.h>
#include <tools/runenvironment.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/settings.h>

#include <QEventLoop>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>

namespace qbs {
using namespace Internal;

class QbsEngine::QbsEnginePrivate
{
public:
    QbsEnginePrivate()
        : observer(0), buildGraph(new BuildGraph(&engine)) {}

    void loadPlugins();
    BuildProject::Ptr setupBuildProject(const ResolvedProject::ConstPtr &project);
    QVariantMap expandedBuildConfiguration(const QVariantMap &userBuildConfig);
    void buildProducts(const QList<BuildProduct::Ptr> &buildProducts,
                       const BuildOptions &buildOptions);
    void buildProducts(const QList<ResolvedProduct::ConstPtr> &products,
                       const BuildOptions &buildOptions, bool needsDepencencyResolving);
    void cleanBuildProducts(const QList<BuildProduct::Ptr> &buildProducts,
                            const BuildOptions &buildOptions, QbsEngine::CleanType cleanType);

    void storeBuildGraphs(const QList<BuildProduct::Ptr> &buildProducts,
                          const BuildOptions &buildOptions);

    ScriptEngine engine;
    ProgressObserver *observer;
    QList<ResolvedProject::Ptr> resolvedProjects;
    QList<BuildProject::Ptr> buildProjects;
    Settings settings;
    const QSharedPointer<BuildGraph> buildGraph;
    PublicObjectsMap publicObjectsMap;

    // Potentially stored temporarily between calls to setupResolvedProject() and setupBuildProject().
    QHash<ResolvedProject::ConstPtr, BuildProject::Ptr> discardedBuildProjects;

private:
    QVariantMap createBuildConfiguration(const QVariantMap &userBuildConfig);

    // First: Unexpanded config (coming in via the API), second: fully expanded config
    typedef QPair<QVariantMap, QVariantMap> BuildConfigPair;
    QList<BuildConfigPair> m_buildConfigurations;

    static bool pluginsLoaded;
    static QMutex pluginsLoadedMutex;
};

bool QbsEngine::QbsEnginePrivate::pluginsLoaded = false;
QMutex QbsEngine::QbsEnginePrivate::pluginsLoadedMutex;

QbsEngine::QbsEngine() : d(new QbsEnginePrivate)
{
    d->loadPlugins();
}

QbsEngine::~QbsEngine()
{
    delete d;
}

void QbsEngine::setProgressObserver(ProgressObserver *observer)
{
    d->observer = observer;
    d->buildGraph->setProgressObserver(observer);
}

Project::Id QbsEngine::setupProject(const QString &projectFileName, const QVariantMap &_buildConfig,
                                    const QString &buildRoot)
{
    const QVariantMap buildConfig = d->expandedBuildConfiguration(_buildConfig);
    const BuildProject::LoadResult loadResult = BuildProject::load(projectFileName,
            d->buildGraph.data(), buildRoot, buildConfig, d->settings.searchPaths());

    BuildProject::Ptr bProject;
    ResolvedProject::Ptr rProject;
    if (!loadResult.discardLoadedProject)
        bProject = loadResult.loadedProject;
    if (bProject) {
        d->buildProjects << bProject;
        rProject = bProject->resolvedProject();
    } else {
        if (loadResult.changedResolvedProject) {
            rProject = loadResult.changedResolvedProject;
        } else {
            Loader loader(&d->engine);
            loader.setSearchPaths(d->settings.searchPaths());
            loader.setProgressObserver(d->observer);
            rProject = loader.loadProject(projectFileName, buildRoot, buildConfig);
        }
        if (rProject->products.isEmpty())
            throw Error(QString("'%1' does not contain products.").arg(projectFileName));
        if (loadResult.loadedProject)
            d->discardedBuildProjects.insert(rProject, loadResult.loadedProject);
    }

    // copy the environment from the platform config into the project's config
    const QVariantMap platformEnvironment = buildConfig.value("environment").toMap();
    rProject->platformEnvironment = platformEnvironment;

    qbsDebug("for %s:", qPrintable(rProject->id()));
    foreach (const ResolvedProduct::ConstPtr &p, rProject->products) {
        qbsDebug("  - [%s] %s as %s"
                 ,qPrintable(p->fileTags.join(", "))
                 ,qPrintable(p->name)
                 ,qPrintable(p->project->id())
                 );
    }
    qbsDebug("");

    d->resolvedProjects << rProject;
    const Project::Id id(quintptr(rProject.data()));
    d->publicObjectsMap.insertProject(id, rProject);
    return id;
}

void QbsEngine::buildProjects(const QList<Project::Id> &projectIds, const BuildOptions &buildOptions)
{
    QList<ResolvedProduct::ConstPtr> products;
    foreach (const Project::Id projectId, projectIds) {
        const ResolvedProject::ConstPtr resolvedProject = d->publicObjectsMap.project(projectId);
        if (!resolvedProject)
            throw Error(Tr::tr("Cannot build: No such project."));
        foreach (const ResolvedProduct::ConstPtr &product, resolvedProject->products)
            products << product;
    }
    d->buildProducts(products, buildOptions, false);
}

static QList<Project::Id> projectListToIdList(const QList<Project> &projects)
{
    QList<Project::Id> projectIds;
    foreach (const Project &project, projects)
        projectIds << project.id();
    return projectIds;
}

void QbsEngine::buildProjects(const QList<Project> &projects, const BuildOptions &buildOptions)
{
    buildProjects(projectListToIdList(projects), buildOptions);
}

void QbsEngine::buildProducts(const QList<Product> &products, const BuildOptions &buildOptions)
{
    QList<ResolvedProduct::ConstPtr> resolvedProducts;
    foreach (const Product &product, products) {
        const ResolvedProduct::ConstPtr resolvedProduct = d->publicObjectsMap.product(product.id());
        if (!resolvedProduct)
            throw Error(Tr::tr("Cannot build: No such product."));
        resolvedProducts << resolvedProduct;
    }
    d->buildProducts(resolvedProducts, buildOptions, true);
}

void QbsEngine::cleanProjects(const QList<Project::Id> &projectIds,
                              const BuildOptions &buildOptions, CleanType cleanType)
{
    QList<BuildProduct::Ptr> products;
    foreach (const Project::Id id, projectIds) {
        const ResolvedProject::ConstPtr rProject = d->publicObjectsMap.project(id);
        if (!rProject)
            throw Error(Tr::tr("Cleaning up failed: Project not found."));
        const BuildProject::ConstPtr bProject = d->setupBuildProject(rProject);
        foreach (const BuildProduct::Ptr &product, bProject->buildProducts())
            products << product;
    }
    d->cleanBuildProducts(products, buildOptions, cleanType);
}

void QbsEngine::cleanProjects(const QList<Project> &projects, const BuildOptions &buildOptions,
                              CleanType cleanType)
{
    cleanProjects(projectListToIdList(projects), buildOptions, cleanType);
}

void QbsEngine::cleanProducts(const QList<Product> &products, const BuildOptions &buildOptions,
                              QbsEngine::CleanType cleanType)
{
    QList<BuildProduct::Ptr> buildProducts;
    foreach (const Product &product, products) {
        const ResolvedProduct::ConstPtr rProduct = d->publicObjectsMap.product(product.id());
        if (!rProduct)
            throw Error(Tr::tr("Cleaning up failed: Product not found."));
        const ResolvedProject::ConstPtr rProject = rProduct->project.toStrongRef();
        const BuildProject::ConstPtr bProject = d->setupBuildProject(rProject);
        foreach (const BuildProduct::Ptr &bProduct, bProject->buildProducts()) {
            if (bProduct->rProduct == rProduct) {
                buildProducts << bProduct;
                break;
            }
        }
    }
    d->cleanBuildProducts(buildProducts, buildOptions, cleanType);
}

Project QbsEngine::retrieveProject(Project::Id projectId) const
{
    const ResolvedProject::ConstPtr resolvedProject = d->publicObjectsMap.project(projectId);
    if (!resolvedProject)
        throw Error(Tr::tr("Cannot retrieve project: No such project."));
    Project project(projectId);
    project.m_qbsFilePath = resolvedProject->qbsFile;
    foreach (const ResolvedProduct::Ptr &resolvedProduct, resolvedProject->products) {
        Product product(Product::Id(quintptr(resolvedProduct.data())));
        product.m_name = resolvedProduct->name;
        product.m_qbsFilePath = resolvedProduct->qbsFile;
        product.m_fileTags = resolvedProduct->fileTags;
        foreach (const ResolvedGroup::Ptr &resolvedGroup, resolvedProduct->groups) {
            Group group(Group::Id(quintptr(resolvedGroup.data())));
            group.m_name = resolvedGroup->name;
            foreach (const SourceArtifact::ConstPtr &sa, resolvedGroup->files)
                group.m_filePaths << sa->absoluteFilePath;
            if (resolvedGroup->wildcards) {
                foreach (const SourceArtifact::ConstPtr &sa, resolvedGroup->wildcards->files)
                    group.m_expandedWildcards << sa->absoluteFilePath;
            }
            group.m_properties = resolvedGroup->properties->value();
            product.m_groups << group;
            d->publicObjectsMap.insertGroup(group.m_id, resolvedGroup);
        }
        project.m_products << product;
        d->publicObjectsMap.insertProduct(product.m_id, resolvedProduct);
    }
    return project;
}

RunEnvironment QbsEngine::getRunEnvironment(const Product &product,
                                            const QProcessEnvironment &environment)
{
    return RunEnvironment(&d->engine, product, d->publicObjectsMap, environment);
}

QString QbsEngine::targetExecutable(const Product &product)
{
    const ResolvedProduct::ConstPtr resolvedProduct = d->publicObjectsMap.product(product.id());
    if (!resolvedProduct)
        throw Error(Tr::tr("Unknown product."));
    if (!resolvedProduct->fileTags.contains(QLatin1String("application")))
        return QString();
    ResolvedProject::ConstPtr resolvedProject;
    foreach (const ResolvedProject::ConstPtr &p, d->resolvedProjects) {
        if (p == resolvedProduct->project) {
            resolvedProject = p;
            break;
        }
    }
    if (!resolvedProject)
        throw Error(Tr::tr("Unknown product '%1'.").arg(resolvedProduct->name));

    const BuildProject::ConstPtr buildProject = d->setupBuildProject(resolvedProject);
    Q_ASSERT(buildProject->resolvedProject() == resolvedProject);
    BuildProduct::ConstPtr buildProduct;
    foreach (const BuildProduct::ConstPtr &bp, buildProject->buildProducts()) {
        if (bp->rProduct == resolvedProduct) {
            buildProduct = bp;
            break;
        }
    }
    Q_ASSERT(buildProduct);

    foreach (const Artifact * const artifact, buildProduct->targetArtifacts) {
        if (artifact->fileTags.contains(QLatin1String("application")))
            return artifact->filePath();
    }
    return QString();
}

void QbsEngine::QbsEnginePrivate::loadPlugins()
{
    QMutexLocker locker(&pluginsLoadedMutex);
    if (pluginsLoaded)
        return;

    QStringList pluginPaths;
    foreach (const QString &pluginPath, settings.pluginPaths()) {
        if (!FileInfo::exists(pluginPath)) {
            qbsWarning() << Tr::tr("Plugin path '%1' does not exist.")
                    .arg(QDir::toNativeSeparators(pluginPath));
        } else {
            pluginPaths << pluginPath;
        }
    }
    ScannerPluginManager::instance()->loadPlugins(pluginPaths);

    pluginsLoaded = true;
}

BuildProject::Ptr QbsEngine::QbsEnginePrivate::setupBuildProject(const ResolvedProject::ConstPtr &project)
{
    foreach (const BuildProject::Ptr &buildProject, buildProjects) {
        if (buildProject->resolvedProject() == project)
            return buildProject;
    }

    ResolvedProject::Ptr mutableRProject;
    foreach (const ResolvedProject::Ptr &p, resolvedProjects) {
        if (p == project) {
            mutableRProject = p;
            break;
        }
    }
    if (!mutableRProject)
        throw Error(Tr::tr("Unknown project."));

    TimedActivityLogger resolveLogger(QLatin1String("Resolving build project"));
    const BuildProject::Ptr buildProject = buildGraph->resolveProject(mutableRProject);
    const QHash<ResolvedProject::ConstPtr, BuildProject::Ptr>::Iterator it
            = discardedBuildProjects.find(project);
    if (it != discardedBuildProjects.end()) {
        buildProject->rescueDependencies(it.value());
        discardedBuildProjects.erase(it);
    }

    buildProjects << buildProject;
    return buildProject;
}

QVariantMap QbsEngine::QbsEnginePrivate::expandedBuildConfiguration(const QVariantMap &userBuildConfig)
{
    foreach (const QbsEnginePrivate::BuildConfigPair &configPair, m_buildConfigurations) {
        if (configPair.first == userBuildConfig)
            return configPair.second;
    }
    const QbsEnginePrivate::BuildConfigPair configPair
            = qMakePair(userBuildConfig, createBuildConfiguration(userBuildConfig));
    m_buildConfigurations << configPair;
    return configPair.second;
}

void QbsEngine::QbsEnginePrivate::buildProducts(const QList<BuildProduct::Ptr> &buildProducts,
        const BuildOptions &buildOptions)
{
    Executor executor;
    QEventLoop execLoop;
    QObject::connect(&executor, SIGNAL(finished()), &execLoop, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(&executor, SIGNAL(error()), &execLoop, SLOT(quit()), Qt::QueuedConnection);
    executor.setEngine(&engine);
    executor.setProgressObserver(observer);
    executor.setBuildOptions(buildOptions);
    TimedActivityLogger buildLogger(QLatin1String("Building products"), QString(), LoggerInfo);
    executor.build(buildProducts);
    execLoop.exec();
    buildLogger.finishActivity();
    storeBuildGraphs(buildProducts, buildOptions);
    if (executor.buildResult() != Executor::SuccessfulBuild)
        throw Error(Tr::tr("Build failed."));
}

void QbsEngine::QbsEnginePrivate::buildProducts(const QList<ResolvedProduct::ConstPtr> &products,
        const BuildOptions &buildOptions, bool needsDepencencyResolving)
{
    // Make sure all products are set up first.
    QSet<ResolvedProject::ConstPtr> rProjects;
    foreach (const ResolvedProduct::ConstPtr &product, products)
        rProjects << product->project.toStrongRef();
    foreach (const ResolvedProject::ConstPtr &rProject, rProjects)
        setupBuildProject(rProject);

    // Gather build products.
    QList<BuildProduct::Ptr> productsToBuild;
    foreach (const ResolvedProduct::ConstPtr &rProduct, products) {
        foreach (const BuildProject::ConstPtr &buildProject, buildProjects) {
            foreach (const BuildProduct::Ptr &buildProduct, buildProject->buildProducts()) {
                if (buildProduct->rProduct == rProduct)
                    productsToBuild << buildProduct;
            }
        }
    }

    if (needsDepencencyResolving) {
        for (int i = 0; i < productsToBuild.count(); ++i) {
            const BuildProduct::ConstPtr &product = productsToBuild.at(i);
            foreach (const BuildProduct::Ptr &dependency, product->dependencies)
                productsToBuild << dependency;
        }
    }

    buildProducts(productsToBuild, buildOptions);
}

void QbsEngine::QbsEnginePrivate::cleanBuildProducts(const QList<BuildProduct::Ptr> &buildProducts,
        const BuildOptions &buildOptions, QbsEngine::CleanType cleanType)
{
    try {
        ArtifactCleaner cleaner;
        cleaner.cleanup(buildProducts, cleanType == QbsEngine::CleanupAll, buildOptions);
    } catch (const Error &error) {
        storeBuildGraphs(buildProducts, buildOptions);
        throw;
    }
    storeBuildGraphs(buildProducts, buildOptions);
}

void QbsEngine::QbsEnginePrivate::storeBuildGraphs(const QList<BuildProduct::Ptr> &buildProducts,
                                                   const BuildOptions &buildOptions)
{
    if (buildOptions.dryRun)
        return;
    QSet<BuildProject *> buildProjects;
    foreach (const BuildProduct::ConstPtr &bProduct, buildProducts)
        buildProjects += bProduct->project;
    foreach (const BuildProject * const bProject, buildProjects)
        bProject->store();
}

QVariantMap QbsEngine::QbsEnginePrivate::createBuildConfiguration(const QVariantMap &userBuildConfig)
{
    QHash<QString, Platform::Ptr > platforms = Platform::platforms();
    if (platforms.isEmpty())
        throw Error(Tr::tr("No platforms configured. You must run 'qbs platforms probe' first."));

    QVariantMap expandedConfig = userBuildConfig;

    // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
    // 1) Things specified on command line (already in buildCfg at this point)
    // 2) Everything from the profile key (in reverse order)
    // 3) Everything from the platform
    // 4) Any remaining keys from modules keyspace
    QString profileName = expandedConfig.value("qbs.profile").toString();
    if (profileName.isNull()) {
        profileName = settings.value("profile").toString();
        if (profileName.isNull())
            throw Error(Tr::tr("No profile given.\n"
                           "Either set the configuration value 'profile' to a valid profile's name\n"
                           "or specify the profile with the command line parameter 'profile:name'."));
        expandedConfig.insert("qbs.profile", profileName);
    }

    // (2)
    const QString profileGroup = QString("profiles/%1").arg(profileName);
    const QStringList profileKeys = settings.allKeysWithPrefix(profileGroup);
    if (profileKeys.isEmpty())
        throw Error(Tr::tr("Unknown profile '%1'.").arg(profileName));
    foreach (const QString &profileKey, profileKeys) {
        QString fixedKey(profileKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, settings.value(profileGroup + "/" + profileKey));
    }

    // (3) Need to make sure we have a value for qbs.platform before going any further
    QVariant platformName = expandedConfig.value("qbs.platform");
    if (!platformName.isValid()) {
        platformName = settings.moduleValue("qbs/platform", profileName);
        if (!platformName.isValid())
            throw Error(Tr::tr("No platform given and no default set."));
        expandedConfig.insert("qbs.platform", platformName);
    }
    Platform::Ptr platform = platforms.value(platformName.toString());
    if (platform.isNull())
        throw Error(Tr::tr("Unknown platform '%1'.").arg(platformName.toString()));
    foreach (const QString &key, platform->settings.allKeys()) {
        if (key.startsWith(Platform::internalKey()))
            continue;
        QString fixedKey = key;
        int idx = fixedKey.lastIndexOf(QChar('/'));
        if (idx > 0)
            fixedKey[idx] = QChar('.');
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, platform->settings.value(key));
    }
    // Now finally do (4)
    foreach (const QString &defaultKey, settings.allKeysWithPrefix("modules")) {
        QString fixedKey(defaultKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, settings.value(QString("modules/") + defaultKey));
    }

    if (!expandedConfig.value("qbs.buildVariant").isValid())
        throw Error(Tr::tr("Property 'qbs.buildVariant' missing in build configuration."));

    foreach (const QString &property, expandedConfig.keys()) {
        QStringList nameElements = property.split('.');
        if (nameElements.count() == 1 && nameElements.first() != "project") // ### Still need this because platform doesn't supply fully-qualified properties (yet), need to fix this
            nameElements.prepend("qbs");
        if (nameElements.count() > 2) { // ### workaround for submodules being represented internally as a single module of name "module/submodule" rather than two nested modules "module" and "submodule"
            QStringList newElements(QStringList(nameElements.mid(0, nameElements.count()-1)).join("/"));
            newElements.append(nameElements.last());
            nameElements = newElements;
        }
        setConfigProperty(expandedConfig, nameElements, expandedConfig.value(property));
        expandedConfig.remove(property);
    }

    return expandedConfig;
}

} // namespace qbs
