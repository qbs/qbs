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

    QSharedPointer<qbs::BuildGraph> buildGraph;
    QVector<Qbs::BuildProject> buildProjects;

    qbs::Settings::Ptr settings;
    QList<Error> errors;

    QString buildDirectoryRoot;

};

SourceProject::SourceProject()
    : d(new SourceProjectPrivate)
{
    d->settings = qbs::Settings::create(); // fix it
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
        if (alreadyCalled) {
            qbsWarning("qbs::SourceProject::loadPlugins was called more than once.");
        }

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
    loader.setSearchPaths(d->searchPaths);
    d->buildGraph = QSharedPointer<qbs::BuildGraph>(new qbs::BuildGraph);

    d->buildGraph->setOutputDirectoryRoot(QFileInfo(projectFileName).absoluteDir().path());

    const QString buildDirectoryRoot = d->buildGraph->buildDirectoryRoot();


    try {
        int productCount = 0;
        foreach (const qbs::Configuration::Ptr &configure, configurations) {
            qbs::BuildProject::Ptr buildProject;
            const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();
            buildProject = qbs::BuildProject::load(d->buildGraph.data(), projectFileTimeStamp, configure, d->searchPaths);
            if (!buildProject) {
                if (!loader.hasLoaded())
                    loader.loadProject(projectFileName);
                productCount += loader.productCount(configure);
            }
        }
        futureInterface.setProgressRange(0, productCount * 2);


        foreach (const qbs::Configuration::Ptr &configure, configurations) {
            qbs::BuildProject::Ptr buildProject;
            const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();
            buildProject = qbs::BuildProject::load(d->buildGraph.data(), projectFileTimeStamp, configure, d->searchPaths);
            if (!buildProject) {
                if (!loader.hasLoaded())
                    loader.loadProject(projectFileName);
                qbs::ResolvedProject::Ptr rProject = loader.resolveProject(buildDirectoryRoot, configure, futureInterface);
                if (rProject->products.isEmpty())
                    throw qbs::Error(QString("'%1' does not contain products.").arg(projectFileName));
                buildProject = d->buildGraph->resolveProject(rProject, futureInterface);
            }

            d->buildProjects.append(BuildProject(buildProject));
        }

    } catch (qbs::Error &error) {
        d->errors.append(Error(error));
        futureInterface.reportResult(false);
        return;
    }

    futureInterface.reportResult(true);
}

void SourceProject::loadProjectCommandLine(QFutureInterface<bool> &futureInterface, QString projectFileName, QList<QVariantMap> buildConfigs)
{
    QHash<QString, qbs::Platform::Ptr > platforms = Platform::platforms();
    if (platforms.isEmpty()) {
        qbsFatal("no platforms configured. maybe you want to run 'qbs platforms probe' first.");
        futureInterface.reportResult(false);
        return;
    }
    if (buildConfigs.isEmpty()) {
        qbsFatal("SourceProject::loadProject: no build configuration given.");
        futureInterface.reportResult(false);
        return;
    }
    QList<qbs::Configuration::Ptr> configurations;
    foreach (QVariantMap buildCfg, buildConfigs) {
        if (!buildCfg.value("platform").isValid()) {
            if (!d->settings->value("defaults/platform").isValid()) {
                qbsFatal("SourceProject::loadProject: no platform given and no default set.");
                continue;
            }
            buildCfg.insert("platform", d->settings->value("defaults/platform").toString());
        }
        Platform::Ptr platform = platforms.value(buildCfg.value("platform").toString());
        if (platform.isNull()) {
            qbsFatal("SourceProject::loadProject: unknown platform: %s", qPrintable(buildCfg.value("platform").toString()));
            continue;
        }
        foreach (const QString &key, platform->settings.allKeys()) {
            QString fixedKey = key;
            int idx = fixedKey.lastIndexOf(QChar('/'));
            if (idx > 0)
                fixedKey[idx] = QChar('.');
            buildCfg.insert(fixedKey, platform->settings.value(key));
        }

        if (!buildCfg.value("buildVariant").isValid()) {
            qbsFatal("SourceProject::loadProject: property 'buildVariant' missing in build configuration.");
            continue;
        }
        qbs::Configuration::Ptr configure(new qbs::Configuration);
        configurations.append(configure);

        foreach (const QString &property, buildCfg.keys()) {
            QStringList nameElements = property.split('.');
            if (nameElements.count() == 1)
                nameElements.prepend("qbs");
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
            bProject = qbs::BuildProject::load(d->buildGraph.data(), projectFileTimeStamp, configure, d->searchPaths);
            if (!bProject) {
                QElapsedTimer timer;
                timer.start();
                if (!loader.hasLoaded())
                    loader.loadProject(projectFileName);
                qbs::ResolvedProject::Ptr rProject = loader.resolveProject(buildDirectoryRoot, configure, futureInterface);
                if (rProject->products.isEmpty())
                    throw qbs::Error(QString("'%1' does not contain products.").arg(projectFileName));
                qDebug() << "loading project took: " << timer.elapsed() << "ms";
                timer.start();
                bProject = d->buildGraph->resolveProject(rProject, futureInterface);
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
        futureInterface.reportResult(false);
        return;
    }

    futureInterface.reportResult(true);


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

QList<Error> SourceProject::errors() const
{
    return d->errors;
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

