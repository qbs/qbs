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

#include "msbuildfiltersproject.h"
#include "msbuildqbsgenerateproject.h"
#include "msbuildsharedsolutionpropertiesproject.h"
#include "msbuildsolutionpropertiesproject.h"
#include "msbuildqbsproductproject.h"
#include "msbuildutils.h"
#include "visualstudiogenerator.h"
#include "visualstudioguidpool.h"

#include "msbuild/msbuildpropertygroup.h"
#include "msbuild/msbuildproject.h"

#include "solution/visualstudiosolution.h"
#include "solution/visualstudiosolutionfileproject.h"
#include "solution/visualstudiosolutionglobalsection.h"
#include "solution/visualstudiosolutionfolderproject.h"

#include "io/msbuildprojectwriter.h"
#include "io/visualstudiosolutionwriter.h"

#include <generators/generatableprojectiterator.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/filesaver.h>
#include <tools/qbsassert.h>
#include <tools/shellutils.h>
#include <tools/visualstudioversioninfo.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

namespace qbs {

using namespace Internal;

class VisualStudioGeneratorPrivate
{
    friend class SolutionDependenciesVisitor;
public:
    VisualStudioGeneratorPrivate(const Internal::VisualStudioVersionInfo &versionInfo)
        : versionInfo(versionInfo) {}

    Internal::VisualStudioVersionInfo versionInfo;

    std::shared_ptr<VisualStudioGuidPool> guidPool;
    std::shared_ptr<VisualStudioSolution> solution;
    QString solutionFilePath;
    QMap<QString, std::shared_ptr<MSBuildProject>> msbuildProjects;
    QMap<QString, VisualStudioSolutionFileProject *> solutionProjects;
    QMap<GeneratableProjectData::Id, VisualStudioSolutionFolderProject *> solutionFolders;
    QList<std::pair<QString, bool>> propertySheetNames;

    void reset();
};

void VisualStudioGeneratorPrivate::reset()
{
    guidPool.reset();
    solution.reset();
    solutionFilePath.clear();
    msbuildProjects.clear();
    solutionProjects.clear();
    solutionFolders.clear();
    propertySheetNames.clear();
}

class SolutionDependenciesVisitor : public IGeneratableProjectVisitor
{
public:
    SolutionDependenciesVisitor(VisualStudioGenerator *generator)
        : generator(generator) {
    }

    void visitProject(const GeneratableProject &project) override {
        Q_UNUSED(project);
        nestedProjects = new VisualStudioSolutionGlobalSection(
                    QStringLiteral("NestedProjects"), generator->d->solution.get());
        generator->d->solution->appendGlobalSection(nestedProjects);
    }

    void visitProjectData(const GeneratableProject &project,
                          const GeneratableProjectData &parentProjectData,
                          const GeneratableProjectData &projectData) override {
        Q_UNUSED(project);
        // The root project will have a null GeneratableProjectData
        // as its parent object (so skip giving it a parent folder)
        if (!parentProjectData.name().isEmpty()) {
            nestedProjects->appendProperty(
                        generator->d->solutionFolders.value(projectData.uniqueName())->guid()
                            .toString(),
                        generator->d->solutionFolders.value(parentProjectData.uniqueName())->guid()
                            .toString());
        }
    }

    void visitProduct(const GeneratableProject &project,
                      const GeneratableProjectData &projectData,
                      const GeneratableProductData &productData) override {
        Q_UNUSED(project);
        Q_UNUSED(projectData);
        const auto dependencies = productData.dependencies();
        for (const auto &dep : dependencies) {
            generator->d->solution->addDependency(
                        generator->d->solutionProjects.value(productData.name()),
                        generator->d->solutionProjects.value(dep));
        }

        nestedProjects->appendProperty(
                    generator->d->solutionProjects.value(productData.name())->guid().toString(),
                    generator->d->solutionFolders.value(projectData.uniqueName())->guid()
                        .toString());
    }

private:
    VisualStudioGenerator *generator = nullptr;
    VisualStudioSolutionGlobalSection *nestedProjects = nullptr;
};

VisualStudioGenerator::VisualStudioGenerator(const VisualStudioVersionInfo &versionInfo)
    : d(new VisualStudioGeneratorPrivate(versionInfo))
{
    if (d->versionInfo.usesVcBuild())
        throw ErrorInfo(Tr::tr("VCBuild (Visual Studio 2008 and below) is not supported"));
    else if (!d->versionInfo.usesMsBuild())
        throw ErrorInfo(Tr::tr("Unknown/unsupported build engine"));
    Q_ASSERT(d->versionInfo.usesSolutions());
}

VisualStudioGenerator::~VisualStudioGenerator() = default;

QString VisualStudioGenerator::generatorName() const
{
    return QStringLiteral("visualstudio%1").arg(d->versionInfo.marketingVersion());
}

void VisualStudioGenerator::addPropertySheets(const GeneratableProject &project)
{
    {
        const auto fileName = QStringLiteral("qbs.props");
        d->propertySheetNames.push_back({ fileName, true });
        d->msbuildProjects.insert(project.baseBuildDirectory().absoluteFilePath(fileName),
                                std::make_shared<MSBuildSolutionPropertiesProject>(
                                    d->versionInfo, project,
                                    qbsExecutableFilePath(), qbsSettingsDir()));
    }

    {
        const auto fileName = QStringLiteral("qbs-shared.props");
        d->propertySheetNames.push_back({ fileName, false });
        d->msbuildProjects.insert(project.baseBuildDirectory().absoluteFilePath(fileName),
                                std::make_shared<MSBuildSharedSolutionPropertiesProject>(
                                    d->versionInfo, project,
                                    qbsExecutableFilePath(), qbsSettingsDir()));
    }
}

void VisualStudioGenerator::addPropertySheets(
        const std::shared_ptr<MSBuildTargetProject> &targetProject)
{
    for (const auto &pair : qAsConst(d->propertySheetNames)) {
        targetProject->appendPropertySheet(
                    QStringLiteral("$(SolutionDir)\\") + pair.first, pair.second);
    }
}

static QString targetFilePath(const QString &baseName, const QString &baseBuildDirectory)
{
    return QDir(baseBuildDirectory).absoluteFilePath(baseName + QStringLiteral(".vcxproj"));
}

static QString targetFilePath(const GeneratableProductData &product,
                              const QString &baseBuildDirectory)
{
    return targetFilePath(product.name(), baseBuildDirectory);
}

static void addDefaultGlobalSections(const GeneratableProject &topLevelProject,
                                     VisualStudioSolution *solution)
{
    const auto configurationPlatformsSection = new VisualStudioSolutionGlobalSection(
                QStringLiteral("SolutionConfigurationPlatforms"), solution);
    solution->appendGlobalSection(configurationPlatformsSection);
    for (const auto &qbsProject : topLevelProject.projects)
        configurationPlatformsSection->appendProperty(MSBuildUtils::fullName(qbsProject),
                                                      MSBuildUtils::fullName(qbsProject));

    const auto projectConfigurationPlatformsSection = new VisualStudioSolutionGlobalSection(
                QStringLiteral("ProjectConfigurationPlatforms"), solution);
    solution->appendGlobalSection(projectConfigurationPlatformsSection);
    projectConfigurationPlatformsSection->setPost(true);
    const auto projects = solution->projects();
    for (const auto project : projects) {
        for (const auto &qbsProject : topLevelProject.projects) {
            projectConfigurationPlatformsSection->appendProperty(
                QStringLiteral("%1.%2.ActiveCfg")
                        .arg(project->guid().toString())
                        .arg(MSBuildUtils::fullDisplayName(qbsProject)),
                MSBuildUtils::fullName(qbsProject));
            projectConfigurationPlatformsSection->appendProperty(
                QStringLiteral("%1.%2.Build.0")
                        .arg(project->guid().toString())
                        .arg(MSBuildUtils::fullDisplayName(qbsProject)),
                MSBuildUtils::fullName(qbsProject));
        }
    }

    const auto solutionPropsSection = new VisualStudioSolutionGlobalSection(
                QStringLiteral("SolutionProperties"), solution);
    solution->appendGlobalSection(solutionPropsSection);
    solutionPropsSection->appendProperty(QStringLiteral("HideSolutionNode"),
                                         QStringLiteral("FALSE"));
}

static void writeProjectFiles(const QMap<QString, std::shared_ptr<MSBuildProject>> &projects)
{
    // Write out all the MSBuild project files to disk
    QMapIterator<QString, std::shared_ptr<MSBuildProject>> it(projects);
    while (it.hasNext()) {
        it.next();
        const auto projectFilePath = it.key();
        Internal::FileSaver file(projectFilePath.toStdString());
        if (!file.open())
            throw ErrorInfo(Tr::tr("Cannot open %s for writing").arg(projectFilePath));

        std::shared_ptr<MSBuildProject> project = it.value();
        MSBuildProjectWriter writer(file.device());
        if (!(writer.write(project.get()) && file.commit()))
            throw ErrorInfo(Tr::tr("Failed to generate %1").arg(projectFilePath));
    }
}

static void writeSolution(const std::shared_ptr<VisualStudioSolution> &solution,
                          const QString &solutionFilePath,
                          const Internal::Logger &logger)
{
    Internal::FileSaver file(solutionFilePath.toStdString());
    if (!file.open())
        throw ErrorInfo(Tr::tr("Cannot open %s for writing").arg(solutionFilePath));

    VisualStudioSolutionWriter writer(file.device());
    writer.setProjectBaseDirectory(QFileInfo(solutionFilePath).path().toStdString());
    if (!(writer.write(solution.get()) && file.commit()))
        throw ErrorInfo(Tr::tr("Failed to generate %1").arg(solutionFilePath));

    logger.qbsInfo() << Tr::tr("Generated %1").arg(QFileInfo(solutionFilePath).fileName());
}

void VisualStudioGenerator::generate()
{
    GeneratableProjectIterator it(project());
    it.accept(this);

    addDefaultGlobalSections(project(), d->solution.get());

    // Second pass: connection solution project interdependencies and project nesting hierarchy
    SolutionDependenciesVisitor solutionDependenciesVisitor(this);
    it.accept(&solutionDependenciesVisitor);

    writeProjectFiles(d->msbuildProjects);
    writeSolution(d->solution, d->solutionFilePath, logger());

    d->reset();
}

void VisualStudioGenerator::visitProject(const GeneratableProject &project)
{
    addPropertySheets(project);

    const auto buildDir = project.baseBuildDirectory();

    d->guidPool = std::make_shared<VisualStudioGuidPool>(
                buildDir.absoluteFilePath(project.name()
                                          + QStringLiteral(".guid.txt")).toStdString());

    d->solutionFilePath = buildDir.absoluteFilePath(project.name() + QStringLiteral(".sln"));
    d->solution = std::make_shared<VisualStudioSolution>(d->versionInfo);

    // Create a helper project to re-run qbs generate
    const auto qbsGenerate = QStringLiteral("qbs-generate");
    const auto projectFilePath = targetFilePath(qbsGenerate, buildDir.absolutePath());
    const auto relativeProjectFilePath = QFileInfo(d->solutionFilePath).dir()
            .relativeFilePath(projectFilePath);
    auto targetProject = std::make_shared<MSBuildQbsGenerateProject>(project, d->versionInfo);
    targetProject->setGuid(d->guidPool->drawProductGuid(relativeProjectFilePath.toStdString()));
    d->msbuildProjects.insert(projectFilePath, targetProject);

    addPropertySheets(targetProject);

    const auto solutionProject = new VisualStudioSolutionFileProject(
                targetFilePath(qbsGenerate, project.baseBuildDirectory().absolutePath()),
                d->solution.get());
    solutionProject->setGuid(targetProject->guid());
    d->solution->appendProject(solutionProject);
    d->solutionProjects.insert(qbsGenerate, solutionProject);
}

void VisualStudioGenerator::visitProjectData(const GeneratableProject &project,
                                             const GeneratableProjectData &projectData)
{
    Q_UNUSED(project);
    const auto solutionFolder = new VisualStudioSolutionFolderProject(d->solution.get());
    solutionFolder->setName(projectData.name());
    d->solution->appendProject(solutionFolder);
    QBS_CHECK(!d->solutionFolders.contains(projectData.uniqueName()));
    d->solutionFolders.insert(projectData.uniqueName(), solutionFolder);
}

void VisualStudioGenerator::visitProduct(const GeneratableProject &project,
                                         const GeneratableProjectData &projectData,
                                         const GeneratableProductData &productData)
{
    Q_UNUSED(projectData);
    const auto projectFilePath = targetFilePath(productData,
                                                project.baseBuildDirectory().absolutePath());
    const auto relativeProjectFilePath = QFileInfo(d->solutionFilePath)
            .dir().relativeFilePath(projectFilePath);
    auto targetProject = std::make_shared<MSBuildQbsProductProject>(project, productData,
                                                                          d->versionInfo);
    targetProject->setGuid(d->guidPool->drawProductGuid(relativeProjectFilePath.toStdString()));

    addPropertySheets(targetProject);

    d->msbuildProjects.insert(projectFilePath, targetProject);
    d->msbuildProjects.insert(projectFilePath + QStringLiteral(".filters"),
                          std::make_shared<MSBuildFiltersProject>(productData));

    const auto solutionProject = new VisualStudioSolutionFileProject(
                targetFilePath(productData, project.baseBuildDirectory().absolutePath()),
                d->solution.get());
    solutionProject->setGuid(targetProject->guid());
    d->solution->appendProject(solutionProject);
    d->solutionProjects.insert(productData.name(), solutionProject);
}

} // namespace qbs
