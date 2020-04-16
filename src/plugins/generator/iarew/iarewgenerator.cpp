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

#include "iarewgenerator.h"
#include "iarewproject.h"
#include "iarewprojectwriter.h"
#include "iarewworkspace.h"
#include "iarewworkspacewriter.h"

#include <generators/generatableprojectiterator.h>

#include <logging/logger.h>
#include <logging/translator.h>

#include <tools/filesaver.h>

namespace qbs {

static QString targetFilePath(const QString &baseName,
                              const QString &baseBuildDirectory)
{
    return QDir(baseBuildDirectory).absoluteFilePath(
                baseName + QStringLiteral(".ewp"));
}

static QString targetFilePath(const GeneratableProductData &product,
                              const QString &baseBuildDirectory)
{
    return targetFilePath(product.name(), baseBuildDirectory);
}

static void writeProjectFiles(const std::map<QString,
                              std::shared_ptr<IarewProject>> &projects,
                              const Internal::Logger &logger)
{
    for (const auto &item : projects) {
        const QString projectFilePath = item.first;
        Internal::FileSaver file(projectFilePath.toStdString());
        if (!file.open())
            throw ErrorInfo(Internal::Tr::tr("Cannot open %s for writing")
                            .arg(projectFilePath));

        std::shared_ptr<IarewProject> project = item.second;
        IarewProjectWriter writer(file.device());
        if (!(writer.write(project.get()) && file.commit()))
            throw ErrorInfo(Internal::Tr::tr("Failed to generate %1")
                            .arg(projectFilePath));

        logger.qbsInfo() << Internal::Tr::tr("Generated %1").arg(
                                QFileInfo(projectFilePath).fileName());
    }
}

static void writeWorkspace(const std::shared_ptr<IarewWorkspace> &wokspace,
                           const QString &workspaceFilePath,
                           const Internal::Logger &logger)
{
    Internal::FileSaver file(workspaceFilePath.toStdString());
    if (!file.open())
        throw ErrorInfo(Internal::Tr::tr("Cannot open %s for writing")
                        .arg(workspaceFilePath));

    IarewWorkspaceWriter writer(file.device());
    if (!(writer.write(wokspace.get()) && file.commit()))
        throw ErrorInfo(Internal::Tr::tr("Failed to generate %1")
                        .arg(workspaceFilePath));

    logger.qbsInfo() << Internal::Tr::tr("Generated %1").arg(
                            QFileInfo(workspaceFilePath).fileName());
}

IarewGenerator::IarewGenerator(const gen::VersionInfo &versionInfo)
    : m_versionInfo(versionInfo)
{
}

QString IarewGenerator::generatorName() const
{
    return QStringLiteral("iarew%1").arg(m_versionInfo.marketingVersion());
}

void IarewGenerator::reset()
{
    m_workspace.reset();
    m_workspaceFilePath.clear();
    m_projects.clear();
}

void IarewGenerator::generate()
{
    GeneratableProjectIterator it(project());
    it.accept(this);

    writeProjectFiles(m_projects, logger());
    writeWorkspace(m_workspace, m_workspaceFilePath, logger());

    reset();
}

void IarewGenerator::visitProject(const GeneratableProject &project)
{
    const QDir buildDir = project.baseBuildDirectory();

    m_workspaceFilePath = buildDir.absoluteFilePath(
                project.name() + QStringLiteral(".eww"));
    m_workspace = std::make_shared<IarewWorkspace>(m_workspaceFilePath);
}

void IarewGenerator::visitProjectData(const GeneratableProject &project,
                                      const GeneratableProjectData &projectData)
{
    Q_UNUSED(project)
    Q_UNUSED(projectData)
}

void IarewGenerator::visitProduct(const GeneratableProject &project,
                                  const GeneratableProjectData &projectData,
                                  const GeneratableProductData &productData)
{
    Q_UNUSED(projectData);
    const QString projectFilePath = targetFilePath(
                productData, project.baseBuildDirectory().absolutePath());
    const auto targetProject = std::make_shared<IarewProject>(
                project, productData,
                m_versionInfo);

    m_projects.insert({projectFilePath, targetProject});
    m_workspace->addProject(projectFilePath);
}

} // namespace qbs
