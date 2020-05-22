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

#include <QtCore/qprocess.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownIarCompilerNames()
{
    return {QStringLiteral("icc8051"), QStringLiteral("iccarm"),
            QStringLiteral("iccavr"), QStringLiteral("iccstm8"),
            QStringLiteral("icc430"), QStringLiteral("iccrl78"),
            QStringLiteral("iccrx"), QStringLiteral("iccrh850"),
            QStringLiteral("iccv850"), QStringLiteral("icc78k"),
            QStringLiteral("iccavr32"), QStringLiteral("iccsh"),
            QStringLiteral("iccriscv"), QStringLiteral("icccf"),
            QStringLiteral("iccm32c"), QStringLiteral("iccr32c"),
            QStringLiteral("iccm16c"), QStringLiteral("icccr16c")};
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
    if (baseName == QLatin1String("iccstm8"))
        return QStringLiteral("stm8");
    if (baseName == QLatin1String("icc430"))
        return QStringLiteral("msp430");
    if (baseName == QLatin1String("iccrl78"))
        return QStringLiteral("rl78");
    if (baseName == QLatin1String("iccrx"))
        return QStringLiteral("rx");
    if (baseName == QLatin1String("iccrh850"))
        return QStringLiteral("rh850");
    if (baseName == QLatin1String("iccv850"))
        return QStringLiteral("v850");
    if (baseName == QLatin1String("icc78k"))
        return QStringLiteral("78k");
    if (baseName == QLatin1String("iccavr32"))
        return QStringLiteral("avr32");
    if (baseName == QLatin1String("iccsh"))
        return QStringLiteral("sh");
    if (baseName == QLatin1String("iccriscv"))
        return QStringLiteral("riscv");
    if (baseName == QLatin1String("icccf"))
        return QStringLiteral("m68k");
    if (baseName == QLatin1String("iccm32c"))
        return QStringLiteral("m32c");
    if (baseName == QLatin1String("iccr32c"))
        return QStringLiteral("r32c");
    if (baseName == QLatin1String("iccm16c"))
        return QStringLiteral("m16c");
    if (baseName == QLatin1String("icccr16c"))
        return QStringLiteral("cr16");
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

static Version dumpIarCompilerVersion(const QFileInfo &compiler)
{
    const QString outFilePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + QLatin1String("/macros.dump");
    const QStringList args = {QStringLiteral("."),
                              QStringLiteral("--predef_macros"),
                              outFilePath};
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

    QByteArray dump;
    QFile out(outFilePath);
    if (out.open(QIODevice::ReadOnly))
        dump = out.readAll();
    out.remove();

    const int verCode = extractVersion(dump, "__VER__ ");
    if (verCode < 0) {
        qbsWarning() << Tr::tr("No '__VER__' token was found in a compiler dump:\n%1")
                        .arg(QString::fromUtf8(dump));
        return Version{};
    }

    const QString arch = guessIarArchitecture(compiler);
    if (arch == QLatin1String("arm")) {
        return Version{verCode / 1000000, (verCode / 1000) % 1000, verCode % 1000};
    } else if (arch == QLatin1String("avr")
               || arch == QLatin1String("mcs51")
               || arch == QLatin1String("stm8")
               || arch == QLatin1String("msp430")
               || arch == QLatin1String("rl78")
               || arch == QLatin1String("rx")
               || arch == QLatin1String("rh850")
               || arch == QLatin1String("v850")
               || arch == QLatin1String("78k")
               || arch == QLatin1String("avr32")
               || arch == QLatin1String("sh")
               || arch == QLatin1String("riscv")
               || arch == QLatin1String("m68k")
               || arch == QLatin1String("m32c")
               || arch == QLatin1String("r32c")
               || arch == QLatin1String("m16c")
               || arch == QLatin1String("rc16")) {
        return Version{verCode / 100, verCode % 100};
    }

    return Version{};
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
        const Version version = dumpIarCompilerVersion(iarPath);
        infos.push_back({iarPath, version});
    }
    std::sort(infos.begin(), infos.end());
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
            {QStringLiteral("EWARM"), QStringLiteral("/arm/bin/iccarm.exe")},
            {QStringLiteral("EWAVR"), QStringLiteral("/avr/bin/iccavr.exe")},
            {QStringLiteral("EW8051"), QStringLiteral("/8051/bin/icc8051.exe")},
            {QStringLiteral("EWSTM8"), QStringLiteral("/stm8/bin/iccstm8.exe")},
            {QStringLiteral("EW430"), QStringLiteral("/430/bin/icc430.exe")},
            {QStringLiteral("EWRL78"), QStringLiteral("/rl78/bin/iccrl78.exe")},
            {QStringLiteral("EWRX"), QStringLiteral("/rx/bin/iccrx.exe")},
            {QStringLiteral("EWRH850"), QStringLiteral("/rh850/bin/iccrh850.exe")},
            {QStringLiteral("EWV850"), QStringLiteral("/v850/bin/iccv850.exe")},
            {QStringLiteral("EW78K"), QStringLiteral("/78k/bin/icc78k.exe")},
            {QStringLiteral("EWAVR32"), QStringLiteral("/avr32/bin/iccavr32.exe")},
            {QStringLiteral("EWSH"), QStringLiteral("/sh/bin/iccsh.exe")},
            {QStringLiteral("EWRISCV"), QStringLiteral("/riscv/bin/iccriscv.exe")},
            {QStringLiteral("EWCF"), QStringLiteral("/cf/bin/icccf.exe")},
            {QStringLiteral("EWM32C"), QStringLiteral("/m32c/bin/iccm32c.exe")},
            {QStringLiteral("EWR32C"), QStringLiteral("/r32c/bin/iccr32c.exe")},
            {QStringLiteral("EWM16C"), QStringLiteral("/m16c/bin/iccm16c.exe")},
            {QStringLiteral("EWCR16C"), QStringLiteral("/cr16c/bin/icccr16c.exe")},
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
                                infos.push_back({iarPath, Version::fromString(threeLevelKey)});
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

    std::sort(infos.begin(), infos.end());
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
    createIarProfileHelper(info, settings, std::move(profileName));
}

void iarProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect IAR toolchains...");

    // Make sure that a returned infos are sorted before using the std::set_union!
    const std::vector<ToolchainInstallInfo> regInfos = installedIarsFromRegistry();
    const std::vector<ToolchainInstallInfo> pathInfos = installedIarsFromPath();
    std::vector<ToolchainInstallInfo> allInfos;
    allInfos.reserve(regInfos.size() + pathInfos.size());
    std::set_union(regInfos.cbegin(), regInfos.cend(),
                   pathInfos.cbegin(), pathInfos.cend(),
                   std::back_inserter(allInfos));

    for (const ToolchainInstallInfo &info : allInfos) {
        const auto profile = createIarProfileHelper(info, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No IAR toolchains found.");
}
