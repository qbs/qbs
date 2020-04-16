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

#include <QtCore/qprocess.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtemporaryfile.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownSdccCompilerNames()
{
    return {QStringLiteral("sdcc")};
}

static QByteArray dumpSdccMacros(const QFileInfo &compiler,
                                 const QString &targetFlag = QString())
{
    QTemporaryFile fakeIn(QStringLiteral("XXXXXX.c"));
    if (!fakeIn.open()) {
        qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                        .arg(fakeIn.fileName(), fakeIn.errorString());
        return {};
    }
    fakeIn.close();

    const QStringList args = {QStringLiteral("-dM"),
                              QStringLiteral("-E"),
                              targetFlag,
                              fakeIn.fileName()};
    QProcess p;
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);
    const auto es = p.exitStatus();
    if (es != QProcess::NormalExit) {
        const QByteArray out = p.readAll();
        qbsWarning() << Tr::tr("Compiler dumping failed:\n%1")
                        .arg(QString::fromUtf8(out));
        return {};
    }

    return  p.readAllStandardOutput();
}

static QString dumpSdccArchitecture(const QFileInfo &compiler,
                                    const QString &arch)
{
    const auto targetFlag = QStringLiteral("-m%1").arg(arch);
    const auto token = QStringLiteral("__SDCC_%1").arg(arch);
    const auto macros = dumpSdccMacros(compiler, targetFlag);
    return macros.contains(token.toLatin1()) ? arch : QString();
}

static std::vector<Profile> createSdccProfileHelper(
        const ToolchainInstallInfo &info,
        Settings *settings,
        const QString &profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;

    std::vector<Profile> profiles;

    const char *knownArchs[] = {"mcs51", "stm8"};
    for (const auto &knownArch : knownArchs) {
        const auto actualArch = dumpSdccArchitecture(
                    compiler, QString::fromLatin1(knownArch));
        // Don't create a profile in case the compiler does
        // not support the proposed architecture.
        if (actualArch != QString::fromLatin1(knownArch))
            continue;

        QString fullProfileName;
        if (profileName.isEmpty()) {
            // Create a full profile name in case we is
            // in auto-detecting mode.
            if (!info.compilerVersion.isValid()) {
                fullProfileName = QStringLiteral("sdcc-unknown-%1")
                        .arg(actualArch);
            } else {
                const QString version = info.compilerVersion.toString(
                            QLatin1Char('_'), QLatin1Char('_'));
                fullProfileName = QStringLiteral("sdcc-%1-%2").arg(
                            version, actualArch);
            }
        } else {
            // Append the detected actual architecture name
            // to the proposed profile name.
            fullProfileName = QStringLiteral("%1-%2").arg(
                        profileName, actualArch);
        }

        Profile profile(fullProfileName, settings);
        profile.setValue(QStringLiteral("cpp.toolchainInstallPath"),
                         compiler.absolutePath());
        profile.setValue(QStringLiteral("qbs.toolchainType"),
                         QStringLiteral("sdcc"));
        if (!actualArch.isEmpty())
            profile.setValue(QStringLiteral("qbs.architecture"), actualArch);

        qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                         profile.name(), compiler.absoluteFilePath());
    }

    return profiles;
}

static Version dumpOldSddcCompilerVersion(const QByteArray &macroDump)
{
    const auto keyToken = QByteArrayLiteral("__SDCC ");
    const int startIndex = macroDump.indexOf(keyToken);
    if (startIndex == -1)
        return Version{};
    const int endIndex = macroDump.indexOf('\n', startIndex);
    if (endIndex == -1)
        return Version{};
    const auto keyLength = keyToken.length();
    return Version::fromString(QString::fromLatin1(
            macroDump.mid(startIndex + keyLength,
                          endIndex - startIndex - keyLength).replace('_', '.')));
}

static Version dumpSdccCompilerVersion(const QFileInfo &compiler)
{
    const QByteArray dump = dumpSdccMacros(compiler);
    if (dump.isEmpty())
        return Version{};

    const int major = extractVersion(dump, "__SDCC_VERSION_MAJOR ");
    const int minor = extractVersion(dump, "__SDCC_VERSION_MINOR ");
    const int patch = extractVersion(dump, "__SDCC_VERSION_PATCH ");
    if (major < 0 || minor < 0 || patch < 0) {
        const auto version = dumpOldSddcCompilerVersion(dump);
        if (!version.isValid()) {
            qbsWarning() << Tr::tr("No '__SDCC_VERSION_xxx' or '__SDCC' token was found "
                                   "in the compiler dump:\n%1")
                            .arg(QString::fromUtf8(dump));
            return Version{};
        }
        return version;
    }

    return Version{major, minor, patch};
}

static std::vector<ToolchainInstallInfo> installedSdccsFromPath()
{
    std::vector<ToolchainInstallInfo> infos;
    const auto compilerNames = knownSdccCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo sdccPath(
                    findExecutable(
                        HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!sdccPath.exists())
            continue;
        const Version version = dumpSdccCompilerVersion(sdccPath);
        infos.push_back({sdccPath, version});
    }
    std::sort(infos.begin(), infos.end());
    return infos;
}

static std::vector<ToolchainInstallInfo> installedSdccsFromRegistry()
{
    std::vector<ToolchainInstallInfo> infos;

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
                                              [candidate](const ToolchainInstallInfo &info) {
                return candidate == SdccInstallInfo{
                    info.compilerPath.filePath(), info.compilerVersion.toString()};
            });
            if (infosIt == infosEnd)
                infos.push_back({candidate.compilerPath, Version::fromString(candidate.version)});
        }
    }

    std::sort(infos.begin(), infos.end());
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
                       const QString &profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createSdccProfileHelper(info, settings, profileName);
}

void sdccProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect SDCC toolchains...");

    // Make sure that a returned infos are sorted before using the std::set_union!
    const std::vector<ToolchainInstallInfo> regInfos = installedSdccsFromRegistry();
    const std::vector<ToolchainInstallInfo> pathInfos = installedSdccsFromPath();
    std::vector<ToolchainInstallInfo> allInfos;
    allInfos.reserve(regInfos.size() + pathInfos.size());
    std::set_union(regInfos.cbegin(), regInfos.cend(),
                   pathInfos.cbegin(), pathInfos.cend(),
                   std::back_inserter(allInfos));

    for (const ToolchainInstallInfo &info : allInfos) {
        const auto newProfiles = createSdccProfileHelper(info, settings);
        profiles.reserve(profiles.size() + int(newProfiles.size()));
        std::copy(newProfiles.cbegin(), newProfiles.cend(),
                  std::back_inserter(profiles));
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No SDCC toolchains found.");
}
