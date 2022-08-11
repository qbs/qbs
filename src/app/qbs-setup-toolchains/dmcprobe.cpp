/****************************************************************************
**
** Copyright (C) 2021 Denis Shienkov <denis.shienkov@gmail.com>
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
#include "dmcprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>

#include <optional>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

namespace {
struct Target {
    QString platform;
    QString architecture;
    QString extender;
};
}

static QStringList knownDmcCompilerNames()
{
    return {QStringLiteral("dmc")};
}

static QStringList dumpOutput(const QFileInfo &compiler, const QStringList &flags,
                              const QStringList &keys)
{
    const QString filePath = QDir(QDir::tempPath()).absoluteFilePath(
        QLatin1String("dmc-dump.c"));
    QFile fakeIn(filePath);
    if (!fakeIn.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text)) {
        qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                        .arg(fakeIn.fileName(), fakeIn.errorString());
        return {};
    }
    fakeIn.write("#define VALUE_TO_STRING(x) #x\n");
    fakeIn.write("#define VALUE(x) VALUE_TO_STRING(x)\n");
    fakeIn.write("#define VAR_NAME_VALUE(var) \"#define \" #var\" \"VALUE(var)\n");
    for (const auto &key : keys) {
        const auto content = key.toLatin1();
        fakeIn.write("#if defined(" + content + ")\n");
        fakeIn.write("#pragma message (VAR_NAME_VALUE(" + content + "))\n");
        fakeIn.write("#endif\n");
    }
    fakeIn.close();

    QStringList args = {QStringLiteral("-e")};
    args.reserve(args.size() + int(flags.size()));
    std::copy(flags.cbegin(), flags.cend(), std::back_inserter(args));
    args.push_back(QDir::toNativeSeparators(filePath));

    QProcess p;
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);
    fakeIn.remove();
    static QRegularExpression re(QLatin1String("\\r?\\n"));
    return QString::fromUtf8(p.readAllStandardOutput()).split(re);
}

static std::optional<Target> dumpDmcTarget(const QFileInfo &compiler, const QStringList &flags)
{
    const QStringList keys = {
        QStringLiteral("DOS16RM"),
        QStringLiteral("DOS386"),
        QStringLiteral("MSDOS"),
        QStringLiteral("__NT__"),
        QStringLiteral("_WINDOWS"),
    };
    const auto macros = dumpMacros([&compiler, &flags, &keys]() {
        return dumpOutput(compiler, flags, keys); });

    if (macros.contains(QLatin1String("__NT__"))) {
        return Target{QLatin1String("windows"), QLatin1String("x86"), QLatin1String("")};
    } else if (macros.contains(QLatin1String("_WINDOWS"))) {
        return Target{QLatin1String("windows"), QLatin1String("x86_16"), QLatin1String("")};
    } else if (macros.contains(QLatin1String("DOS386"))) {
        if (flags.contains(QLatin1String("-mx")))
            return Target{QLatin1String("dos"), QLatin1String("x86"), QLatin1String("dosx")};
        else if (flags.contains(QLatin1String("-mp")))
            return Target{QLatin1String("dos"), QLatin1String("x86"), QLatin1String("dosp")};
    } else if (macros.contains(QLatin1String("DOS16RM"))) {
        if (flags.contains(QLatin1String("-mz")))
            return Target{QLatin1String("dos"), QLatin1String("x86_16"), QLatin1String("dosz")};
        else if (flags.contains(QLatin1String("-mr")))
            return Target{QLatin1String("dos"), QLatin1String("x86_16"), QLatin1String("dosr")};
    } else if (macros.contains(QLatin1String("MSDOS"))) {
        return Target{QLatin1String("dos"), QLatin1String("x86_16"), QLatin1String("")};
    }

    return {};
}

static std::vector<Profile> createDmcProfileHelper(const ToolchainInstallInfo &info,
                                                   Settings *settings,
                                                   QStringView profileName = {})
{
    const QFileInfo compiler = info.compilerPath;
    std::vector<Profile> profiles;

    const QVector<QStringList> probes = {
        { QStringLiteral("-mn"), QStringLiteral("-WA") }, // Windows NT 32 bit.
        { QStringLiteral("-ml"),  QStringLiteral("-WA") }, // Windows 3.x 16 bit.
        { QStringLiteral("-mx") }, // DOS with DOSX extender 32 bit.
        { QStringLiteral("-mp") }, // DOS with Phar Lap extender 32 bit.
        { QStringLiteral("-mr") }, // DOS with Rational DOS Extender 16 bit.
        { QStringLiteral("-mz") }, // DOS with  ZPM DOS Extender 16 bit.
        { QStringLiteral("-mc") }, // DOS 16 bit.
    };

   for (const auto &flags : probes) {
       const auto target = dumpDmcTarget(compiler, flags);
        if (!target)
            continue;

        QString fullProfilename;
        QString platform = target->extender.isEmpty() ? target->platform : target->extender;
        if (profileName.isEmpty()) {
            // Create a full profile name in case we is in auto-detecting mode.
            if (!info.compilerVersion.isValid()) {
                fullProfilename = QStringLiteral("dmc-unknown-%1-%2")
                        .arg(platform, target->architecture);
            } else {
                const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                      QLatin1Char('_'));
                fullProfilename = QStringLiteral("dmc-%1-%2-%3")
                        .arg(version, platform, target->architecture);
            }
        } else {
            // Append the detected actual architecture name to the proposed profile name.
            fullProfilename = QStringLiteral("%1-%2-%3")
                    .arg(profileName, platform, target->architecture);
        }

        Profile profile(fullProfilename, settings);
        profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
        profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("dmc"));
        profile.setValue(QStringLiteral("qbs.architecture"), target->architecture);
        profile.setValue(QStringLiteral("qbs.targetPlatform"), target->platform);
        if (!target->extender.isEmpty())
            profile.setValue(QStringLiteral("cpp.extenderName"), target->extender);

        qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                     .arg(profile.name(), compiler.absoluteFilePath());

        profiles.push_back(std::move(profile));
   }

    return profiles;
}

static Version dumpDmcVersion(const QFileInfo &compiler)
{
    const QStringList keys = {QStringLiteral("__DMC__")};
    const auto macros = dumpMacros([&compiler, keys]() {
        return dumpOutput(compiler, {}, keys); });
    for (const auto &macro : macros) {
        if (!macro.startsWith(QLatin1String("0x")))
            continue;
        const int verCode = macro.mid(2).toInt();
        return Version{(verCode / 100), (verCode % 100), 0};
    }
    qbsWarning() << Tr::tr("No __DMC__ token was found in the compiler dump");
    return Version{};
}

static std::vector<ToolchainInstallInfo> installedDmcsFromPath()
{
    std::vector<ToolchainInstallInfo> infos;
    const auto compilerNames = knownDmcCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo dmcPath(
            findExecutable(
                HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!dmcPath.exists())
            continue;
        const Version version = dumpDmcVersion(dmcPath);
        infos.push_back({dmcPath, version});
    }
    std::sort(infos.begin(), infos.end());
    return infos;
}

bool isDmcCompiler(const QString &compilerName)
{
    return Internal::any_of(knownDmcCompilerNames(),
                            [compilerName](const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createDmcProfile(const QFileInfo &compiler, Settings *settings,
                      QStringView profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createDmcProfileHelper(info, settings, profileName);
}

void dmcProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect DMC toolchains...");

    const std::vector<ToolchainInstallInfo> allInfos = installedDmcsFromPath();
    if (allInfos.empty()) {
        qbsInfo() << Tr::tr("No DMC toolchains found.");
        return;
    }

    for (const ToolchainInstallInfo &info : allInfos) {
        const auto newProfiles = createDmcProfileHelper(info, settings);
        profiles.reserve(profiles.size() + int(newProfiles.size()));
        std::copy(newProfiles.cbegin(), newProfiles.cend(), std::back_inserter(profiles));
    }
}
