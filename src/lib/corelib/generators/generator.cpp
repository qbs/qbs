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

#include "generator.h"
#include <logging/logger.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>
#include <QtCore/qcoreapplication.h>

namespace qbs {

class ProjectGeneratorPrivate {
public:
    QList<Project> projects;
    QList<QVariantMap> buildConfigurations;
    InstallOptions installOptions;
    QString qbsSettingsDir;
    Internal::Logger logger = Internal::Logger(nullptr);
};

ProjectGenerator::ProjectGenerator()
    : d(new ProjectGeneratorPrivate)
{
}

ProjectGenerator::~ProjectGenerator()
{
    delete d;
}

static QString _configurationName(const Project &project)
{
    return project.projectConfiguration()
            .value(Internal::StringConstants::qbsModule()).toMap()
            .value(Internal::StringConstants::configurationNameProperty()).toString();
}

static QString _configurationName(const QVariantMap &buildConfiguration)
{
    return buildConfiguration.value(QStringLiteral("qbs.configurationName")).toString();
}

ErrorInfo ProjectGenerator::generate(const QList<Project> &projects,
                                const QList<QVariantMap> &buildConfigurations,
                                const InstallOptions &installOptions,
                                const QString &qbsSettingsDir,
                                const Internal::Logger &logger)
{
    d->projects = projects;
    std::sort(d->projects.begin(), d->projects.end(),
              [](const Project &a, const Project &b) {
                  return _configurationName(a) < _configurationName(b); });
    d->buildConfigurations = buildConfigurations;
    std::sort(d->buildConfigurations.begin(), d->buildConfigurations.end(),
              [](const QVariantMap &a, const QVariantMap &b) {
                  return _configurationName(a) < _configurationName(b); });
    d->installOptions = installOptions;
    d->qbsSettingsDir = qbsSettingsDir;
    d->logger = logger;
    try {
        generate();
    } catch (const ErrorInfo &e) {
        return e;
    }
    return {};
}

QList<Project> ProjectGenerator::projects() const
{
    return d->projects;
}

QList<QVariantMap> ProjectGenerator::buildConfigurations() const
{
    return d->buildConfigurations;
}

QVariantMap ProjectGenerator::buildConfiguration(const Project &project) const
{
    int idx = d->projects.indexOf(project);
    if (idx < 0)
        return {};
    return d->buildConfigurations.at(idx);
}

QStringList ProjectGenerator::buildConfigurationCommandLine(const Project &project) const
{
    QVariantMap config = buildConfiguration(project);

    const QString name = config.take(QStringLiteral("qbs.configurationName")).toString();
    if (name.isEmpty())
        throw ErrorInfo(QStringLiteral("Can't find configuration name for project"));

    QStringList commandLineParameters;
    commandLineParameters += QStringLiteral("config:") + name;

    QMapIterator<QString, QVariant> it(config);
    while (it.hasNext()) {
        it.next();
        commandLineParameters += it.key() + QStringLiteral(":") + it.value().toString();
    }

    return commandLineParameters;
}

// Count the number of products in the project (singular)
// Precondition: each project data (i.e. per-configuration project data)
// has the same number of products.
static int _productCount(const QList<ProjectData> &projects)
{
    int count = -1;
    for (const auto &project : projects) {
        int oldCount = count;
        count = project.products().size();
        QBS_CHECK(oldCount == -1 || oldCount == count);
    }
    return count;
}

static int _subprojectCount(const QList<ProjectData> &projects)
{
    int count = -1;
    for (const auto &project : projects) {
        int oldCount = count;
        count = project.subProjects().size();
        QBS_CHECK(oldCount == -1 || oldCount == count);
    }
    return count;
}

static GeneratableProjectData _reduceProjectConfigurations(
        const QMap<QString, ProjectData> &map) {
    GeneratableProjectData gproject;

    // Add the project's project data for each configuration
    QMapIterator<QString, ProjectData> it(map);
    while (it.hasNext()) {
        it.next();
        gproject.data.insert(it.key(), it.value());
    }

    // Add the project's products...
    for (int i = 0; i < _productCount(map.values()); ++i) {
        GeneratableProductData prod;

        // once for each configuration
        QMapIterator<QString, ProjectData> it(map);
        while (it.hasNext()) {
            it.next();
            prod.data.insert(it.key(), it.value().products().at(i));
        }

        gproject.products.push_back(prod);
    }

    // Add the project's subprojects...
    for (int i = 0; i < _subprojectCount(map.values()); ++i) {
        QMap<QString, ProjectData> subprojectMap;

        // once for each configuration
        QMapIterator<QString, ProjectData> it(map);
        while (it.hasNext()) {
            it.next();
            subprojectMap.insert(it.key(), it.value().subProjects().at(i));
        }

        gproject.subProjects.push_back(_reduceProjectConfigurations(subprojectMap));
    }

    return gproject;
}

const GeneratableProject ProjectGenerator::project() const
{
    QMap<QString, ProjectData> rootProjects;
    GeneratableProject proj;
    for (const auto &project : qAsConst(d->projects)) {
        const QString configurationName = _configurationName(project);
        rootProjects.insert(configurationName, project.projectData());
        proj.projects.insert(configurationName, project);
        proj.buildConfigurations.insert(configurationName, buildConfiguration(project));
        proj.commandLines.insert(configurationName, buildConfigurationCommandLine(project));
    }
    auto p = _reduceProjectConfigurations(rootProjects);
    proj.data = p.data;
    proj.products = p.products;
    proj.subProjects = p.subProjects;
    proj.installOptions = d->installOptions;
    return proj;
}

QFileInfo ProjectGenerator::qbsExecutableFilePath() const
{
    const QString qbsInstallDir = QString::fromLocal8Bit(qgetenv("QBS_INSTALL_DIR"));
    auto file = QFileInfo(Internal::HostOsInfo::appendExecutableSuffix(!qbsInstallDir.isEmpty()
        ? qbsInstallDir + QLatin1String("/bin/qbs")
        : QCoreApplication::applicationDirPath() + QLatin1String("/qbs")));
    QBS_CHECK(file.isAbsolute() && file.exists());
    return file;
}

QString ProjectGenerator::qbsSettingsDir() const
{
    return d->qbsSettingsDir;
}

const Internal::Logger &ProjectGenerator::logger() const
{
    return d->logger;
}

} // namespace qbs
