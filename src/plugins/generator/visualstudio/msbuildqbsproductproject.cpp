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

#include "msbuildqbsproductproject.h"

#include "msbuild/msbuildimport.h"
#include "msbuild/msbuildimportgroup.h"
#include "msbuild/msbuilditemdefinitiongroup.h"
#include "msbuild/msbuilditemgroup.h"
#include "msbuild/msbuilditemmetadata.h"
#include "msbuild/msbuildproperty.h"
#include "msbuild/msbuildpropertygroup.h"

#include "msbuild/items/msbuildclcompile.h"
#include "msbuild/items/msbuildclinclude.h"
#include "msbuild/items/msbuildlink.h"
#include "msbuild/items/msbuildnone.h"

#include "msbuildutils.h"
#include "visualstudiogenerator.h"

#include <api/runenvironment.h>
#include <tools/pathutils.h>
#include <tools/shellutils.h>
#include <tools/stringconstants.h>
#include <tools/version.h>

#include <QtCore/qdir.h>
#include <QtCore/quuid.h>

namespace qbs {
using namespace Internal;

MSBuildQbsProductProject::MSBuildQbsProductProject(
        const GeneratableProject &project,
        const GeneratableProductData &product,
        const Internal::VisualStudioVersionInfo &versionInfo,
        VisualStudioGenerator *parent)
    : MSBuildTargetProject(project, versionInfo, parent)
{
    Q_ASSERT(project.projects.size() == project.commandLines.size());
    Q_ASSERT(project.projects.size() == product.data.size());

    const int count = std::max(project.projects.size(), product.data.size());

    globalsPropertyGroup()->appendProperty(QStringLiteral("QbsProductName"), product.name());

    const auto cppDefaultProps = new MSBuildImport(this);
    cppDefaultProps->setProject(QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.Default.props"));

    for (int i = 0; i < count; ++i) {
        addConfiguration(
                    project,
                    project.projects.values().at(i),
                    product.data.values().at(i),
                    project.commandLines.values().at(i));
    }

    const auto cppProps = new MSBuildImport(this);
    cppProps->setProject(QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.props"));

    for (int i = 0; i < count; ++i)
        addItemDefGroup(project.projects.values().at(i), product.data.values().at(i));

    addFiles(project, product);
}

static QString productTargetPath(const qbs::ProductData &productData)
{
    const QString fullPath = productData.targetExecutable();
    if (!fullPath.isEmpty())
        return QFileInfo(fullPath).absolutePath();
    return productData.properties().value(QStringLiteral("buildDirectory")).toString();
}

static bool listEnvironmentVariableContainsValue(const QString &environmentVariable,
                                                 const QString &value)
{
    return environmentVariable.contains(QLatin1Char(';') + value + QLatin1Char(';'))
            || environmentVariable.startsWith(value + QLatin1Char(';'))
            || environmentVariable.endsWith(QLatin1Char(';') + value);
}

void MSBuildQbsProductProject::addConfiguration(const GeneratableProject &project,
                                            const Project &buildTask,
                                            const ProductData &productData,
                                            const QStringList &buildConfigurationCommandLine)
{
    const auto targetDir = Internal::PathUtils::toNativeSeparators(
                productTargetPath(productData), Internal::HostOsInfo::HostOsWindows);

    auto configurationDir = Internal::PathUtils::toNativeSeparators(
                project.baseBuildDirectory().absolutePath()
                    + QLatin1Char('\\')
                    + MSBuildUtils::configurationName(buildTask),
                Internal::HostOsInfo::HostOsWindows);
    auto relativeTargetDir = targetDir;
    if (targetDir.startsWith(configurationDir))
        relativeTargetDir = QStringLiteral("$(SolutionDir)$(Configuration)")
                + relativeTargetDir.mid(configurationDir.size());

    const auto &properties = productData.moduleProperties();

    const bool debugBuild = properties.getModuleProperty(StringConstants::qbsModule(),
                                                         QStringLiteral("debugInformation"))
            .toBool();

    const auto includePaths = QStringList()
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"),
                                                          QStringLiteral("includePaths"))
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"),
                                                          QStringLiteral("systemIncludePaths"));
    const auto cppDefines = properties
            .getModulePropertiesAsStringList(QStringLiteral("cpp"), QStringLiteral("defines"));

    const auto sep = Internal::HostOsInfo::pathListSeparator(Internal::HostOsInfo::HostOsWindows);

    const auto propertyGroup1 = new MSBuildPropertyGroup(this);
    propertyGroup1->setCondition(MSBuildUtils::buildTaskCondition(buildTask));
    propertyGroup1->setLabel(QStringLiteral("Configuration"));
    propertyGroup1->appendProperty(QStringLiteral("UseDebugLibraries"),
                                   debugBuild ? QStringLiteral("true") : QStringLiteral("false"));

    // General - General
    // We need a trailing backslash for $(OutDir); See also the VS documentation:
    // https://docs.microsoft.com/en-us/cpp/ide/common-macros-for-build-commands-and-properties
    propertyGroup1->appendProperty(QStringLiteral("OutDir"), relativeTargetDir + QLatin1Char('\\'));
    propertyGroup1->appendProperty(QStringLiteral("TargetName"), productData.targetName());
    propertyGroup1->appendProperty(QStringLiteral("PlatformToolset"),
                                   versionInfo().platformToolsetVersion());
    propertyGroup1->appendProperty(QStringLiteral("ConfigurationType"), QStringLiteral("Makefile"));

    // VS possible values: Unicode|MultiByte|NotSet
    propertyGroup1->appendProperty(QStringLiteral("CharacterSet"),
        properties.getModuleProperty(QStringLiteral("cpp"),
            QStringLiteral("windowsApiCharacterSet")) == QStringLiteral("unicode")
                ? QStringLiteral("MultiByte") : QStringLiteral("NotSet"));

    // Debugging
    propertyGroup1->appendProperty(QStringLiteral("DebuggerFlavor"),
                                   QStringLiteral("WindowsLocalDebugger"));
    propertyGroup1->appendProperty(QStringLiteral("LocalDebuggerCommand"),
                                   QStringLiteral("$(OutDir)$(TargetName)$(TargetExt)"));
    propertyGroup1->appendProperty(QStringLiteral("LocalDebuggerWorkingDirectory"),
                                   QStringLiteral("$(OutDir)"));

    auto env = buildTask.getRunEnvironment(productData, project.installOptions,
                                           QProcessEnvironment(), QStringList(), nullptr)
            .runEnvironment();
    if (!env.isEmpty()) {
        const auto systemEnv = QProcessEnvironment::systemEnvironment();
        const auto keys = systemEnv.keys();
        for (const auto &key : keys) {
            if (!env.contains(key))
                continue;

            // Don't duplicate keys from the system environment
            if (env.value(key) == systemEnv.value(key)) {
                env.remove(key);
                continue;
            }

            // Cleverly concatenate list variables to avoid duplicating system environment
            const QString systemValue = systemEnv.value(key);
            QString overriddenValue = env.value(key);
            if (listEnvironmentVariableContainsValue(overriddenValue, systemValue)) {
                env.insert(key, overriddenValue.replace(systemValue,
                                                        QLatin1Char('%') + key + QLatin1Char('%')));
            }

            QString installRoot = project.installOptions.installRoot();
            if (!installRoot.isEmpty()) {
                if (listEnvironmentVariableContainsValue(overriddenValue, installRoot)) {
                    env.insert(key, overriddenValue.replace(installRoot,
                        QStringLiteral("$(QbsInstallRoot)")));
                }
            } else {
                installRoot = Internal::PathUtils::toNativeSeparators(
                            QDir(buildTask.projectData().buildDirectory()).absoluteFilePath(
                                project.installOptions.defaultInstallRoot()),
                            Internal::HostOsInfo::HostOsWindows);
                if (listEnvironmentVariableContainsValue(overriddenValue, installRoot)) {
                    env.insert(key, overriddenValue.replace(installRoot,
                        QStringLiteral("$(SolutionDir)$(Configuration)\\install-root")));
                }
            }
        }

        propertyGroup1->appendProperty(QStringLiteral("LocalDebuggerEnvironment"),
                                       env.toStringList().join(QLatin1String("\n")));
    }

    // NMake - General
    // Skip configuration name, that's handled in qbs-shared.props
    const auto params = Internal::shellQuote(buildConfigurationCommandLine.mid(1),
                                                Internal::HostOsInfo::HostOsWindows);
    propertyGroup1->appendProperty(QStringLiteral("NMakeBuildCommandLine"),
                                   QStringLiteral("$(QbsBuildCommandLine) ") + params);
    propertyGroup1->appendProperty(QStringLiteral("NMakeReBuildCommandLine"),
                                   QStringLiteral("$(QbsReBuildCommandLine) ") + params);
    propertyGroup1->appendProperty(QStringLiteral("NMakeCleanCommandLine"),
                                   QStringLiteral("$(QbsCleanCommandLine) ") + params);
    propertyGroup1->appendProperty(QStringLiteral("NMakeOutput"),
                                   QStringLiteral("$(OutDir)$(TargetName)$(TargetExt)"));

    // NMake - IntelliSense
    propertyGroup1->appendProperty(QStringLiteral("NMakePreprocessorDefinitions"),
                                   cppDefines.join(sep));
    propertyGroup1->appendProperty(QStringLiteral("NMakeIncludeSearchPath"),
                                   includePaths.join(sep));
}

static QString subsystemVersion(const QString &version)
{
    const auto v = Version::fromString(version);
    return QStringLiteral("%1.%2").arg(
                QString::number(v.majorVersion()),
                QString::number(v.minorVersion()).rightJustified(2, QLatin1Char('0')));
}

void MSBuildQbsProductProject::addItemDefGroup(const Project &project,
                                               const ProductData &productData)
{
    const auto &properties = productData.moduleProperties();

    const bool consoleApp = productData.properties().value(QStringLiteral("consoleApplication"))
            .toBool();
    const bool debugBuild = properties.getModuleProperty(StringConstants::qbsModule(),
                                                         QStringLiteral("debugInformation"))
            .toBool();
    const auto optimizationLevel = properties.getModuleProperty(StringConstants::qbsModule(),
                                                                QStringLiteral("optimization"))
            .toString();
    const auto warningLevel = properties.getModuleProperty(StringConstants::qbsModule(),
                                                           QStringLiteral("warningLevel"))
            .toString();

    const auto includePaths = QStringList()
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"),
                                                          QStringLiteral("includePaths"))
            << properties.getModulePropertiesAsStringList(QStringLiteral("cpp"),
                                                          QStringLiteral("systemIncludePaths"));
    const auto cppDefines = properties.getModulePropertiesAsStringList(
                QStringLiteral("cpp"), QStringLiteral("defines"));

    const auto sep = Internal::HostOsInfo::pathListSeparator(Internal::HostOsInfo::HostOsWindows);

    const auto itemDefGroup = new MSBuildItemDefinitionGroup(this);
    itemDefGroup->setCondition(MSBuildUtils::buildTaskCondition(project));

    const auto compile = new MSBuildClCompile(itemDefGroup);

    // C++ - General
    compile->appendProperty(QStringLiteral("AdditionalIncludeDirectories"),
                            includePaths.join(sep)
                                + sep
                                + QStringLiteral("%(AdditionalIncludeDirectories)"));
    if (warningLevel == QStringLiteral("none"))
        compile->appendProperty(QStringLiteral("WarningLevel"),
                                QStringLiteral("TurnOffAllWarnings"));
    else if (warningLevel == QStringLiteral("all"))
        compile->appendProperty(QStringLiteral("WarningLevel"),
                                QStringLiteral("EnableAllWarnings"));
    else
        compile->appendProperty(QStringLiteral("WarningLevel"),
                                QStringLiteral("Level3")); // this is VS default.

    // C++ - Optimization
    compile->appendProperty(QStringLiteral("Optimization"),
                            optimizationLevel == QStringLiteral("none")
                                ? QStringLiteral("Disabled")
                                : QStringLiteral("MaxSpeed"));

    // C++ - Preprocessor
    compile->appendProperty(QStringLiteral("PreprocessorDefinitions"),
                            cppDefines.join(sep)
                                + sep
                                + QStringLiteral("%(PreprocessorDefinitions)"));

    // C++ - Code Generation
    compile->appendProperty(QStringLiteral("RuntimeLibrary"), debugBuild
                            ? QStringLiteral("MultiThreadedDebugDLL")
                            : QStringLiteral("MultiThreadedDLL"));

    const auto link = new MSBuildLink(itemDefGroup);

    // Linker - General
    link->appendProperty(QStringLiteral("AdditionalLibraryDirectories"),
        properties.getModulePropertiesAsStringList(QStringLiteral("cpp"),
                                                   QStringLiteral("libraryPaths")).join(sep));

    // Linker - Input
    link->appendProperty(QStringLiteral("AdditionalDependencies"),
        properties.getModulePropertiesAsStringList(QStringLiteral("cpp"),
                                                   QStringLiteral("staticLibraries")).join(sep)
                                            + sep + QStringLiteral("%(AdditionalDependencies)"));

    // Linker - Debugging
    link->appendProperty(QStringLiteral("GenerateDebugInformation"),
                         debugBuild ? QStringLiteral("true") : QStringLiteral("false"));

    // Linker - System
    link->appendProperty(QStringLiteral("SubSystem"),
                         consoleApp ? QStringLiteral("Console") : QStringLiteral("Windows"));
    const auto subsysVersion = properties.getModuleProperty(
                QStringLiteral("cpp"), QStringLiteral("minimumWindowsVersion")).toString();
    if (!subsysVersion.isEmpty())
        link->appendProperty(QStringLiteral("MinimumRequiredVersion"),
                             subsystemVersion(subsysVersion));

    // Linker - Optimization
    link->appendProperty(QStringLiteral("OptimizeReferences"),
                         debugBuild ? QStringLiteral("false") : QStringLiteral("true"));
}

static MSBuildFileItem *fileItemForFileTags(const QList<QString> &fileTags,
                                            IMSBuildItemGroup *parent = nullptr)
{
    if (fileTags.contains(QStringLiteral("hpp")))
        return new MSBuildClInclude(parent);
    if (fileTags.contains(QStringLiteral("c")) || fileTags.contains(QStringLiteral("cpp")))
        return new MSBuildClCompile(parent);
    return new MSBuildNone(parent);
}

void MSBuildQbsProductProject::addFiles(const GeneratableProject &project,
                                        const GeneratableProductData &product)
{
    const auto itemGroup = new MSBuildItemGroup(this);

    addQbsFile(project, product, itemGroup);

    std::map<QString, MSBuildFileItem *> sourceFileNodes;
    std::map<QString, QStringList> sourceFileEnabledConfigurations;

    // Create a ClCompile item for each source file, keeping track of which configurations that
    // file's containing group is enabled in
    QMapIterator<QString, qbs::ProductData> productDataIt(product.data);
    while (productDataIt.hasNext()) {
        productDataIt.next();
        const auto groups = productDataIt.value().groups();
        for (const auto &group : groups) {
            const auto sourceArtifacts = group.allSourceArtifacts();
            for (const auto &sourceArtifact : sourceArtifacts) {
                const auto filePath = sourceArtifact.filePath();
                if (sourceFileNodes.find(filePath) == sourceFileNodes.end()) {
                    sourceFileNodes.insert({
                        filePath,
                        fileItemForFileTags(sourceArtifact.fileTags(), itemGroup)
                    });
                }
                auto fileItem = sourceFileNodes[filePath];
                QString path = project.baseBuildDirectory().relativeFilePath(filePath);
                // The path still might not be relative (for example if the file item is
                // located on a different drive)
                if (QFileInfo(path).isRelative())
                    path = QStringLiteral("$(ProjectDir)") + path;
                fileItem->setFilePath(path);
                if (group.isEnabled())
                    sourceFileEnabledConfigurations[filePath] << productDataIt.key();
            }
        }
    }

    // Add ExcludedFromBuild item metadata to each file for each configuration
    // where that file's containing group is disabled
    for (const auto &sourceFileNode : sourceFileNodes) {
        QMapIterator<QString, qbs::Project> projIt(project.projects);
        while (projIt.hasNext()) {
            projIt.next();
            if (!sourceFileEnabledConfigurations[sourceFileNode.first].contains(projIt.key())) {
                const auto metadata = new MSBuildItemMetadata(
                            QStringLiteral("ExcludedFromBuild"),
                            QStringLiteral("true"),
                            sourceFileNode.second);
                metadata->setCondition(QStringLiteral("'$(Configuration)|$(Platform)'=='")
                    + MSBuildUtils::fullName(projIt.value())
                    + QStringLiteral("'"));
            }
        }
    }

    const auto import = new MSBuildImport(this);
    import->setProject(QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.targets"));
}

void MSBuildQbsProductProject::addQbsFile(const GeneratableProject &project,
                                          const GeneratableProductData &product,
                                          MSBuildItemGroup *itemGroup)
{
    const auto fileItem = new MSBuildNone(itemGroup);
    QString path = project.baseBuildDirectory().relativeFilePath(product.location().filePath());
    // The path still might not be relative (for example if the file item is
    // located on a different drive)
    if (QFileInfo(path).isRelative())
        path = QStringLiteral("$(ProjectDir)") + path;
    fileItem->setFilePath(path);
}


} // namespace qbs
