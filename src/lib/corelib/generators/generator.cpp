/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "generator.h"
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/qbsassert.h>
#include <QCoreApplication>

namespace qbs {

class ProjectGeneratorPrivate {
public:
    QList<Project> projects;
    QList<QVariantMap> buildConfigurations;
    InstallOptions installOptions;
};

ProjectGenerator::ProjectGenerator()
    : d(new ProjectGeneratorPrivate)
{
}

ProjectGenerator::~ProjectGenerator()
{
    delete d;
}

static QString _configurationName(const qbs::Project &project)
{
    return project.projectConfiguration()
            .value(QStringLiteral("qbs")).toMap()
            .value(QStringLiteral("configurationName")).toString();
}

static QString _configurationName(const QVariantMap &buildConfiguration)
{
    return buildConfiguration.value(QStringLiteral("qbs.configurationName")).toString();
}

void ProjectGenerator::generate(const QList<Project> &projects,
                                const QList<QVariantMap> &buildConfigurations,
                                const InstallOptions &installOptions)
{
    d->projects = projects;
    std::sort(d->projects.begin(), d->projects.end(),
              [](const qbs::Project &a, const qbs::Project &b) {
                  return _configurationName(a) < _configurationName(b); });
    d->buildConfigurations = buildConfigurations;
    std::sort(d->buildConfigurations.begin(), d->buildConfigurations.end(),
              [](const QVariantMap &a, const QVariantMap &b) {
                  return _configurationName(a) < _configurationName(b); });
    d->installOptions = installOptions;
    generate();
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
        return QVariantMap();
    return d->buildConfigurations.at(idx);
}

QStringList ProjectGenerator::buildConfigurationCommandLine(const Project &project) const
{
    QVariantMap config = buildConfiguration(project);

    const QString name = config.take(QStringLiteral("qbs.configurationName")).toString();
    if (name.isEmpty())
        throw qbs::ErrorInfo(QStringLiteral("Can't find configuration name for project"));

    QStringList commandLineParameters;
    commandLineParameters += name;

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
static int _productCount(const QList<qbs::ProjectData> &projects)
{
    int count = -1;
    for (const auto &project : projects) {
        int oldCount = count;
        count = project.products().size();
        QBS_CHECK(oldCount == -1 || oldCount == count);
    }
    return count;
}

static int _subprojectCount(const QList<qbs::ProjectData> &projects)
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
        const QMap<QString, qbs::ProjectData> &map) {
    GeneratableProjectData gproject;

    // Add the project's project data for each configuration
    QMapIterator<QString, qbs::ProjectData> it(map);
    while (it.hasNext()) {
        it.next();
        gproject.data.insert(it.key(), it.value());
    }

    // Add the project's products...
    for (int i = 0; i < _productCount(map.values()); ++i) {
        GeneratableProductData prod;

        // once for each configuration
        QMapIterator<QString, qbs::ProjectData> it(map);
        while (it.hasNext()) {
            it.next();
            prod.data.insert(it.key(), it.value().products().at(i));
        }

        gproject.products.append(prod);
    }

    // Add the project's subprojects...
    for (int i = 0; i < _subprojectCount(map.values()); ++i) {
        QMap<QString, qbs::ProjectData> subprojectMap;

        // once for each configuration
        QMapIterator<QString, qbs::ProjectData> it(map);
        while (it.hasNext()) {
            it.next();
            subprojectMap.insert(it.key(), it.value().subProjects().at(i));
        }

        gproject.subProjects.append(_reduceProjectConfigurations(subprojectMap));
    }

    return gproject;
}

const GeneratableProject ProjectGenerator::project() const
{
    QMap<QString, qbs::ProjectData> rootProjects;
    GeneratableProject proj;
    for (const auto &project : projects()) {
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
    proj.installRoot = d->installOptions.installRoot();
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

} // namespace qbs
