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

#include "oldsourceproject.h"

#include <buildgraph/buildgraph.h>
#include <buildgraph/executor.h>
#include <language/loader.h>
#include <tools/runenvironment.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/logger.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/platform.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QScopedPointer>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>

namespace qbs {

SourceProject::SourceProject(qbs::Settings::Ptr settings)
    : m_settings(settings)
{
}

void SourceProject::setSearchPaths(const QStringList &searchPaths)
{
    m_searchPaths = searchPaths;
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

void SourceProject::loadProject(QFutureInterface<bool> &futureInterface, QString projectFileName, QList<QVariantMap> buildConfigs)
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
            if (!m_settings->value("defaults/platform").isValid()) {
                qbsFatal("SourceProject::loadProject: no platform given and no default set.");
                continue;
            }
            buildCfg.insert("platform", m_settings->value("defaults/platform").toString());
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
            qDebug() << fixedKey << platform->settings.value(key);
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
    loader.setSearchPaths(m_searchPaths);
    m_buildGraph = QSharedPointer<qbs::BuildGraph>(new qbs::BuildGraph);
    m_buildGraph->setOutputDirectoryRoot(QDir::currentPath());
    const QString buildDirectoryRoot = m_buildGraph->buildDirectoryRoot();

    try {
        foreach (const qbs::Configuration::Ptr &configure, configurations) {
            qbs::BuildProject::Ptr bProject;
            const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();
            bProject = qbs::BuildProject::load(m_buildGraph.data(), projectFileTimeStamp, configure, m_searchPaths);
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
                bProject = m_buildGraph->resolveProject(rProject, futureInterface);
                qDebug() << "build graph took: " << timer.elapsed() << "ms";
            }

            m_buildProjects.append(bProject);

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
        m_errors.append(e);
        futureInterface.reportResult(false);
        return;
    }

    futureInterface.reportResult(true);
}

QList<BuildProject::Ptr> SourceProject::buildProjects() const
{
    return m_buildProjects;
}

QList<Error> SourceProject::errors() const
{
    return m_errors;
}

} // namespace qbs

