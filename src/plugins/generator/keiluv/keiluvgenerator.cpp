/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "keiluvgenerator.h"
#include "keiluvproject.h"
#include "keiluvprojectwriter.h"
#include "keiluvworkspace.h"
#include "keiluvworkspacewriter.h"

#include <generators/generatableprojectiterator.h>
#include <generators/generatorutils.h>

#include <logging/logger.h>
#include <logging/translator.h>

#include <tools/filesaver.h>

namespace qbs {

static void writeProjectFiles(const std::map<QString,
                              std::shared_ptr<KeiluvProject>> &projects,
                              const Internal::Logger &logger)
{
    for (const auto &item : projects) {
        const QString projectFilePath = item.first;
        Internal::FileSaver file(projectFilePath.toStdString());
        if (!file.open())
            throw ErrorInfo(Internal::Tr::tr("Cannot open %s for writing")
                            .arg(projectFilePath));

        std::shared_ptr<KeiluvProject> project = item.second;
        KeiluvProjectWriter writer(file.device());
        if (!(writer.write(project.get()) && file.commit()))
            throw ErrorInfo(Internal::Tr::tr("Failed to generate %1")
                            .arg(projectFilePath));

        logger.qbsInfo() << Internal::Tr::tr("Generated %1").arg(
                                QFileInfo(projectFilePath).fileName());
    }
}

static void writeWorkspace(const std::shared_ptr<KeiluvWorkspace> &wokspace,
                           const QString &workspaceFilePath,
                           const Internal::Logger &logger)
{
    Internal::FileSaver file(workspaceFilePath.toStdString());
    if (!file.open())
        throw ErrorInfo(Internal::Tr::tr("Cannot open %s for writing")
                        .arg(workspaceFilePath));

    KeiluvWorkspaceWriter writer(file.device());
    if (!(writer.write(wokspace.get()) && file.commit()))
        throw ErrorInfo(Internal::Tr::tr("Failed to generate %1")
                        .arg(workspaceFilePath));

    logger.qbsInfo() << Internal::Tr::tr("Generated %1").arg(
                            QFileInfo(workspaceFilePath).fileName());
}

KeiluvGenerator::KeiluvGenerator(const gen::VersionInfo &versionInfo)
    : m_versionInfo(versionInfo)
{
}

QString KeiluvGenerator::generatorName() const
{
    return QStringLiteral("keiluv%1").arg(m_versionInfo.marketingVersion());
}

void KeiluvGenerator::reset()
{
    m_workspace.reset();
    m_workspaceFilePath.clear();
    m_projects.clear();
}

void KeiluvGenerator::generate()
{
    GeneratableProjectIterator it(project());
    it.accept(this);

    writeProjectFiles(m_projects, logger());
    writeWorkspace(m_workspace, m_workspaceFilePath, logger());

    reset();
}

void KeiluvGenerator::visitProject(const GeneratableProject &project)
{
    const QDir buildDir = project.baseBuildDirectory();

    m_workspaceFilePath = buildDir.absoluteFilePath(
                project.name() + QStringLiteral(".uvmpw"));
    m_workspace = std::make_shared<KeiluvWorkspace>(m_workspaceFilePath);
}

void KeiluvGenerator::visitProjectData(
        const GeneratableProject &project,
        const GeneratableProjectData &projectData)
{
    Q_UNUSED(project)
    Q_UNUSED(projectData)
}

void KeiluvGenerator::visitProduct(
        const GeneratableProject &project,
        const GeneratableProjectData &projectData,
        const GeneratableProductData &productData)
{
    Q_UNUSED(projectData);

    const QDir baseBuildDir(project.baseBuildDirectory().absolutePath());
    const QString projFileName = productData.name() + QLatin1String(".uvprojx");
    const QString projectFilePath = baseBuildDir.absoluteFilePath(projFileName);
    const auto targetProject = std::make_shared<KeiluvProject>(
                project, productData,  m_versionInfo);

    m_projects.insert({projectFilePath, targetProject});
    m_workspace->addProject(projectFilePath);
}

} // namespace qbs
