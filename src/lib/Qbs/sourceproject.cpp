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

#include "sourceproject.h"

#include <language/loader.h>
#include <tools/logger.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/platform.h>

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>

using namespace qbs;

namespace Qbs {

class SourceProjectPrivate : public QSharedData
{
public:
    QStringList searchPaths;
    QList<qbs::Configuration::Ptr> configurations;

    QFutureInterface<bool> *futureInterface;
    QSharedPointer<qbs::BuildGraph> buildGraph;
    QVector<Qbs::BuildProject> buildProjects;

    qbs::Settings::Ptr settings;
    QList<Error> errors;

    QString buildDirectoryRoot;
    int progressValue;
};

SourceProject::SourceProject()
    : d(new SourceProjectPrivate)
{
    d->settings = qbs::Settings::create(); // fix it
    d->progressValue = 0;
    d->futureInterface = 0;
}

SourceProject::~SourceProject()
{
}

SourceProject::SourceProject(const SourceProject &other)
    : d(other.d)
{
}

SourceProject &SourceProject::operator =(const SourceProject &other)
{
    d = other.d;

    return *this;
}

void SourceProject::setSettings(const qbs::Settings::Ptr &setttings)
{
    d->settings = setttings;
}

void SourceProject::setSearchPaths(const QStringList &searchPaths)
{
    d->searchPaths = searchPaths;
}

void SourceProject::loadPlugins(const QStringList &pluginPaths)
{
    static bool alreadyCalled = false;
    if (alreadyCalled)
        qbsWarning("qbs::SourceProject::loadPlugins was called more than once.");
    alreadyCalled = true;

    foreach (const QString &pluginPath, pluginPaths)
        QCoreApplication::addLibraryPath(pluginPath);

    qbs::ScannerPluginManager::instance()->loadPlugins(pluginPaths);
}

void SourceProject::loadProjectIde(QFutureInterface<bool> &futureInterface,
                                const QString projectFileName,
                                const QList<QVariantMap> buildConfigurations)
{
    if (buildConfigurations.isEmpty()) {
        qbsFatal("SourceProject::loadProject: no build configuration given.");
        futureInterface.reportResult(false);
        return;
    }

    QList<qbs::Configuration::Ptr> configurations;
    foreach (QVariantMap buildConfiguation, buildConfigurations) {
        if (!buildConfiguation.value("qbs.buildVariant").isValid()) {
            qbsFatal("SourceProject::loadProject: property 'buildVariant' missing in build configuration.");
            continue;
        }

        qbs::Configuration::Ptr configuration(new qbs::Configuration);
        configurations.append(configuration);

        QVariantMap configurationMap = configuration->value();
        foreach (const QString &property, buildConfiguation.keys()) {
            qbs::setConfigProperty(configurationMap, property.split('.'), buildConfiguation.value(property));
        }
        configuration->setValue(configurationMap);
    }

    qbs::Loader loader;
    loader.setProgressObserver(this);
    loader.setSearchPaths(d->searchPaths);
    d->buildGraph = QSharedPointer<qbs::BuildGraph>(new qbs::BuildGraph);
    d->buildGraph->setProgressObserver(this);
    d->buildGraph->setOutputDirectoryRoot(QFileInfo(projectFileName).absoluteDir().path());
    d->futureInterface = &futureInterface;

    const QString buildDirectoryRoot = d->buildGraph->buildDirectoryRoot();


    try {
        int productCount = 0;
        QHash<qbs::Configuration::Ptr, qbs::BuildProject::Ptr> buildProjectsPerConfig;
        QHash<qbs::Configuration::Ptr, qbs::ResolvedProject::Ptr> resolvedProjectsPerConfig;
        QHash<qbs::Configuration::Ptr, qbs::ProjectFile::Ptr> projectFilesPerConfig;
        foreach (const qbs::Configuration::Ptr &configuration, configurations) {
            const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();
            qbs::BuildProject::LoadResult loadResult;
            loadResult = qbs::BuildProject::load(d->buildGraph.data(), projectFileTimeStamp, configuration, d->searchPaths);
            if (loadResult.loadedProject && !loadResult.discardLoadedProject) {
                buildProjectsPerConfig.insert(configuration, loadResult.loadedProject);
            } else if (loadResult.changedResolvedProject) {
                productCount += loadResult.changedResolvedProject->products.count();
                resolvedProjectsPerConfig.insert(configuration, loadResult.changedResolvedProject);
            } else {
                ProjectFile::Ptr projectFile = loader.loadProject(projectFileName);
                projectFilesPerConfig.insert(configuration, projectFile);
                productCount += loader.productCount(configuration);
            }
        }

        futureInterface.setProgressRange(0, productCount * 2);

        foreach (const qbs::Configuration::Ptr &configuration, configurations) {
            qbs::BuildProject::Ptr buildProject = buildProjectsPerConfig.value(configuration);
            if (!buildProject) {
                ResolvedProject::Ptr rProject = resolvedProjectsPerConfig.value(configuration);
                if (!rProject) {
                    ProjectFile::Ptr projectFile = projectFilesPerConfig.value(configuration);
                    if (!projectFile)
                        projectFile = loader.loadProject(projectFileName);
                    rProject = loader.resolveProject(projectFile, buildDirectoryRoot, configuration);
                }
                if (rProject->products.isEmpty())
                    throw qbs::Error(QString("'%1' does not contain products.").arg(projectFileName));
                buildProject = d->buildGraph->resolveProject(rProject);
            }

            d->buildProjects.append(BuildProject(buildProject));
        }

    } catch (qbs::Error &error) {
        d->errors.append(Error(error));
        futureInterface.reportResult(false);
        d->futureInterface = 0;
        return;
    }

    futureInterface.reportResult(true);
    d->futureInterface = 0;
}

void warnLegacyConfig(const QString &aKey)
{
    qbsWarning("Config key %s is deprecated. Run qbs config --upgrade [--global|--local]",
        qPrintable(QString(aKey).replace("/", ".")));
}

void SourceProject::loadProjectCommandLine(QString projectFileName, QList<QVariantMap> buildConfigs)
{
    QHash<QString, qbs::Platform::Ptr > platforms = Platform::platforms();
    if (platforms.isEmpty()) {
        d->errors.append(Error(tr("No platforms configured. You must run 'qbs platforms probe' first.")));
        return;
    }
    if (buildConfigs.isEmpty()) {
        d->errors.append(Error(tr("SourceProject::loadProject: no build configuration given.")));
        return;
    }
    QList<qbs::Configuration::Ptr> configurations;
    foreach (QVariantMap buildCfg, buildConfigs) {
        // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
        // 1) Things specified on command line (already in buildCfg at this point)
        // 2) Everything from the profile key (in reverse order)
        // 2a) For compatibility with v0.1, treat qt/<qtVersionName>/* the same as if it were profiles.<qtVersionName>.qt.core.*
        // 3) Everything from the platform
        // 4) Any remaining keys from modules keyspace
        QStringList profiles;
        if (buildCfg.contains("profile"))
            profiles = buildCfg.take("profile").toString().split(QChar(','), QString::SkipEmptyParts); // Remove from buildCfg if present
        else
            profiles = d->settings->value("profile", "").toString().split(QChar(','), QString::SkipEmptyParts);

        const QString combinedBuildProfileName = profiles.join(QLatin1String("-"));
        buildCfg.insert("buildProfileName", combinedBuildProfileName);

        // (2)
        bool profileError = false;
        for (int i = profiles.count() - 1; i >= 0; i--) {
            QString profileGroup = QString("profiles/%1").arg(profiles[i]);
            QStringList profileKeys = d->settings->allKeysWithPrefix(profileGroup);
            if (profileKeys.isEmpty()) {
                qbsFatal("Unknown profile '%s'.", qPrintable(profiles[i]));
                profileError = true; // Need this because we can't break out of 2 for loops at once
                break;
            }
            foreach (const QString &profileKey, profileKeys) {
                QString fixedKey(profileKey);
                fixedKey.replace(QChar('/'), QChar('.'));
                if (!buildCfg.contains(fixedKey))
                    buildCfg.insert(fixedKey, d->settings->value(profileGroup + "/" + profileKey));
            }
        }
        if (profileError)
            continue;

        // (2a) ### can remove? This replaces the qbs.configurationValue() stuff in qtcore.qbs in v0.1
        // Check for someone setting qt.core.qtVersionName explicitly on command-line because we've removed this property
        QString legacyQtVersionName = buildCfg.take("qt/core.qtVersionName").toString();
        if (legacyQtVersionName.length()) {
            qbsWarning("Setting qt/core.qtVersionName is deprecated. Run 'qbs config --upgrade [--global|--local]' and use profile:%s instead", qPrintable(legacyQtVersionName));
        } else {
            legacyQtVersionName = d->settings->value("defaults/qtVersionName").toString();
            if (legacyQtVersionName.length())
                warnLegacyConfig("defaults.qtVersionName");
            else
                legacyQtVersionName = "default";
        }
        QStringList legacyQtKeys = d->settings->allKeysWithPrefix("qt/" + legacyQtVersionName);
        foreach (const QString &key, legacyQtKeys) {
            QString oldKey = QString("qt/%1/%2").arg(legacyQtVersionName).arg(key);
            QString newKey = QString("qt.core.%1").arg(key);
            if (!buildCfg.contains(newKey)) {
                warnLegacyConfig(oldKey);
                buildCfg.insert(newKey, d->settings->value(oldKey));
            }
        }

        // (3) Need to make sure we have a value for qbs.platform before going any further
        if (!buildCfg.value("qbs.platform").isValid()) {
            QVariant defaultPlatform = d->settings->moduleValue("qbs/platform", profiles);
            if (!defaultPlatform.isValid()) {
                // ### Compatibility with v0.1
                defaultPlatform = d->settings->value("defaults/platform");
                if (defaultPlatform.isValid())
                    warnLegacyConfig("defaults/platform");
                }
            if (!defaultPlatform.isValid()) {
                qbsFatal("SourceProject::loadProject: no platform given and no default set.");
                continue;
            }
            buildCfg.insert("qbs.platform", defaultPlatform);
        }
        Platform::Ptr platform = platforms.value(buildCfg.value("qbs.platform").toString());
        if (platform.isNull()) {
            qbsFatal("SourceProject::loadProject: unknown platform: %s", qPrintable(buildCfg.value("qbs.platform").toString()));
            continue;
        }
        foreach (const QString &key, platform->settings.allKeys()) {
            if (key.startsWith(Platform::internalKey()))
                continue;
            QString fixedKey = key;
            int idx = fixedKey.lastIndexOf(QChar('/'));
            if (idx > 0)
                fixedKey[idx] = QChar('.');
            if (!buildCfg.contains(fixedKey))
                buildCfg.insert(fixedKey, platform->settings.value(key));
        }
        // Now finally do (4)
        foreach (const QString &defaultKey, d->settings->allKeysWithPrefix("modules")) {
            QString fixedKey(defaultKey);
            fixedKey.replace(QChar('/'), QChar('.'));
            if (!buildCfg.contains(fixedKey))
                buildCfg.insert(fixedKey, d->settings->value(QString("modules/") + defaultKey));
        }

        if (!buildCfg.value("qbs.buildVariant").isValid()) {
            qbsFatal("SourceProject::loadProject: property 'qbs.buildVariant' missing in build configuration.");
            continue;
        }
        qbs::Configuration::Ptr configure(new qbs::Configuration);
        configurations.append(configure);

        foreach (const QString &property, buildCfg.keys()) {
            QStringList nameElements = property.split('.');
            if (nameElements.count() == 1 && nameElements.first() != "project") // ### Still need this because platform doesn't supply fully-qualified properties (yet), need to fix this
                nameElements.prepend("qbs");
            if (nameElements.count() > 2) { // ### workaround for submodules being represented internally as a single module of name "module/submodule" rather than two nested modules "module" and "submodule"
                QStringList newElements(QStringList(nameElements.mid(0, nameElements.count()-1)).join("/"));
                newElements.append(nameElements.last());
                nameElements = newElements;
            }
            QVariantMap configValue = configure->value();
            qbs::setConfigProperty(configValue, nameElements, buildCfg.value(property));
            configure->setValue(configValue);
        }
    }

    qbs::Loader loader;
    loader.setSearchPaths(d->searchPaths);
    d->buildGraph = QSharedPointer<qbs::BuildGraph>(new qbs::BuildGraph);
    d->buildGraph->setOutputDirectoryRoot(QDir::currentPath());
    const QString buildDirectoryRoot = d->buildGraph->buildDirectoryRoot();

    try {
        foreach (const qbs::Configuration::Ptr &configure, configurations) {
            qbs::BuildProject::Ptr bProject;
            const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();
            qbs::BuildProject::LoadResult loadResult;
            loadResult = qbs::BuildProject::load(d->buildGraph.data(), projectFileTimeStamp, configure, d->searchPaths);
            if (!loadResult.discardLoadedProject)
                bProject = loadResult.loadedProject;
            if (!bProject) {
                QElapsedTimer timer;
                timer.start();
                if (!loader.hasLoaded())
                    loader.loadProject(projectFileName);
                qbs::ResolvedProject::Ptr rProject;
                if (loadResult.changedResolvedProject)
                    rProject = loadResult.changedResolvedProject;
                else
                    rProject = loader.resolveProject(buildDirectoryRoot, configure);
                if (rProject->products.isEmpty())
                    throw qbs::Error(QString("'%1' does not contain products.").arg(projectFileName));
                qDebug() << "loading project took: " << timer.elapsed() << "ms";
                timer.start();
                bProject = d->buildGraph->resolveProject(rProject);
                if (loadResult.loadedProject)
                    bProject->rescueDependencies(loadResult.loadedProject);
                qDebug() << "build graph took: " << timer.elapsed() << "ms";
            }

            // copy the environment from the platform config into the project's config
            QVariantMap projectCfg = bProject->resolvedProject()->configuration->value();
            projectCfg.insert("environment", configure->value().value("environment"));
            bProject->resolvedProject()->configuration->setValue(projectCfg);

            d->buildProjects.append(bProject);

            printf("for %s:\n", qPrintable(bProject->resolvedProject()->id));
            foreach (qbs::ResolvedProduct::Ptr p, bProject->resolvedProject()->products) {
                printf("  - [%s] %s as %s\n"
                        ,qPrintable(p->fileTags.join(", "))
                        ,qPrintable(p->name)
                        ,qPrintable(p->project->id)
                      );
            }
            printf("\n");
        }

    } catch (qbs::Error &e) {
        d->errors.append(e);
        return;
    }
}

static QStringList buildGraphFilePaths(const QDir &buildDirectory)
{
    QStringList filePathList;

    QStringList nameFilters;
    nameFilters.append("*.bg");

    foreach (const QFileInfo &fileInfo, buildDirectory.entryInfoList(nameFilters, QDir::Files))
        filePathList.append(fileInfo.absoluteFilePath());

    return filePathList;
}

void SourceProject::loadBuildGraphs(const QString projectFileName)
{
    QDir buildDirectory = QFileInfo(projectFileName).absoluteDir();
    QString projectRootPath = buildDirectory.path();

    bool sucess = buildDirectory.cd(QLatin1String("build"));

    if (sucess) {
        d->buildGraph = QSharedPointer<qbs::BuildGraph>(new qbs::BuildGraph);
        d->buildGraph->setOutputDirectoryRoot(projectRootPath);
        const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();

        foreach (const QString &buildGraphFilePath, buildGraphFilePaths(buildDirectory))
            loadBuildGraph(buildGraphFilePath, projectFileTimeStamp);
    }
}

void SourceProject::storeBuildProjectsBuildGraph()
{
    foreach (BuildProject buildProject, d->buildProjects)
        buildProject.storeBuildGraph();
}

void SourceProject::setBuildDirectoryRoot(const QString &buildDirectoryRoot)
{
    d->buildDirectoryRoot = buildDirectoryRoot;
}

QString SourceProject::buildDirectoryRoot() const
{
    return d->buildDirectoryRoot;
}

QVector<BuildProject> SourceProject::buildProjects() const
{
    return d->buildProjects;
}

QList<qbs::BuildProject::Ptr> SourceProject::internalBuildProjects() const
{
    QList<qbs::BuildProject::Ptr> result;
    foreach (const Qbs::BuildProject &buildProject, d->buildProjects)
        result += buildProject.internalBuildProject();
    return result;
}

QList<Error> SourceProject::errors() const
{
    return d->errors;
}

void SourceProject::setProgressRange(int minimum, int maximum)
{
    if (d->futureInterface)
        d->futureInterface->setProgressRange(minimum, maximum);
}

void SourceProject::setProgressValue(int value)
{
    if (d->futureInterface)
        d->futureInterface->setProgressValue(value);
}

int SourceProject::progressValue()
{
    return d->futureInterface ? d->futureInterface->progressValue() : 0;
}

void SourceProject::loadBuildGraph(const QString &buildGraphPath, const qbs::FileTime &projectFileTimeStamp)
{
    try {
        qbs::BuildProject::Ptr buildProject;
        buildProject = qbs::BuildProject::loadBuildGraph(buildGraphPath, d->buildGraph.data(), projectFileTimeStamp, d->searchPaths);
        d->buildProjects.append(BuildProject(buildProject));
    } catch (qbs::Error &error) {
        d->errors.append(Error(error));
        return;
    }
}

}

