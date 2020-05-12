/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "makefilegenerator.h"

#include <language/filetags.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/shellutils.h>
#include <tools/stringconstants.h>
#include <tools/set.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qtextstream.h>

#include <utility>
#include <vector>

namespace qbs {
using namespace Internal;

QString qbs::MakefileGenerator::generatorName() const
{
    return QStringLiteral("makefile");
}

static QString quote(const QString &s)
{
    QString quoted = shellQuote(s);
    quoted.replace(QLatin1Char('$'), QLatin1String("$$")); // For make
    quoted.replace(QLatin1String("$$(SRCDIR)"), QLatin1String("$(SRCDIR)"));
    quoted.replace(QLatin1String("$$(BUILD_ROOT)"), QLatin1String("$(BUILD_ROOT)"));
    quoted.replace(QLatin1String("$$(INSTALL_ROOT)"), QLatin1String("$(INSTALL_ROOT)"));
    return quoted;
}

enum class TargetType { Product, Path };
static QString makeValidTargetName(const QString &name, TargetType targetType)
{
    QString modifiedName = name;
    switch (targetType) {
    case TargetType::Product: {
        static const QRegularExpression illegalChar(QStringLiteral("[^_.0-9A-Za-z]"));
        modifiedName.replace(illegalChar, QStringLiteral("_"));
        break;
    }
    case TargetType::Path:
        if (HostOsInfo::isWindowsHost()) {
            modifiedName = QDir::toNativeSeparators(modifiedName);
            modifiedName = quote(modifiedName);
        } else {
            modifiedName.replace(QLatin1Char(' '), QStringLiteral("\\ "));
        }
    }
    return modifiedName;
}

static QString makeValidTargetName(const ProductData &product)
{
    QString name = makeValidTargetName(product.name(), TargetType::Product);
    if (!product.multiplexConfigurationId().isEmpty())
        name.append(QLatin1Char('_')).append(product.multiplexConfigurationId());
    return name;
}

using PrefixSpec = std::pair<QString, QString>;
static QString replacePrefix(const QString &path, const std::vector<PrefixSpec> &candidates)
{
    for (const PrefixSpec &prefixSpec : candidates) {
        if (path.startsWith(prefixSpec.first)
                && (path.size() == prefixSpec.first.size()
                    || path.at(prefixSpec.first.size()) == QLatin1Char('/'))) {
            QString p = path;
            return p.replace(0, prefixSpec.first.size(),
                             QLatin1String("$(") + prefixSpec.second + QLatin1Char(')'));
        }
    }
    return path;
}

static QString bruteForcePathReplace(const QString &value, const QString &srcDir,
                                     const QString &buildDir, const QString &installRoot)
{
    QString transformedValue = value;
    if (!installRoot.isEmpty())
        transformedValue.replace(installRoot, QStringLiteral("$(INSTALL_ROOT)"));
    transformedValue.replace(buildDir, QStringLiteral("$(BUILD_ROOT)"));
    transformedValue.replace(srcDir, QStringLiteral("$(SRCDIR)"));
    return transformedValue;
}

static QString mkdirCmdLine(const QString &dir)
{
    if (HostOsInfo::isWindowsHost())
        return QStringLiteral("if not exist %1 mkdir %1 & if not exist %1 exit 1").arg(dir);
    return QStringLiteral("mkdir -p ") + dir;
}

static QString installFileCommand()
{
    return HostOsInfo::isWindowsHost() ? QStringLiteral("copy /Y")
                                       : QStringLiteral("install -m 644 -p");
}

static QString installProgramCommand()
{
    return HostOsInfo::isWindowsHost() ? installFileCommand()
                                       : QStringLiteral("install -m 755 -p");
}

static QString removeCommand()
{
    return HostOsInfo::isWindowsHost() ? QStringLiteral("del") : QStringLiteral("rm -f");
}

void qbs::MakefileGenerator::generate()
{
    const auto projects = project().projects.values();
    for (const Project &theProject : projects) {
        const QString makefileFilePath = theProject.projectData().buildDirectory()
                + QLatin1String("/Makefile");
        QFile makefile(makefileFilePath);
        if (!makefile.open(QIODevice::WriteOnly)) {
            throw ErrorInfo(Tr::tr("Failed to create '%1': %2")
                            .arg(makefileFilePath, makefile.errorString()));
        }
        QTextStream stream(&makefile);
        ErrorInfo error;
        const ProjectTransformerData projectTransformerData = theProject.transformerData(&error);
        if (error.hasError())
            throw error;
        stream << "# This file was generated by qbs" << "\n\n";
        stream << "INSTALL_FILE = " << installFileCommand() << '\n';
        stream << "INSTALL_PROGRAM = " << installProgramCommand() << '\n';
        stream << "RM = " << removeCommand() << '\n';
        stream << '\n';
        const ProjectData projectData = theProject.projectData();
        const QString srcDir = QFileInfo(projectData.location().filePath()).path();
        if (srcDir.contains(QLatin1Char(' '))) {
            throw ErrorInfo(Tr::tr("The project directory '%1' contains space characters, which"
                                   "is not supported by this generator.").arg(srcDir));
        }
        stream << "SRCDIR = " << QDir::toNativeSeparators(srcDir) << '\n';
        const QString &buildDir = projectData.buildDirectory();
        if (buildDir.contains(QLatin1Char(' '))) {
            throw ErrorInfo(Tr::tr("The build directory '%1' contains space characters, which"
                                   "is not supported by this generator.").arg(buildDir));
        }
        stream << "BUILD_ROOT = " << QDir::toNativeSeparators(buildDir) << '\n';
        QString installRoot;
        const QList<ArtifactData> allInstallables = projectData.installableArtifacts();
        if (!allInstallables.empty()) {
            installRoot = allInstallables.first().installData().installRoot();
            if (installRoot.contains(QLatin1Char(' '))) {
                throw ErrorInfo(Tr::tr("The install root '%1' contains space characters, which"
                                       "is not supported by this generator.").arg(installRoot));
            }
            stream << "INSTALL_ROOT = " << QDir::toNativeSeparators(installRoot) << '\n';
        }
        stream << "\nall:\n";
        const std::vector<PrefixSpec> srcDirPrefixSpecs{std::make_pair(srcDir,
                                                                       QStringLiteral("SRCDIR"))};
        const auto prefixifiedSrcDirPath = [&srcDirPrefixSpecs](const QString &path) {
            return replacePrefix(path, srcDirPrefixSpecs);
        };
        const std::vector<PrefixSpec> buildRootPrefixSpecs{
            std::make_pair(buildDir, QStringLiteral("BUILD_ROOT"))};
        const auto prefixifiedBuildDirPath = [&buildRootPrefixSpecs](const QString &path) {
            return replacePrefix(path, buildRootPrefixSpecs);
        };
        const std::vector<PrefixSpec> installRootPrefixSpecs{
            std::make_pair(installRoot, QStringLiteral("INSTALL_ROOT"))};
        const auto prefixifiedInstallDirPath
                = [&installRoot, &installRootPrefixSpecs](const QString &path) {
            if (installRoot.isEmpty())
                return path;
            return replacePrefix(path, installRootPrefixSpecs);
        };
        const auto transformedOutputFilePath = [=](const ArtifactData &output) {
            return makeValidTargetName(prefixifiedBuildDirPath(output.filePath()),
                                       TargetType::Path);
        };
        const auto transformedInputFilePath = [=](const ArtifactData &input) {
            return makeValidTargetName(prefixifiedSrcDirPath(input.filePath()), TargetType::Path);
        };
        const auto transformedArtifactFilePath = [=](const ArtifactData &artifact) {
            return artifact.isGenerated() ? transformedOutputFilePath(artifact)
                                          : transformedInputFilePath(artifact);
        };
        QStringList allTargets;
        QStringList allDefaultTargets;
        QStringList filesCreatedByJsCommands;
        bool jsCommandsEncountered = false;
        for (const auto &d : projectTransformerData) {
            const ProductData productData = d.first;
            const QString productTarget = makeValidTargetName(productData);
            const ProductTransformerData productTransformerData = d.second;
            const bool builtByDefault = productData.properties().value(
                        StringConstants::builtByDefaultProperty()).toBool();
            if (builtByDefault)
                allDefaultTargets.push_back(productTarget);
            allTargets.push_back(productTarget);
            stream << productTarget << ':';
            const auto targetArtifacts = productData.targetArtifacts();
            for (const ArtifactData &ta : targetArtifacts)
                stream << ' ' << transformedOutputFilePath(ta);
            stream << '\n';
            for (const TransformerData &transformerData : productTransformerData) {
                stream << transformedOutputFilePath(transformerData.outputs().constFirst()) << ":";
                const auto inputs = transformerData.inputs();
                for (const ArtifactData &input : inputs)
                    stream << ' ' << transformedArtifactFilePath(input);
                stream << '\n';
                Set<QString> createdDirs;
                const auto outputs = transformerData.outputs();
                for (const ArtifactData &output : outputs) {
                    const QString outputDir = QFileInfo(output.filePath()).path();
                    if (createdDirs.insert(outputDir).second)
                        stream << "\t" << mkdirCmdLine(QDir::toNativeSeparators(
                                                           prefixifiedBuildDirPath(outputDir)))
                               << '\n';
                }
                bool processCommandEncountered = false;
                const auto commands = transformerData.commands();
                for (const RuleCommand &command : commands) {
                    if (command.type() == RuleCommand::JavaScriptCommandType) {
                        jsCommandsEncountered = true;
                        continue;
                    }
                    processCommandEncountered = true;
                    stream << '\t' << QDir::toNativeSeparators(
                                  quote(bruteForcePathReplace(command.executable(), srcDir,
                                                              buildDir, installRoot)));
                    // TODO: Optionally use environment?
                    const auto args = command.arguments();
                    for (const QString &arg : args) {
                        stream << ' '
                               << quote(bruteForcePathReplace(arg, srcDir, buildDir, installRoot));
                    }
                    stream << '\n';
                }
                for (int i = 1; i < transformerData.outputs().size(); ++i) {
                    stream << transformedOutputFilePath(transformerData.outputs().at(i)) << ": "
                           << transformedOutputFilePath(transformerData.outputs().at(i-1)) << '\n';
                }
                if (!processCommandEncountered && builtByDefault) {
                    const auto outputs = transformerData.outputs();
                    for (const ArtifactData &output : outputs)
                        filesCreatedByJsCommands.push_back(output.filePath());
                }
            }
            stream << "install-" << productTarget << ": " << productTarget << '\n';
            Set<QString> createdDirs;
            const auto installableArtifacts = productData.installableArtifacts();
            for (const ArtifactData &artifact : productData.installableArtifacts()) {
                const QString &outputDir = artifact.installData().localInstallDir();
                if (outputDir.contains(QLatin1Char(' '))) {
                    logger().qbsWarning() << Tr::tr("Skipping installation of '%1', because "
                            "target directory '%2' contains spaces.")
                                             .arg(artifact.filePath(), outputDir);
                    continue;
                }
                if (createdDirs.insert(outputDir).second)
                    stream << "\t" << mkdirCmdLine(QDir::toNativeSeparators(
                                                       prefixifiedInstallDirPath(outputDir)))
                           << '\n';
                const QFileInfo fileInfo(artifact.filePath());
                const QString transformedInputFilePath
                        = QDir::toNativeSeparators((artifact.isGenerated()
                                                    ? prefixifiedBuildDirPath(fileInfo.path())
                                                    : prefixifiedSrcDirPath(fileInfo.path()))
                                                   + QLatin1Char('/') + quote(fileInfo.fileName()));
                const QString transformedOutputDir
                        = QDir::toNativeSeparators(prefixifiedInstallDirPath(
                                                       artifact.installData().localInstallDir()));
                stream << "\t"
                       << (artifact.isExecutable() ? "$(INSTALL_PROGRAM) " : "$(INSTALL_FILE) ")
                       << transformedInputFilePath << ' ' << transformedOutputDir << '\n';
            }
            stream << "clean-" << productTarget << ":\n";
            for (const ArtifactData &artifact : productData.generatedArtifacts()) {
                const QFileInfo fileInfo(artifact.filePath());
                const QString transformedFilePath = QDir::toNativeSeparators(
                            prefixifiedBuildDirPath(fileInfo.path())
                            + QLatin1Char('/') + quote(fileInfo.fileName()));
                stream << '\t';
                if (HostOsInfo::isWindowsHost())
                    stream << '-';
                stream << "$(RM) " << transformedFilePath << '\n';
            }
        }

        stream << "all:";
        for (const QString &target : qAsConst(allDefaultTargets))
            stream << ' ' << target;
        stream << '\n';
        stream << "install:";
        for (const QString &target : qAsConst(allDefaultTargets))
            stream << ' ' << "install-" << target;
        stream << '\n';
        stream << "clean:";
        for (const QString &target : qAsConst(allTargets))
            stream << ' ' << "clean-" << target;
        stream << '\n';
        if (!filesCreatedByJsCommands.empty()) {
            logger().qbsWarning() << Tr::tr("Some rules used by this project are not "
                "Makefile-compatible, because they depend entirely on JavaScriptCommands. "
                "The build is probably not fully functional. "
                "Affected build artifacts:\n\t%1")
                    .arg(filesCreatedByJsCommands.join(QLatin1String("\n\t")));
        } else if (jsCommandsEncountered) {
            logger().qbsWarning() << Tr::tr("Some rules in this project use JavaScriptCommands, "
                "which cannot be converted to Makefile-compatible constructs. The build may "
                "not be fully functional.");
        }
        logger().qbsInfo() << Tr::tr("Makefile successfully generated at '%1'.")
                              .arg(makefileFilePath);
    }
}

} // namespace qbs
