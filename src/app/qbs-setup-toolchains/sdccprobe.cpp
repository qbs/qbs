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

static QString guessSdccArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("sdcc"))
        return QStringLiteral("mcs51");
    return {};
}

static Profile createSdccProfileHelper(const ToolchainInstallInfo &info,
                                       Settings *settings,
                                       QString profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;
    const QString architecture = guessSdccArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty()) {
        if (!info.compilerVersion.isValid()) {
            profileName = QStringLiteral("sdcc-unknown-%1").arg(architecture);
        } else {
            const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                  QLatin1Char('_'));
            profileName = QStringLiteral("sdcc-%1-%2").arg(
                        version, architecture);
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("sdcc"));
    if (!architecture.isEmpty())
        profile.setValue(QStringLiteral("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                     profile.name(), compiler.absoluteFilePath());
    return profile;
}


static Version dumpSdccCompilerVersion(const QFileInfo &compiler)
{
    QTemporaryFile fakeIn(QStringLiteral("XXXXXX.c"));
    if (!fakeIn.open()) {
        qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                        .arg(fakeIn.fileName(), fakeIn.errorString());
        return Version{};
    }
    fakeIn.close();

    const QStringList args = {QStringLiteral("-dM"),
                              QStringLiteral("-E"),
                              fakeIn.fileName()};
    QProcess p;
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);
    const auto es = p.exitStatus();
    if (es != QProcess::NormalExit) {
        const QByteArray out = p.readAll();
        qbsWarning() << Tr::tr("Compiler dumping failed:\n%1")
                        .arg(QString::fromUtf8(out));
        return Version{};
    }

    const QByteArray dump = p.readAllStandardOutput();
    const int major = extractVersion(dump, "__SDCC_VERSION_MAJOR ");
    const int minor = extractVersion(dump, "__SDCC_VERSION_MINOR ");
    const int patch = extractVersion(dump, "__SDCC_VERSION_PATCH ");
    if (major < 0 || minor < 0 || patch < 0) {
        qbsWarning() << Tr::tr("No '__SDCC_VERSION_xxx' token was found "
                               "in the compiler dump:\n%1")
                        .arg(QString::fromUtf8(dump));
        return Version{};
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

#ifdef Q_OS_WIN64
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\SDCC";
#else
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\SDCC";
#endif

        QSettings registry(QLatin1String(kRegistryNode), QSettings::NativeFormat);
        QString rootPath = registry.value(QStringLiteral("Default")).toString();
        if (!rootPath.isEmpty()) {
            // Build full compiler path.
            const QFileInfo sdccPath(rootPath + QLatin1String("\\bin\\sdcc.exe"));
            if (sdccPath.exists()) {
                // Build compiler version.
                const QString version = QStringLiteral("%1.%2.%3").arg(
                            registry.value(QStringLiteral("VersionMajor")).toString(),
                            registry.value(QStringLiteral("VersionMinor")).toString(),
                            registry.value(QStringLiteral("VersionRevision")).toString());
                infos.push_back({sdccPath, Version::fromString(version)});
            }
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
                       QString profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createSdccProfileHelper(info, settings, profileName);
}

void sdccProbe(Settings *settings, QList<Profile> &profiles)
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
        const auto profile = createSdccProfileHelper(info, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No SDCC toolchains found.");
}
