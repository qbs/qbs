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
#include "sdccprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlist.h>
#include <QtCore/qsettings.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownSdccCompilerNames()
{
    return {QStringLiteral("sdcc")};
}

static QString guessSdccArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("sdcc"))
        return QStringLiteral("mcs51");
    return {};
}

static Profile createSdccProfileHelper(const QFileInfo &compiler, Settings *settings,
                                       QString profileName = QString())
{
    const QString architecture = guessSdccArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty())
        profileName = QLatin1String("sdcc-") + architecture;

    Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("sdcc"));
    if (!architecture.isEmpty())
        profile.setValue(QStringLiteral("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                     profile.name(), compiler.absoluteFilePath());
    return profile;
}

static std::vector<SdccInstallInfo> installedSdccsFromPath()
{
    std::vector<SdccInstallInfo> infos;
    const auto compilerNames = knownSdccCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo sdccPath(
                    findExecutable(
                        HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!sdccPath.exists())
            continue;
        infos.push_back({sdccPath.absoluteFilePath(), {}});
    }
    return infos;
}

static std::vector<SdccInstallInfo> installedSdccsFromRegistry()
{
    std::vector<SdccInstallInfo> infos;

    if (HostOsInfo::isWindowsHost()) {
        // Tries to detect the candidate from the 32-bit
        // or 64-bit system registry format.
        auto probeSdccToolchainInfo = [](QSettings::Format format) {
            SdccInstallInfo info;
            QSettings registry(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\SDCC"),
                               format);
            const QString rootPath = registry.value(QStringLiteral("Default"))
                    .toString();
            if (rootPath.isEmpty())
                return info;
            // Build full compiler path.
            const QFileInfo sdccPath(rootPath + QLatin1String("\\bin\\sdcc.exe"));
            if (!sdccPath.exists())
                return info;
            info.compilerPath = sdccPath.filePath();
            // Build compiler version.
            const QString version = QStringLiteral("%1.%2.%3").arg(
                        registry.value(QStringLiteral("VersionMajor")).toString(),
                        registry.value(QStringLiteral("VersionMinor")).toString(),
                        registry.value(QStringLiteral("VersionRevision")).toString());
            info.version = version;
            return info;
        };

        static constexpr QSettings::Format allowedFormats[] = {
            QSettings::NativeFormat,
#ifdef Q_OS_WIN
            QSettings::Registry32Format,
            QSettings::Registry64Format,
#endif
        };

        for (const QSettings::Format format : allowedFormats) {
            const SdccInstallInfo candidate = probeSdccToolchainInfo(format);
            if (candidate.compilerPath.isEmpty())
                continue;
            const auto infosEnd = infos.cend();
            const auto infosIt = std::find_if(infos.cbegin(), infosEnd,
                                              [candidate](const SdccInstallInfo &info) {
                return candidate == info;
            });
            if (infosIt == infosEnd)
                infos.push_back(candidate);
        }
    }

    return infos;
}

bool isSdccCompiler(const QString &compilerName)
{
    return Internal::any_of(knownSdccCompilerNames(), [compilerName](
                            const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createSdccProfile(const QFileInfo &compiler, Settings *settings,
                       QString profileName)
{
    createSdccProfileHelper(compiler, settings, profileName);
}

void sdccProbe(Settings *settings, QList<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect SDCC toolchains...");

    std::vector<SdccInstallInfo> allInfos = installedSdccsFromRegistry();
    const std::vector<SdccInstallInfo> pathInfos = installedSdccsFromPath();
    allInfos.insert(std::end(allInfos), std::begin(pathInfos), std::end(pathInfos));

    for (const SdccInstallInfo &info : allInfos) {
        const auto profile = createSdccProfileHelper(info.compilerPath, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No SDCC toolchains found.");
}
