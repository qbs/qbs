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

#include "msbuildsharedsolutionpropertiesproject.h"

#include "msbuild/msbuildpropertygroup.h"

#include <tools/pathutils.h>
#include <tools/shellutils.h>

#include <QtCore/qfileinfo.h>

namespace qbs {

static QString qbsCommandLine(const GeneratableProject &project,
                              const QString &subCommand,
                              const QString &qbsSettingsDir,
                              const Internal::VisualStudioVersionInfo &versionInfo)
{
    auto addEnvironmentVariableArgument = [](Internal::CommandLine &cl, const QString &var,
                                             const QString &prefix = QString()) {
        cl.appendRawArgument(QStringLiteral("\"%1$(%2)\"").arg(prefix, var));
    };

    auto realSubCommand = subCommand;
    if (subCommand == QStringLiteral("rebuild"))
        realSubCommand = QStringLiteral("build");

    // "path/to/qbs.exe" {build|clean}
    // --settings-dir "path/to/settings/directory/"
    // -f "path/to/project.qbs" -d "/build/directory/"
    // -p product_name [[configuration key:value]...]
    Internal::CommandLine commandLine;
    commandLine.setProgram(QStringLiteral("\"$(QbsExecutablePath)\""), true);
    commandLine.appendArgument(realSubCommand);

    if (!qbsSettingsDir.isEmpty()) {
        commandLine.appendArgument(QStringLiteral("--settings-dir"));
        addEnvironmentVariableArgument(commandLine, QStringLiteral("QbsSettingsDir"));
    }

    commandLine.appendArgument(QStringLiteral("-f"));
    addEnvironmentVariableArgument(commandLine, QStringLiteral("QbsProjectFile"));
    commandLine.appendArgument(QStringLiteral("-d"));
    addEnvironmentVariableArgument(commandLine, QStringLiteral("QbsBuildDir"));

    if (subCommand == QStringLiteral("generate")) {
        commandLine.appendArgument(QStringLiteral("-g"));
        commandLine.appendArgument(QStringLiteral("visualstudio%1")
                                   .arg(versionInfo.marketingVersion()));
    } else {
        commandLine.appendArgument(QStringLiteral("-p"));
        addEnvironmentVariableArgument(commandLine, QStringLiteral("QbsProductName"));

        commandLine.appendArgument(QStringLiteral("--wait-lock"));
    }

    if (realSubCommand == QStringLiteral("build")
            && !project.installOptions.installRoot().isEmpty()) {
        commandLine.appendArgument(QStringLiteral("--install-root"));
        addEnvironmentVariableArgument(commandLine, QStringLiteral("QbsInstallRoot"));
    }

    if (realSubCommand == QStringLiteral("build") && subCommand == QStringLiteral("rebuild")) {
        commandLine.appendArgument(QStringLiteral("--check-timestamps"));
        commandLine.appendArgument(QStringLiteral("--force-probe-execution"));
    }

    addEnvironmentVariableArgument(commandLine, QStringLiteral("Configuration"),
                                   QStringLiteral("config:"));

    return commandLine.toCommandLine(Internal::HostOsInfo::HostOsWindows);
}

MSBuildSharedSolutionPropertiesProject::MSBuildSharedSolutionPropertiesProject(
        const Internal::VisualStudioVersionInfo &versionInfo,
        const GeneratableProject &project,
        const QFileInfo &qbsExecutable,
        const QString &qbsSettingsDir)
{
    setDefaultTargets(QStringLiteral("Build"));
    setToolsVersion(versionInfo.toolsVersion());

    const auto group = new MSBuildPropertyGroup(this);
    group->setLabel(QStringLiteral("UserMacros"));

    // Order's important here... a variable must be listed before one that uses it
    group->appendProperty(QStringLiteral("QbsExecutablePath"),
                          QStringLiteral("$(QbsExecutableDir)") + qbsExecutable.fileName());
    if (!project.installOptions.installRoot().isEmpty()) {
        group->appendProperty(QStringLiteral("QbsInstallRoot"),
                              Internal::PathUtils::toNativeSeparators(
                                  project.installOptions.installRoot(),
                                  Internal::HostOsInfo::HostOsWindows));
    }

    group->appendProperty(QStringLiteral("QbsProjectFile"),
                          QStringLiteral("$(QbsProjectDir)")
                          + project.filePath().fileName());

    // Trailing '.' is not a typo. It prevents the trailing slash from combining with the closing
    // quote to form an escape sequence. Unfortunately, Visual Studio expands variables *before*
    // passing them to the underlying command shell, so there's not much we can do with regard to
    // doing it "properly". Setting environment variables through MSBuild and using them in place
    // of actual arguments does not work either, as Visual Studio apparently expands the environment
    // variables as well, before passing them to the underlying shell.
    group->appendProperty(QStringLiteral("QbsBuildDir"),
                          QStringLiteral("$(SolutionDir)."));

    group->appendProperty(QStringLiteral("QbsBuildCommandLine"),
                          qbsCommandLine(project, QStringLiteral("build"),
                                         qbsSettingsDir, versionInfo));
    group->appendProperty(QStringLiteral("QbsReBuildCommandLine"),
                          qbsCommandLine(project, QStringLiteral("rebuild"),
                                         qbsSettingsDir, versionInfo));
    group->appendProperty(QStringLiteral("QbsCleanCommandLine"),
                          qbsCommandLine(project, QStringLiteral("clean"),
                                         qbsSettingsDir, versionInfo));
    group->appendProperty(QStringLiteral("QbsGenerateCommandLine"),
                          qbsCommandLine(project, QStringLiteral("generate"),
                                         qbsSettingsDir, versionInfo));
}

} // namespace qbs
