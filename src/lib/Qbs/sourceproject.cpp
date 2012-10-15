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
#include <tools/logger.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/platform.h>

#include <QElapsedTimer>

using namespace qbs;

namespace Qbs {

class SourceProjectPrivate : public QSharedData
{
public:
    QbsEngine *engine;
    QSharedPointer<qbs::BuildGraph> buildGraph;
    QVector<Qbs::BuildProject> buildProjects;
    qbs::Settings::Ptr settings;
};

SourceProject::SourceProject(QbsEngine *engine) : d(new SourceProjectPrivate)
{
    d->engine = engine;
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

void SourceProject::setSettings(const qbs::Settings::Ptr &settings)
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
    foreach (const QString &pluginPath, d->settings->pluginPaths()) {
        if (!qbs::FileInfo::exists(pluginPath)) {
            qbsWarning() << tr("Plugin path '%1' does not exist.")
                    .arg(QDir::toNativeSeparators(pluginPath));
        } else {
            pluginPaths << pluginPath;
        }
    }
    foreach (const QString &pluginPath, pluginPaths)
        QCoreApplication::addLibraryPath(pluginPath);
    qbs::ScannerPluginManager::instance()->loadPlugins(pluginPaths);
}

void SourceProject::loadProject(const QString &projectFileName,
        const QList<QVariantMap> &buildConfigs)
{
    if (buildConfigs.isEmpty())
        throw Error(tr("No build configuration given."));

    QHash<QString, qbs::Platform::Ptr > platforms = Platform::platforms();
    if (platforms.isEmpty())
        throw Error(tr("No platforms configured. You must run 'qbs platforms probe' first."));

    QList<qbs::Configuration::Ptr> configurations;
    foreach (QVariantMap buildCfg, buildConfigs) {
        // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
        // 1) Things specified on command line (already in buildCfg at this point)
        // 2) Everything from the profile key (in reverse order)
        // 3) Everything from the platform
        // 4) Any remaining keys from modules keyspace
        QString profileName = buildCfg.value("qbs.profile").toString();
        if (profileName.isNull()) {
            profileName = d->settings->value("profile").toString();
            if (profileName.isNull())
                throw Error(tr("No profile given.\n"
                               "Either set the configuration value 'profile' to a valid profile's name\n"
                               "or specify the profile with the command line parameter 'profile:name'."));
            buildCfg.insert("qbs.profile", profileName);
        }

        // (2)
        const QString profileGroup = QString("profiles/%1").arg(profileName);
        const QStringList profileKeys = d->settings->allKeysWithPrefix(profileGroup);
        if (profileKeys.isEmpty())
            throw Error(tr("Unknown profile '%1'.").arg(profileName));
        foreach (const QString &profileKey, profileKeys) {
            QString fixedKey(profileKey);
            fixedKey.replace(QChar('/'), QChar('.'));
            if (!buildCfg.contains(fixedKey))
                buildCfg.insert(fixedKey, d->settings->value(profileGroup + "/" + profileKey));
        }

        // (3) Need to make sure we have a value for qbs.platform before going any further
        QVariant platformName = buildCfg.value("qbs.platform");
        if (!platformName.isValid()) {
            platformName = d->settings->moduleValue("qbs/platform", profileName);
            if (!platformName.isValid())
                throw Error(tr("No platform given and no default set."));
            buildCfg.insert("qbs.platform", platformName);
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

        if (!buildCfg.value("qbs.buildVariant").isValid())
            throw Error(tr("Property 'qbs.buildVariant' missing in build configuration."));
        qbs::Configuration::Ptr configure = qbs::Configuration::create();
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

    qbs::Loader loader(d->engine);
    loader.setSearchPaths(d->settings->searchPaths());
    d->buildGraph = QSharedPointer<qbs::BuildGraph>(new qbs::BuildGraph(d->engine));
    d->buildGraph->setOutputDirectoryRoot(QDir::currentPath());
    const QString buildDirectoryRoot = d->buildGraph->buildDirectoryRoot();

    ProjectFile::Ptr projectFile;
    foreach (const qbs::Configuration::Ptr &configure, configurations) {
        qbs::BuildProject::Ptr bProject;
        const qbs::FileTime projectFileTimeStamp = qbs::FileInfo(projectFileName).lastModified();
        qbs::BuildProject::LoadResult loadResult;
        loadResult = qbs::BuildProject::load(d->buildGraph.data(), projectFileTimeStamp, configure, d->settings->searchPaths());
        if (!loadResult.discardLoadedProject)
            bProject = loadResult.loadedProject;
        if (!bProject) {
            QElapsedTimer timer;
            timer.start();
            if (!projectFile)
                projectFile = loader.loadProject(projectFileName);
            qbs::ResolvedProject::Ptr rProject;
            if (loadResult.changedResolvedProject)
                rProject = loadResult.changedResolvedProject;
            else
                rProject = loader.resolveProject(projectFile, buildDirectoryRoot, configure);
            if (rProject->products.isEmpty())
                throw qbs::Error(QString("'%1' does not contain products.").arg(projectFileName));
            qbsDebug() << "Project loaded in " << timer.elapsed() << "ms.";
            timer.start();
            bProject = d->buildGraph->resolveProject(rProject);
            if (loadResult.loadedProject)
                bProject->rescueDependencies(loadResult.loadedProject);
            qbsDebug() << "Build graph took " << timer.elapsed() << "ms.";
        }

        // copy the environment from the platform config into the project's config
        const QVariantMap platformEnvironment = configure->value().value("environment").toMap();
        bProject->resolvedProject()->platformEnvironment = platformEnvironment;

        d->buildProjects.append(bProject);

        qbsDebug("for %s:", qPrintable(bProject->resolvedProject()->id()));
        foreach (const qbs::ResolvedProduct::ConstPtr &p, bProject->resolvedProject()->products) {
            qbsDebug("  - [%s] %s as %s"
                   ,qPrintable(p->fileTags.join(", "))
                   ,qPrintable(p->name)
                   ,qPrintable(p->project->id())
                   );
        }
        qbsDebug("");
    }
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

QbsEngine *SourceProject::engine() const
{
    return d->engine;
}

} // namespace Qbs

