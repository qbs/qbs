/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "probe.h"
#include "gccprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/toolchains.h>

#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

static QString qsystem(const QString &exe, const QStringList &args = QStringList())
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(exe, args);
    if (!p.waitForStarted()) {
        throw qbs::ErrorInfo(Tr::tr("Failed to start compiler '%1': %2")
                             .arg(exe, p.errorString()));
    }
    if (!p.waitForFinished(-1) || p.exitCode() != 0)
        throw qbs::ErrorInfo(Tr::tr("Failed to run compiler '%1': %2")
                             .arg(exe, p.errorString()));
    return QString::fromLocal8Bit(p.readAll());
}

static QStringList validMinGWMachines()
{
    // List of MinGW machine names (gcc -dumpmachine) recognized by Qbs
    return {QStringLiteral("mingw32"),
            QStringLiteral("mingw64"),
            QStringLiteral("i686-w64-mingw32"),
            QStringLiteral("x86_64-w64-mingw32"),
            QStringLiteral("i686-w64-mingw32.shared"),
            QStringLiteral("x86_64-w64-mingw32.shared"),
            QStringLiteral("i686-w64-mingw32.static"),
            QStringLiteral("x86_64-w64-mingw32.static"),
            QStringLiteral("i586-mingw32msvc"),
            QStringLiteral("amd64-mingw32msvc")};
}

static QString gccMachineName(const QFileInfo &compiler)
{
    return qsystem(compiler.absoluteFilePath(), {QStringLiteral("-dumpmachine")})
            .trimmed();
}

static QStringList standardCompilerFileNames()
{
    return {HostOsInfo::appendExecutableSuffix(QStringLiteral("gcc")),
            HostOsInfo::appendExecutableSuffix(QStringLiteral("g++")),
            HostOsInfo::appendExecutableSuffix(QStringLiteral("clang")),
            HostOsInfo::appendExecutableSuffix(QStringLiteral("clang++"))};
}

class ToolchainDetails
{
public:
    explicit ToolchainDetails(const QFileInfo &compiler)
    {
        auto baseName = compiler.completeBaseName();
        if (baseName.contains(QRegExp(QLatin1String("\\d$")))) {
            const auto dashIndex = baseName.lastIndexOf(QLatin1Char('-'));
            version = baseName.mid(dashIndex + 1);
            baseName = baseName.left(dashIndex);
        }

        const auto dashIndex = baseName.lastIndexOf(QLatin1Char('-'));
        suffix = baseName.mid(dashIndex + 1);
        prefix = baseName.left(dashIndex + 1);
    }

    QString prefix;
    QString suffix;
    QString version;
};

static void setCommonProperties(Profile &profile, const QFileInfo &compiler,
        const QStringList &toolchainTypes, const ToolchainDetails &details)
{
    if (toolchainTypes.contains(QStringLiteral("mingw")))
        profile.setValue(QStringLiteral("qbs.targetPlatform"),
                         QStringLiteral("windows"));

    if (!details.prefix.isEmpty())
        profile.setValue(QStringLiteral("cpp.toolchainPrefix"), details.prefix);

    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"),
                     compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchain"), toolchainTypes);

    if (!standardCompilerFileNames().contains(
                HostOsInfo::appendExecutableSuffix(details.suffix))) {
        qWarning("%s", qPrintable(
                     QStringLiteral("'%1' is not a standard compiler file name; "
                                    "you must set the cpp.cCompilerName and "
                                    "cpp.cxxCompilerName properties of this profile "
                                    "manually").arg(compiler.fileName())));
    }
}

class ToolPathSetup
{
public:
    explicit ToolPathSetup(Profile *profile, QString path, ToolchainDetails details)
        : m_profile(profile),
          m_compilerDirPath(std::move(path)),
          m_details(std::move(details))
    {
    }

    void apply(const QString &toolName, const QString &propertyName) const
    {
        // Check for full tool name at first (includes suffix and version).
        QString filePath = toolFilePath(toolName, UseFullToolName);
        if (filePath.isEmpty()) {
            // Check for base tool name at second (without of suffix and version).
            filePath = toolFilePath(toolName, UseBaseToolName);
            if (filePath.isEmpty()) {
                // Check for short tool name at third (only a tool name).
                filePath = toolFilePath(toolName, UseOnlyShortToolName);
            }
        }

        if (filePath.isEmpty()) {
            qWarning("%s", qPrintable(
                         QStringLiteral("'%1' not found in '%2'. "
                                        "Qbs will try to find it in PATH at build time.")
                         .arg(toolName, m_compilerDirPath)));
        }

        m_profile->setValue(propertyName, filePath);
    }

private:
    enum ToolNameParts : quint8 {
        UseOnlyShortToolName = 0x0,
        UseToolPrefix = 0x01,
        UseToolSuffix = 0x02,
        UseToolVersion = 0x04,
        UseFullToolName = UseToolPrefix | UseToolSuffix | UseToolVersion,
        UseBaseToolName = UseToolPrefix,
    };

    QString toolFilePath(const QString &toolName, int parts) const
    {
        QString fileName;
        if ((parts & UseToolPrefix) && !m_details.prefix.isEmpty())
            fileName += m_details.prefix;
        if ((parts & UseToolSuffix) && !m_details.suffix.isEmpty())
            fileName += m_details.suffix + QLatin1Char('-');
        fileName += toolName;
        if ((parts & UseToolVersion) && !m_details.version.isEmpty())
            fileName += QLatin1Char('-') + m_details.version;

        fileName = HostOsInfo::appendExecutableSuffix(fileName);
        QString filePath = QDir(m_compilerDirPath).absoluteFilePath(fileName);
        if (QFile::exists(filePath))
            return filePath;
        return {};
    }

    Profile * const m_profile;
    QString m_compilerDirPath;
    ToolchainDetails m_details;
};

static bool doesProfileTargetOS(const Profile &profile, const QString &os)
{
    const auto target = profile.value(QStringLiteral("qbs.targetPlatform"));
    if (target.isValid()) {
        return Internal::contains(HostOsInfo::canonicalOSIdentifiers(
                                      target.toString().toStdString()),
                                  os.toStdString());
    }
    return Internal::contains(HostOsInfo::hostOSIdentifiers(), os.toStdString());
}

static QString buildProfileName(const QFileInfo &cfi)
{
    // We need to replace a dot-separated compiler version string
    // with a underscore-separated string, because the profile
    // name does not allow a dots.
    auto result = cfi.completeBaseName();
    result.replace(QLatin1Char('.'), QLatin1Char('_'));
    return result;
}

static QStringList buildCompilerNameFilters(const QString &compilerName)
{
    QStringList filters = {
        // "clang", "gcc"
        compilerName,
        // "clang-8", "gcc-5"
        compilerName + QLatin1String("-[1-9]*"),
        // "avr-gcc"
        QLatin1String("*-") + compilerName,
        // "arm-none-eabi-gcc"
        QLatin1String("*-*-*-") + compilerName,
        // "arm-none-eabi-gcc-9.1.0"
        QLatin1String("*-*-*-") + compilerName + QLatin1String("-[1-9]*"),
         // "x86_64-pc-linux-gnu-gcc"
        QLatin1String("*-*-*-*-") + compilerName,
        // "x86_64-pc-linux-gnu-gcc-7.4.1"
        QLatin1String("*-*-*-*-") + compilerName + QLatin1String("-[1-9]*")
    };

    std::transform(filters.begin(), filters.end(), filters.begin(),
                   [](const auto &filter) {
        return HostOsInfo::appendExecutableSuffix(filter);
    });

    return filters;
}

Profile createGccProfile(const QFileInfo &compiler, Settings *settings,
                         const QStringList &toolchainTypes,
                         const QString &profileName)
{
    const QString machineName = gccMachineName(compiler);

    if (toolchainTypes.contains(QLatin1String("mingw"))) {
        if (!validMinGWMachines().contains(machineName)) {
            throw ErrorInfo(Tr::tr("Detected gcc platform '%1' is not supported.")
                            .arg(machineName));
        }
    }

    Profile profile(!profileName.isEmpty() ? profileName : machineName, settings);
    profile.removeProfile();

    const ToolchainDetails details(compiler);

    setCommonProperties(profile, compiler, toolchainTypes, details);

    if (!toolchainTypes.contains(QLatin1String("clang"))) {
        // Check whether auxiliary tools reside within the toolchain's install path.
        // This might not be the case when using icecc or another compiler wrapper.
        const QString compilerDirPath = compiler.absolutePath();
        const ToolPathSetup toolPathSetup(&profile, compilerDirPath, details);
        toolPathSetup.apply(QStringLiteral("ar"), QStringLiteral("cpp.archiverPath"));
        toolPathSetup.apply(QStringLiteral("as"), QStringLiteral("cpp.assemblerPath"));
        toolPathSetup.apply(QStringLiteral("nm"), QStringLiteral("cpp.nmPath"));
        if (doesProfileTargetOS(profile, QStringLiteral("darwin")))
            toolPathSetup.apply(QStringLiteral("dsymutil"),
                                QStringLiteral("cpp.dsymutilPath"));
        else
            toolPathSetup.apply(QStringLiteral("objcopy"),
                                QStringLiteral("cpp.objcopyPath"));
        toolPathSetup.apply(QStringLiteral("strip"),
                            QStringLiteral("cpp.stripPath"));
    }

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                 .arg(profile.name(), compiler.absoluteFilePath());
    return profile;
}

void gccProbe(Settings *settings, QList<Profile> &profiles, const QString &compilerName)
{
    qbsInfo() << Tr::tr("Trying to detect %1...").arg(compilerName);

    const QStringList searchPaths = systemSearchPaths();

    std::vector<QFileInfo> candidates;
    const auto filters = buildCompilerNameFilters(compilerName);
    for (const auto &searchPath : searchPaths) {
        const QDir dir(searchPath);
        const QStringList fileNames = dir.entryList(
                    filters, QDir::Files | QDir::Executable);
        for (const QString &fileName : fileNames) {
            // Ignore unexpected compiler names.
            if (fileName.startsWith(QLatin1String("c89-gcc"))
                    || fileName.startsWith(QLatin1String("c99-gcc"))) {
                continue;
            }
            const QFileInfo candidate = dir.filePath(fileName);
            // Filter duplicates.
            const auto existingEnd = candidates.end();
            const auto existingIt = std::find_if(
                        candidates.begin(), existingEnd,
                        [candidate](const QFileInfo &existing) {
                return isSameExecutable(candidate.absoluteFilePath(),
                                        existing.absoluteFilePath());
            });
            if (existingIt == existingEnd) {
                // No duplicates are found, just add a new candidate.
                candidates.push_back(candidate);
            } else {
                // Replace the existing entry if a candidate name more than
                // an existing name.
                const auto candidateName = candidate.completeBaseName();
                const auto existingName = existingIt->completeBaseName();
                if (candidateName > existingName)
                    *existingIt = candidate;
            }
        }
    }

    if (candidates.empty()) {
        qbsInfo() << Tr::tr("No %1 toolchains found.").arg(compilerName);
        return;
    }

    for (const auto &candidate : qAsConst(candidates)) {
        const QStringList toolchainTypes = toolchainTypeFromCompilerName(
                    candidate.baseName());
        const QString profileName = buildProfileName(candidate);
        auto profile = createGccProfile(candidate, settings,
                                        toolchainTypes, profileName);
        profiles.push_back(std::move(profile));
    }
}
