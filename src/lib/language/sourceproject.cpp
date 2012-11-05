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

#include "sourceproject.h"

#include <buildgraph/artifact.h>
#include <buildgraph/executor.h>
#include <language/qbsengine.h>
#include <language/loader.h>
#include <logging/logger.h>
#include <tools/runenvironment.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/settings.h>
#include <tools/platform.h>

#include <QEventLoop>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>

namespace qbs {

class SourceProjectPrivate : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(SourceProjectPrivate)
public:
    SourceProjectPrivate()
        : observer(0), settings(Settings::create()), buildGraph(new BuildGraph(&engine)) {}

    void loadPlugins();
    BuildProject::Ptr setupBuildProject(const ResolvedProject::ConstPtr &project);
    QVariantMap expandedBuildConfiguration(const QVariantMap &userBuildConfig);
    void buildProducts(const QList<BuildProduct::Ptr> &buildProducts,
                       const BuildOptions &buildOptions);
    void buildProducts(const QList<ResolvedProduct::ConstPtr> &products,
                       const BuildOptions &buildOptions, bool needsDepencencyResolving);

    QbsEngine engine;
    ProgressObserver *observer;
    QList<ResolvedProject::Ptr> resolvedProjects;
    QList<BuildProject::Ptr> buildProjects;
    const Settings::Ptr settings;
    const QSharedPointer<BuildGraph> buildGraph;

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

bool SourceProjectPrivate::pluginsLoaded = false;
QMutex SourceProjectPrivate::pluginsLoadedMutex;

SourceProject::SourceProject() : d(new SourceProjectPrivate)
{
    d->loadPlugins();
}

SourceProject::~SourceProject()
{
    delete d;
}

void SourceProject::setProgressObserver(ProgressObserver *observer)
{
    d->observer = observer;
    d->buildGraph->setProgressObserver(observer);
}

void SourceProject::setBuildRoot(const QString &directory)
{
    d->buildGraph->setOutputDirectoryRoot(directory);
}

ResolvedProject::ConstPtr SourceProject::setupResolvedProject(const QString &projectFileName,
                                                         const QVariantMap &_buildConfig)
{
    Loader loader(&d->engine);
    loader.setSearchPaths(d->settings->searchPaths());
    loader.setProgressObserver(d->observer);

    ProjectFile::Ptr projectFile;
    const QVariantMap buildConfig = d->expandedBuildConfiguration(_buildConfig);
    const FileTime projectFileTimeStamp = FileInfo(projectFileName).lastModified();
    const BuildProject::LoadResult loadResult = BuildProject::load(projectFileName,
            d->buildGraph.data(), projectFileTimeStamp, buildConfig, d->settings->searchPaths());

    BuildProject::Ptr bProject;
    ResolvedProject::Ptr rProject;
    if (!loadResult.discardLoadedProject)
        bProject = loadResult.loadedProject;
    if (bProject) {
        d->buildProjects << bProject;
        rProject = bProject->resolvedProject();
    } else {
        TimedActivityLogger loadLogger(QLatin1String("Loading project"));
        projectFile = loader.loadProject(projectFileName);
        if (loadResult.changedResolvedProject) {
            rProject = loadResult.changedResolvedProject;
        } else {
            const QString buildDir = d->buildGraph->buildDirectory(
                        ResolvedProject::deriveId(buildConfig));
            rProject = loader.resolveProject(projectFile, buildDir, buildConfig);
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
    return rProject;
}

void SourceProject::buildProjects(const QList<ResolvedProject::ConstPtr> &projects,
                                  const BuildOptions &buildOptions)
{
    QList<ResolvedProduct::ConstPtr> products;
    foreach (const ResolvedProject::ConstPtr &project, projects) {
        foreach (const ResolvedProduct::ConstPtr &product, project->products)
            products << product;
    }
    d->buildProducts(products, buildOptions, false);
}

void SourceProject::buildProducts(const QList<ResolvedProduct::ConstPtr> &products,
                                  const BuildOptions &buildOptions)
{
    d->buildProducts(products, buildOptions, true);
}

RunEnvironment SourceProject::getRunEnvironment(const ResolvedProduct::ConstPtr &product,
                                                const QProcessEnvironment &environment)
{
    foreach (const ResolvedProject::ConstPtr &rProject, d->resolvedProjects) {
        if (product->project != rProject)
            continue;
        foreach (const ResolvedProduct::Ptr &p, rProject->products) {
            if (p == product)
                return RunEnvironment(&d->engine, p, environment);
        }
    }
    throw Error(tr("Unknown product"));
}

QString SourceProject::targetExecutable(const ResolvedProduct::ConstPtr &product)
{
    Q_ASSERT(product);

    if (!product->fileTags.contains(QLatin1String("application")))
        return QString();
    ResolvedProject::ConstPtr resolvedProject;
    foreach (const ResolvedProject::ConstPtr &p, d->resolvedProjects) {
        if (p == product->project) {
            resolvedProject = p;
            break;
        }
    }
    if (!resolvedProject)
        throw Error(tr("Unknown product '%1'.").arg(product->name));

    const BuildProject::ConstPtr buildProject = d->setupBuildProject(resolvedProject);
    Q_ASSERT(buildProject->resolvedProject() == resolvedProject);
    BuildProduct::ConstPtr buildProduct;
    foreach (const BuildProduct::ConstPtr &bp, buildProject->buildProducts()) {
        if (bp->rProduct == product) {
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

void SourceProjectPrivate::loadPlugins()
{
    QMutexLocker locker(&pluginsLoadedMutex);
    if (pluginsLoaded)
        return;

    QStringList pluginPaths;
    foreach (const QString &pluginPath, settings->pluginPaths()) {
        if (!FileInfo::exists(pluginPath)) {
            qbsWarning() << tr("Plugin path '%1' does not exist.")
                    .arg(QDir::toNativeSeparators(pluginPath));
        } else {
            pluginPaths << pluginPath;
        }
    }
    foreach (const QString &pluginPath, pluginPaths)
        QCoreApplication::addLibraryPath(pluginPath); // TODO: Probably wrong to do it like this and possibly not needed at all.
    ScannerPluginManager::instance()->loadPlugins(pluginPaths);

    pluginsLoaded = true;
}

BuildProject::Ptr SourceProjectPrivate::setupBuildProject(const ResolvedProject::ConstPtr &project)
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
        throw Error(tr("Unknown project."));

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

QVariantMap SourceProjectPrivate::expandedBuildConfiguration(const QVariantMap &userBuildConfig)
{
    foreach (const SourceProjectPrivate::BuildConfigPair &configPair, m_buildConfigurations) {
        if (configPair.first == userBuildConfig)
            return configPair.second;
    }
    const SourceProjectPrivate::BuildConfigPair configPair
            = qMakePair(userBuildConfig, createBuildConfiguration(userBuildConfig));
    m_buildConfigurations << configPair;
    return configPair.second;
}

void SourceProjectPrivate::buildProducts(const QList<BuildProduct::Ptr> &buildProducts,
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
    if (executor.state() != Executor::ExecutorError) {
        foreach (const BuildProject::ConstPtr &buildProject, buildProjects)
            buildProject->store();
    }
    if (executor.buildResult() != Executor::SuccessfulBuild)
        throw Error(tr("Build failed."));
}

void SourceProjectPrivate::buildProducts(const QList<ResolvedProduct::ConstPtr> &products,
        const BuildOptions &buildOptions, bool needsDepencencyResolving)
{

    // Make sure all products are set up first.
    QSet<const ResolvedProject *> rProjects;
    foreach (const ResolvedProduct::ConstPtr &product, products)
        rProjects << product->project;
    foreach (const ResolvedProject *rproject, rProjects) {

        // TODO: This awful loop is there because we don't use weak pointers. Change that.
        foreach (const ResolvedProject::ConstPtr &rProjectSp, resolvedProjects) {
            if (rproject == rProjectSp) {
                setupBuildProject(rProjectSp);
                break;
            }
        }
    }

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
            foreach (BuildProduct * const dependency, product->usings) {

                // TODO: This awful loop is there because we don't use weak pointers. Change that.
                BuildProduct::Ptr dependencySP;
                foreach (const BuildProduct::Ptr &bp, productsToBuild) {
                    if (bp == dependency) {
                        dependencySP = bp;
                        break;
                    }
                }

                if (dependencySP)
                    productsToBuild << dependencySP;
            }
        }
    }

    buildProducts(productsToBuild, buildOptions);
}

QVariantMap SourceProjectPrivate::createBuildConfiguration(const QVariantMap &userBuildConfig)
{
    QHash<QString, Platform::Ptr > platforms = Platform::platforms();
    if (platforms.isEmpty())
        throw Error(tr("No platforms configured. You must run 'qbs platforms probe' first."));

    QVariantMap expandedConfig = userBuildConfig;

    // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
    // 1) Things specified on command line (already in buildCfg at this point)
    // 2) Everything from the profile key (in reverse order)
    // 3) Everything from the platform
    // 4) Any remaining keys from modules keyspace
    QString profileName = expandedConfig.value("qbs.profile").toString();
    if (profileName.isNull()) {
        profileName = settings->value("profile").toString();
        if (profileName.isNull())
            throw Error(tr("No profile given.\n"
                           "Either set the configuration value 'profile' to a valid profile's name\n"
                           "or specify the profile with the command line parameter 'profile:name'."));
        expandedConfig.insert("qbs.profile", profileName);
    }

    // (2)
    const QString profileGroup = QString("profiles/%1").arg(profileName);
    const QStringList profileKeys = settings->allKeysWithPrefix(profileGroup);
    if (profileKeys.isEmpty())
        throw Error(tr("Unknown profile '%1'.").arg(profileName));
    foreach (const QString &profileKey, profileKeys) {
        QString fixedKey(profileKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, settings->value(profileGroup + "/" + profileKey));
    }

    // (3) Need to make sure we have a value for qbs.platform before going any further
    QVariant platformName = expandedConfig.value("qbs.platform");
    if (!platformName.isValid()) {
        platformName = settings->moduleValue("qbs/platform", profileName);
        if (!platformName.isValid())
            throw Error(tr("No platform given and no default set."));
        expandedConfig.insert("qbs.platform", platformName);
    }
    Platform::Ptr platform = platforms.value(platformName.toString());
    if (platform.isNull())
        throw Error(tr("Unknown platform '%1'.").arg(platformName.toString()));
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
    foreach (const QString &defaultKey, settings->allKeysWithPrefix("modules")) {
        QString fixedKey(defaultKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, settings->value(QString("modules/") + defaultKey));
    }

    if (!expandedConfig.value("qbs.buildVariant").isValid())
        throw Error(tr("Property 'qbs.buildVariant' missing in build configuration."));

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
