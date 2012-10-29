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

#include <language/loader.h>
#include <logging/logger.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/platform.h>

#include <QHash>
#include <QList>
#include <QPair>

namespace qbs {

class SourceProjectPrivate : public QSharedData
{
public:
    QbsEngine *engine;
    QSharedPointer<BuildGraph> buildGraph;
    Settings::Ptr settings;

    // First: Unexpanded config (coming in via the API), second: fully expanded config
    typedef QPair<QVariantMap, QVariantMap> BuildConfigPair;
    QList<BuildConfigPair> buildConfigurations;

    // Potentially stored temporarily between calls to setupResolvedProject() and setupBuildProject().
    QList<BuildProject::Ptr> restoredBuildProjects;
    QHash<ResolvedProject::Ptr, BuildProject::Ptr> discardedBuildProjects;
};

SourceProject::SourceProject(QbsEngine *engine) : d(new SourceProjectPrivate)
{
    d->engine = engine;
}

SourceProject::~SourceProject()
{
}

SourceProject::SourceProject(const SourceProject &other) : d(other.d)
{
}

SourceProject &SourceProject::operator =(const SourceProject &other)
{
    d = other.d;

    return *this;
}

void SourceProject::setSettings(const Settings::Ptr &settings)
{
    d->settings = settings;
}

void SourceProject::loadPlugins()
{
    static bool alreadyCalled = false;
    if (alreadyCalled)
        qbsWarning("qbs::SourceProject::loadPlugins was called more than once.");
    alreadyCalled = true;

    QStringList pluginPaths;
    foreach (const QString &pluginPath, settings()->pluginPaths()) {
        if (!FileInfo::exists(pluginPath)) {
            qbsWarning() << tr("Plugin path '%1' does not exist.")
                    .arg(QDir::toNativeSeparators(pluginPath));
        } else {
            pluginPaths << pluginPath;
        }
    }
    foreach (const QString &pluginPath, pluginPaths)
        QCoreApplication::addLibraryPath(pluginPath);
    ScannerPluginManager::instance()->loadPlugins(pluginPaths);
}

ResolvedProject::Ptr SourceProject::setupResolvedProject(const QString &projectFileName,
                                                         const QVariantMap &_buildConfig)
{
    Loader loader(d->engine);
    loader.setSearchPaths(settings()->searchPaths());
    if (!d->buildGraph) {
        d->buildGraph = QSharedPointer<BuildGraph>(new BuildGraph(d->engine));
        d->buildGraph->setOutputDirectoryRoot(QDir::currentPath());
    }

    ProjectFile::Ptr projectFile;
    const QVariantMap buildConfig = expandedBuildConfiguration(_buildConfig);
    const FileTime projectFileTimeStamp = FileInfo(projectFileName).lastModified();
    const BuildProject::LoadResult loadResult = BuildProject::load(projectFileName,
            d->buildGraph.data(), projectFileTimeStamp, buildConfig, settings()->searchPaths());

    BuildProject::Ptr bProject;
    ResolvedProject::Ptr rProject;
    if (!loadResult.discardLoadedProject)
        bProject = loadResult.loadedProject;
    if (bProject) {
        d->restoredBuildProjects << bProject;
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

    return rProject;
}

BuildProject::Ptr SourceProject::setupBuildProject(const ResolvedProject::Ptr &resolvedProject)
{
    for (int i = 0; i < d->restoredBuildProjects.count(); ++i) {
        const BuildProject::Ptr buildProject = d->restoredBuildProjects.at(i);
        if (buildProject->resolvedProject() == resolvedProject) {
            d->restoredBuildProjects.removeAt(i);
            return buildProject;
        }
    }

    TimedActivityLogger resolveLogger(QLatin1String("Resolving build project"));
    const BuildProject::Ptr buildProject = d->buildGraph->resolveProject(resolvedProject);
    const QHash<ResolvedProject::Ptr, BuildProject::Ptr>::Iterator it
            = d->discardedBuildProjects.find(resolvedProject);
    if (it != d->discardedBuildProjects.end()) {
        buildProject->rescueDependencies(it.value());
        d->discardedBuildProjects.erase(it);
    }

    return buildProject;
}

QVariantMap SourceProject::expandedBuildConfiguration(const QVariantMap &userBuildConfig)
{
    foreach (const SourceProjectPrivate::BuildConfigPair &configPair, d->buildConfigurations) {
        if (configPair.first == userBuildConfig)
            return configPair.second;
    }
    const SourceProjectPrivate::BuildConfigPair configPair
            = qMakePair(userBuildConfig, createBuildConfiguration(userBuildConfig));
    d->buildConfigurations << configPair;
    return configPair.second;
}

QVariantMap SourceProject::createBuildConfiguration(const QVariantMap &userBuildConfig)
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
        profileName = settings()->value("profile").toString();
        if (profileName.isNull())
            throw Error(tr("No profile given.\n"
                           "Either set the configuration value 'profile' to a valid profile's name\n"
                           "or specify the profile with the command line parameter 'profile:name'."));
        expandedConfig.insert("qbs.profile", profileName);
    }

    // (2)
    const QString profileGroup = QString("profiles/%1").arg(profileName);
    const QStringList profileKeys = settings()->allKeysWithPrefix(profileGroup);
    if (profileKeys.isEmpty())
        throw Error(tr("Unknown profile '%1'.").arg(profileName));
    foreach (const QString &profileKey, profileKeys) {
        QString fixedKey(profileKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, settings()->value(profileGroup + "/" + profileKey));
    }

    // (3) Need to make sure we have a value for qbs.platform before going any further
    QVariant platformName = expandedConfig.value("qbs.platform");
    if (!platformName.isValid()) {
        platformName = settings()->moduleValue("qbs/platform", profileName);
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
    foreach (const QString &defaultKey, settings()->allKeysWithPrefix("modules")) {
        QString fixedKey(defaultKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, settings()->value(QString("modules/") + defaultKey));
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

Settings::Ptr SourceProject::settings()
{
    if (!d->settings)
        d->settings = Settings::create();
    return d->settings;
}

} // namespace qbs
