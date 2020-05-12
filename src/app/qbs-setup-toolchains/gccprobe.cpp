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
#include <tools/settings.h>
#include <tools/toolchains.h>

#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregularexpression.h>

#include <algorithm>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

constexpr char kUninstallRegistryKey[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\" \
                                         "Windows\\CurrentVersion\\Uninstall\\";

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
        auto baseName = HostOsInfo::stripExecutableSuffix(compiler.fileName());
        // Extract the version sub-string if it exists. We assume that a version
        // sub-string located after the compiler prefix && suffix. E.g. this code
        // parses a version from the compiler names, like this:
        // - avr-gcc-4.9.2.exe
        // - arm-none-eabi-gcc-8.2.1
        // - rl78-elf-gcc-4.9.2.201902-GNURL78
        const QRegularExpression re(QLatin1String("-(\\d+|\\d+\\.\\d+|" \
                                                  "\\d+\\.\\d+\\.\\d+|" \
                                                  "\\d+\\.\\d+\\.\\d+\\.\\d+)" \
                                                  "[-[0-9a-zA-Z]*]?$"));
        const QRegularExpressionMatch match = re.match(baseName);
        if (match.hasMatch()) {
            version = match.captured(1);
            baseName = baseName.left(match.capturedStart());
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
        const QString &toolchainType, const ToolchainDetails &details)
{
    if (toolchainType == QStringLiteral("mingw"))
        profile.setValue(QStringLiteral("qbs.targetPlatform"),
                         QStringLiteral("windows"));

    if (!details.prefix.isEmpty())
        profile.setValue(QStringLiteral("cpp.toolchainPrefix"), details.prefix);

    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"),
                     compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchainType"), toolchainType);

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
        } else {
            m_profile->setValue(propertyName, filePath);
        }
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
        // "avr-gcc-5.4.0"
        QLatin1String("*-") + compilerName + QLatin1String("-[1-9]*"),
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

static QStringList gnuRegistrySearchPaths()
{
    if (!HostOsInfo::isWindowsHost())
        return {};

    QStringList searchPaths;

    QSettings registry(QLatin1String(kUninstallRegistryKey), QSettings::NativeFormat);
    const auto productGroups = registry.childGroups();
    for (const QString &productKey : productGroups) {
        // Registry token for the "GNU Tools for ARM Embedded Processors".
        if (!productKey.startsWith(
                    QLatin1String("GNU Tools for ARM Embedded Processors"))) {
            continue;
        }
        registry.beginGroup(productKey);
        QString uninstallFilePath = registry.value(
                    QLatin1String("UninstallString")).toString();
        if (uninstallFilePath.startsWith(QLatin1Char('"')))
            uninstallFilePath.remove(0, 1);
        if (uninstallFilePath.endsWith(QLatin1Char('"')))
            uninstallFilePath.remove(uninstallFilePath.size() - 1, 1);
        registry.endGroup();

        const QString toolkitRootPath = QFileInfo(uninstallFilePath).path();
        const QString toolchainPath = toolkitRootPath + QLatin1String("/bin");
        searchPaths.push_back(toolchainPath);
    }

    return searchPaths;
}

static QStringList atmelRegistrySearchPaths()
{
    if (!HostOsInfo::isWindowsHost())
        return {};

    // Registry token for the "Atmel" toolchains, e.g. provided by installed
    // "Atmel Studio" IDE.
    static const char kRegistryToken[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Atmel\\";

    QStringList searchPaths;
    QSettings registry(QLatin1String(kRegistryToken), QSettings::NativeFormat);

    // This code enumerate the installed toolchains provided
    // by the Atmel Studio v6.x.
    const auto toolchainGroups = registry.childGroups();
    for (const QString &toolchainKey : toolchainGroups) {
        if (!toolchainKey.endsWith(QLatin1String("GCC")))
            continue;
        registry.beginGroup(toolchainKey);
        const auto entries = registry.childGroups();
        for (const auto &entryKey : entries) {
            registry.beginGroup(entryKey);
            const QString installDir = registry.value(
                        QStringLiteral("Native/InstallDir")).toString();
            const QString version = registry.value(
                        QStringLiteral("Native/Version")).toString();
            registry.endGroup();

            QString toolchainPath = installDir
                    + QLatin1String("/Atmel Toolchain/")
                    + toolchainKey + QLatin1String("/Native/")
                    + version;
            if (toolchainKey.startsWith(QLatin1String("ARM")))
                toolchainPath += QLatin1String("/arm-gnu-toolchain");
            else if (toolchainKey.startsWith(QLatin1String("AVR32")))
                toolchainPath += QLatin1String("/avr32-gnu-toolchain");
            else if (toolchainKey.startsWith(QLatin1String("AVR8)")))
                toolchainPath += QLatin1String("/avr8-gnu-toolchain");
            else
                break;

            toolchainPath += QLatin1String("/bin");

            if (QFileInfo::exists(toolchainPath)) {
                searchPaths.push_back(toolchainPath);
                break;
            }
        }
        registry.endGroup();
    }

    // This code enumerate the installed toolchains provided
    // by the Atmel Studio v7.
    registry.beginGroup(QStringLiteral("AtmelStudio"));
    const auto productVersions = registry.childGroups();
    for (const auto &productVersionKey : productVersions) {
        registry.beginGroup(productVersionKey);
        const QString installDir = registry.value(
                    QStringLiteral("InstallDir")).toString();
        registry.endGroup();

        const QStringList knownToolchainSubdirs = {
            QStringLiteral("/toolchain/arm/arm-gnu-toolchain/bin/"),
            QStringLiteral("/toolchain/avr8/avr8-gnu-toolchain/bin/"),
            QStringLiteral("/toolchain/avr32/avr32-gnu-toolchain/bin/"),
        };

        for (const auto &subdir : knownToolchainSubdirs) {
            const QString toolchainPath = installDir + subdir;
            if (!QFileInfo::exists(toolchainPath))
                continue;
            searchPaths.push_back(toolchainPath);
        }
    }
    registry.endGroup();

    return searchPaths;
}

static QStringList renesasRl78RegistrySearchPaths()
{
    if (!HostOsInfo::isWindowsHost())
        return {};

    QStringList searchPaths;

    QSettings registry(QLatin1String(kUninstallRegistryKey), QSettings::NativeFormat);
    const auto productGroups = registry.childGroups();
    for (const QString &productKey : productGroups) {
        // Registry token for the "Renesas RL78" toolchain.
        if (!productKey.startsWith(
                    QLatin1String("GCC for Renesas RL78"))) {
            continue;
        }
        registry.beginGroup(productKey);
        const QString installLocation = registry.value(
                    QLatin1String("InstallLocation")).toString();
        registry.endGroup();
        if (installLocation.isEmpty())
            continue;

        const QFileInfo toolchainPath = QDir(installLocation).absolutePath()
                + QLatin1String("/rl78-elf/rl78-elf/bin");
        if (!toolchainPath.exists())
            continue;
        searchPaths.push_back(toolchainPath.absoluteFilePath());
    }

    return searchPaths;
}

static QStringList mplabX32RegistrySearchPaths()
{
    if (!HostOsInfo::isWindowsHost())
        return {};

    QStringList searchPaths;

    QSettings registry(QLatin1String(kUninstallRegistryKey), QSettings::NativeFormat);
    const auto productGroups = registry.childGroups();
    for (const QString &productKey : productGroups) {
        // Registry token for the "MPLAB X32" toolchain.
        if (!productKey.startsWith(
                    QLatin1String("MPLAB XC32 Compiler"))) {
            continue;
        }
        registry.beginGroup(productKey);
        const QString installLocation = registry.value(
                    QLatin1String("InstallLocation")).toString();
        registry.endGroup();
        if (installLocation.isEmpty())
            continue;

        const QFileInfo toolchainPath = QDir(installLocation).absolutePath()
                + QLatin1String("/bin");
        if (!toolchainPath.exists())
            continue;
        searchPaths.push_back(toolchainPath.absoluteFilePath());
    }

    return searchPaths;
}

Profile createGccProfile(const QFileInfo &compiler, Settings *settings,
                         const QString &toolchainType,
                         const QString &profileName)
{
    const QString machineName = gccMachineName(compiler);

    if (toolchainType == QLatin1String("mingw")) {
        if (!validMinGWMachines().contains(machineName)) {
            throw ErrorInfo(Tr::tr("Detected gcc platform '%1' is not supported.")
                            .arg(machineName));
        }
    }

    Profile profile(!profileName.isEmpty() ? profileName : machineName, settings);
    profile.removeProfile();

    const ToolchainDetails details(compiler);

    setCommonProperties(profile, compiler, toolchainType, details);

    if (HostOsInfo::isWindowsHost() && toolchainType == QLatin1String("clang")) {
        const QStringList profileNames = settings->profiles();
        bool foundMingw = false;
        for (const QString &profileName : profileNames) {
            const Profile otherProfile(profileName, settings);
            if (otherProfile.value(QLatin1String("qbs.toolchainType")).toString()
                    == QLatin1String("mingw")
                    || otherProfile.value(QLatin1String("qbs.toolchain"))
                    .toStringList().contains(QLatin1String("mingw"))) {
                const QFileInfo tcDir(otherProfile.value(QLatin1String("cpp.toolchainInstallPath"))
                        .toString());
                if (!tcDir.fileName().isEmpty() && tcDir.exists()) {
                    profile.setValue(QLatin1String("qbs.sysroot"), tcDir.path());
                    foundMingw = true;
                    break;
                }
            }
        }
        if (!foundMingw) {
            qbsWarning() << Tr::tr("Using clang on Windows requires a mingw installation. "
                                   "Please set qbs.sysroot accordingly for profile '%1'.")
                            .arg(profile.name());
        }
    }

    if (toolchainType != QLatin1String("clang")) {
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

void gccProbe(Settings *settings, std::vector<Profile> &profiles, const QString &compilerName)
{
    qbsInfo() << Tr::tr("Trying to detect %1...").arg(compilerName);

    QStringList searchPaths;
    searchPaths << systemSearchPaths()
                << gnuRegistrySearchPaths()
                << atmelRegistrySearchPaths()
                << renesasRl78RegistrySearchPaths()
                << mplabX32RegistrySearchPaths();

    std::vector<QFileInfo> candidates;
    const auto filters = buildCompilerNameFilters(compilerName);
    for (const auto &searchPath : qAsConst(searchPaths)) {
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

    // Sort candidates so that mingw comes first. Information from mingw profiles is potentially
    // used for setting up clang profiles.
    if (HostOsInfo::isWindowsHost()) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const QFileInfo &fi1, const QFileInfo &fi2) {
            return fi1.absoluteFilePath().contains(QLatin1String("mingw"))
                    && !fi2.absoluteFilePath().contains(QLatin1String("mingw"));
        });
    }

    for (const auto &candidate : qAsConst(candidates)) {
        const QString toolchainType = toolchainTypeFromCompilerName(
                    candidate.baseName());
        const QString profileName = buildProfileName(candidate);
        try {
            auto profile = createGccProfile(candidate, settings,
                                            toolchainType, profileName);
            profiles.push_back(std::move(profile));
        } catch (const qbs::ErrorInfo &info) {
            qbsWarning() << Tr::tr("Skipping %1: %2").arg(profileName, info.toString());
        }
    }
}
