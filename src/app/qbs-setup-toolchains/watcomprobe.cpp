/****************************************************************************
**
** Copyright (C) 2022 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "watcomprobe.h"
#include "probe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qdir.h>
#include <QtCore/qmap.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtemporaryfile.h>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

namespace {

struct Details {
    QStringView architecture;
    QStringView platform;
};

constexpr struct Platform {
    QStringView flag;
    Details keys;
    Details target;
} knownPlatforms[] = {
    // DOS 16/32 bit.
    {u"-bdos", {u"__I86__", u"__DOS__"}, {u"x86_16", u"dos"}},
    {u"-bdos4g", {u"__386__", u"__DOS__"}, {u"x86", u"dos"}},
    // Windows 16/32 bit.
    {u"-bwindows", {u"__I86__", u"__WINDOWS__"}, {u"x86_16", u"windows"}},
    {u"-bnt", {u"__386__", u"__NT__"}, {u"x86", u"windows"}},
    // OS/2 16/32 bit.
    {u"-bos2", {u"__I86__", u"__OS2__"}, {u"x86_16", u"os2"}},
    {u"-bos2v2", {u"__386__", u"__OS2__"}, {u"x86", u"os2"}},
    // Linux 32 bit.
    {u"-blinux", {u"__386__", u"__LINUX__"}, {u"x86", u"linux"}},
};

} // namespace

static QStringList knownWatcomCompilerNames()
{
    return {QStringLiteral("owcc")};
}

static QStringList dumpOutput(const QFileInfo &compiler, QStringView flag,
                              const QList<QStringView> &keys)
{
    const QString filePath = QDir(QDir::tempPath()).absoluteFilePath(
        QLatin1String("watcom-dump.c"));
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
        fakeIn.write("#if defined(" + key.toLatin1() + ")\n");
        fakeIn.write("#pragma message (VAR_NAME_VALUE(" + key.toLatin1() + "))\n");
        fakeIn.write("#endif\n");
    }
    fakeIn.close();
    QProcess p;
    QStringList args;
    if (!flag.isEmpty())
        args.push_back(flag.toString());
    args.push_back(QDir::toNativeSeparators(filePath));
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);
    fakeIn.remove();
    const QStringList lines = QString::fromUtf8(p.readAllStandardOutput())
                                  .split(QRegularExpression(QLatin1String("\\r?\\n")));
    return lines;
}

static bool supportsWatcomPlatform(const QFileInfo &compiler, const Platform &platform)
{
    const auto macros = dumpMacros([&compiler, &platform]() {
        const QList<QStringView> keys = {platform.keys.architecture, platform.keys.platform};
        return dumpOutput(compiler, platform.flag, keys); });

    auto matches = [&macros](QStringView key) {
        const auto k = key.toString();
        if (!macros.contains(k))
            return false;
        return macros.value(k) == QLatin1String("1");
    };

    return matches(platform.keys.architecture) && matches(platform.keys.platform);
}

static std::vector<Profile> createWatcomProfileHelper(const ToolchainInstallInfo &info,
                                                      Settings *settings,
                                                      QStringView profileName = {})
{
    const QFileInfo compiler = info.compilerPath;
    std::vector<Profile> profiles;

    for (const auto &knownPlatform : knownPlatforms) {
        // Don't create a profile in case the compiler does
        // not support the proposed architecture.
        if (!supportsWatcomPlatform(compiler, knownPlatform))
            continue;

        QString fullProfilename;
        if (profileName.isEmpty()) {
            // Create a full profile name in case we is in auto-detecting mode.
            if (!info.compilerVersion.isValid()) {
                fullProfilename = QStringLiteral("watcom-unknown-%1-%2")
                        .arg(knownPlatform.target.platform)
                        .arg(knownPlatform.target.architecture);
            } else {
                const QString version= info.compilerVersion.toString(QLatin1Char('_'),
                                                                     QLatin1Char('_'));
                fullProfilename = QStringLiteral("watcom-%1-%2-%3")
                        .arg(version)
                        .arg(knownPlatform.target.platform)
                        .arg(knownPlatform.target.architecture);
            }
        } else {
            // Append the detected actual architecture name to the proposed profile name.
            fullProfilename = QStringLiteral("%1-%2-%3")
                    .arg(profileName)
                    .arg(knownPlatform.target.platform)
                    .arg(knownPlatform.target.architecture);
        }

        Profile profile(fullProfilename, settings);
        profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
        profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("watcom"));
        profile.setValue(QStringLiteral("qbs.architecture"),
                         knownPlatform.target.architecture.toString());
        profile.setValue(QStringLiteral("qbs.targetPlatform"),
                         knownPlatform.target.platform.toString());

        qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                     .arg(profile.name(), compiler.absoluteFilePath());

        profiles.push_back(std::move(profile));
    }

    return profiles;
}

static Version dumpWatcomVersion(const QFileInfo &compiler)
{
    const QList<QStringView> keys = {u"__WATCOMC__", u"__WATCOM_CPLUSPLUS__"};
    const auto macros = dumpMacros([&compiler, &keys]() {
        return dumpOutput(compiler, u"", keys); });
    for (const auto &macro : macros) {
        const int verCode = macro.toInt();
        return Version{(verCode - 1100) / 100,
                       (verCode / 10) % 10,
                       ((verCode % 10) > 0) ? (verCode % 10) : 0};
    }
    qbsWarning() << Tr::tr("No __WATCOMC__ or __WATCOM_CPLUSPLUS__ tokens was found"
                           " in the compiler dump");
    return Version{};
}

static std::vector<ToolchainInstallInfo> installedWatcomsFromPath()
{
    std::vector<ToolchainInstallInfo> infos;
    const auto compilerNames = knownWatcomCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo watcomPath(findExecutable(
            HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!watcomPath.exists())
            continue;
        const Version version = dumpWatcomVersion(watcomPath);
        infos.push_back({watcomPath, version});
    }
    std::sort(infos.begin(), infos.end());
    return infos;
}

bool isWatcomCompiler(const QString &compilerName)
{
    return Internal::any_of(knownWatcomCompilerNames(), [compilerName](const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createWatcomProfile(const QFileInfo &compiler, Settings *settings, QStringView profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createWatcomProfileHelper(info, settings, profileName);
}

void watcomProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect WATCOM toolchains...");

    const std::vector<ToolchainInstallInfo> allInfos = installedWatcomsFromPath();
    if (allInfos.empty()) {
        qbsInfo() << Tr::tr("No WATCOM toolchains found.");
        return;
    }

    for (const ToolchainInstallInfo &info : allInfos) {
        const auto newProfiles = createWatcomProfileHelper(info, settings);
        profiles.reserve(profiles.size() + int(newProfiles.size()));
        std::copy(newProfiles.cbegin(), newProfiles.cend(), std::back_inserter(profiles));
    }
}
