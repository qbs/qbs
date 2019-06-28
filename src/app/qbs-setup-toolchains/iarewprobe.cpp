/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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
#include "iarewprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qlist.h>
#include <QtCore/qsettings.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownIarCompilerNames()
{
    return {QStringLiteral("icc8051"), QStringLiteral("iccarm"), QStringLiteral("iccavr")};
}

static QString guessIarArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("icc8051"))
        return QStringLiteral("mcs51");
    if (baseName == QLatin1String("iccarm"))
        return QStringLiteral("arm");
    if (baseName == QLatin1String("iccavr"))
        return QStringLiteral("avr");
    return {};
}

static Profile createIarProfileHelper(const ToolchainInstallInfo &info,
                                      Settings *settings,
                                      QString profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;
    const QString architecture = guessIarArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty()) {
        if (!info.compilerVersion.isValid()) {
            profileName = QStringLiteral("iar-unknown-%1").arg(architecture);
        } else {
            const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                  QLatin1Char('_'));
            profileName = QStringLiteral("iar-%1-%2").arg(
                        version, architecture);
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QLatin1String("qbs.toolchainType"), QLatin1String("iar"));
    if (!architecture.isEmpty())
        profile.setValue(QLatin1String("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                     profile.name(), compiler.absoluteFilePath());
    return profile;
}

static std::vector<ToolchainInstallInfo> installedIarsFromPath()
{
    std::vector<ToolchainInstallInfo> infos;
    const auto compilerNames = knownIarCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo iarPath(
                    findExecutable(
                        HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!iarPath.exists())
            continue;
        infos.push_back({iarPath, Version{}});
    }
    return infos;
}

static std::vector<ToolchainInstallInfo> installedIarsFromRegistry()
{
    std::vector<ToolchainInstallInfo> infos;

    if (HostOsInfo::isWindowsHost()) {

#ifdef Q_OS_WIN64
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\IAR Systems\\Embedded Workbench";
#else
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\IAR Systems\\Embedded Workbench";
#endif

        // Dictionary for know toolchains.
        static const struct Entry {
            QString registryKey;
            QString subExePath;
        } knowToolchains[] = {
            {QStringLiteral("EWARM"), QStringLiteral("\\arm\\bin\\iccarm.exe")},
            {QStringLiteral("EWAVR"), QStringLiteral("\\avr\\bin\\iccavr.exe")},
            {QStringLiteral("EW8051"), QStringLiteral("\\8051\\bin\\icc8051.exe")},
        };

        QSettings registry(QLatin1String(kRegistryNode), QSettings::NativeFormat);
        const auto oneLevelGroups = registry.childGroups();
        for (const QString &oneLevelKey : oneLevelGroups) {
            registry.beginGroup(oneLevelKey);
            const auto twoLevelGroups = registry.childGroups();
            for (const Entry &entry : knowToolchains) {
                if (twoLevelGroups.contains(entry.registryKey)) {
                    registry.beginGroup(entry.registryKey);
                    const auto threeLevelGroups = registry.childGroups();
                    for (const QString &threeLevelKey : threeLevelGroups) {
                        registry.beginGroup(threeLevelKey);
                        const QString rootPath = registry.value(
                                    QStringLiteral("InstallPath")).toString();
                        if (!rootPath.isEmpty()) {
                            // Build full compiler path.
                            const QFileInfo iarPath(rootPath + entry.subExePath);
                            if (iarPath.exists()) {
                                // Note: threeLevelKey is a guessed toolchain version.
                                const QString version = threeLevelKey;
                                infos.push_back({iarPath, Version::fromString(version)});
                            }
                        }
                        registry.endGroup();
                    }
                    registry.endGroup();
                }
            }
            registry.endGroup();
        }

    }

    return infos;
}

bool isIarCompiler(const QString &compilerName)
{
    return Internal::any_of(knownIarCompilerNames(), [compilerName](
                            const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createIarProfile(const QFileInfo &compiler, Settings *settings,
                      QString profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createIarProfileHelper(info, settings, profileName);
}

void iarProbe(Settings *settings, QList<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect IAR toolchains...");

    std::vector<ToolchainInstallInfo> allInfos = installedIarsFromRegistry();
    const std::vector<ToolchainInstallInfo> pathInfos = installedIarsFromPath();
    allInfos.insert(std::end(allInfos), std::begin(pathInfos), std::end(pathInfos));

    for (const ToolchainInstallInfo &info : allInfos) {
        const auto profile = createIarProfileHelper(info, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No IAR toolchains found.");
}
