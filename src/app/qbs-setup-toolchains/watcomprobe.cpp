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

static QStringList knownWatcomCompilerNames()
{
    return {QStringLiteral("owcc")};
}

static QStringList dumpOutput(const QFileInfo &compiler, const QStringList &keys)
{
    const QString filePath = QDir(QDir::tempPath()).absoluteFilePath(
        QLatin1String("watcom-dump.c"));
    QFile fakeIn(filePath);
    if (!fakeIn.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text)) {
        qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                            .arg(fakeIn.fileName(), fakeIn.errorString());
        return QStringList{};
    }
    fakeIn.write("#define VALUE_TO_STRING(x) #x\n");
    fakeIn.write("#define VALUE(x) VALUE_TO_STRING(x)\n");
    fakeIn.write("#define VAR_NAME_VALUE(var) \"#define \" #var\" \"VALUE(var)\n");
    for (const QString &key : keys) {
        fakeIn.write("#if defined(" + key.toLatin1() + ")\n");
        fakeIn.write("#pragma message (VAR_NAME_VALUE(" + key.toLatin1() + "))\n");
        fakeIn.write("#endif\n");
    }
    fakeIn.close();
    QProcess p;
    p.start(compiler.absoluteFilePath(), {QDir::toNativeSeparators(filePath)});
    p.waitForFinished(3000);
    fakeIn.remove();
    const QStringList lines = QString::fromUtf8(p.readAllStandardOutput())
                                  .split(QRegularExpression(QLatin1String("\\r?\\n")));
    return lines;
}

static QString guessWatcomArchitecture(const QFileInfo &compiler)
{
    const QStringList keys = {QStringLiteral("__I86__"), QStringLiteral("__386__")};
    const auto macros = dumpMacros([&compiler, &keys]() { return dumpOutput(compiler, keys); });
    for (auto index = 0; index < keys.count(); ++index) {
        const auto &key = keys.at(index);
        if (macros.contains(key) && macros.value(key) == QLatin1String("1")) {
            switch (index) {
            case 0:
                return QLatin1String("x86_16");
            case 1:
                return QLatin1String("x86");
            default:
                break;
            }
        }
    }
    return QLatin1String("unknown");
}

static Profile createWatcomProfileHelper(const ToolchainInstallInfo &info,
                                         Settings *settings,
                                         QString profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;
    const QString architecture = guessWatcomArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty()) {
        if (!info.compilerVersion.isValid()) {
            profileName = QStringLiteral("watcom-unknown-%1").arg(architecture);
        } else {
            const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                  QLatin1Char('_'));
            profileName = QStringLiteral("watcom-%1-%2").arg(version, architecture);
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("watcom"));
    if (!architecture.isEmpty())
        profile.setValue(QStringLiteral("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                     .arg(profile.name(), compiler.absoluteFilePath());
    return profile;
}

static Version dumpWatcomVersion(const QFileInfo &compiler)
{
    const QStringList keys = {QStringLiteral("__WATCOMC__"),
                              QStringLiteral("__WATCOM_CPLUSPLUS__")};
    const auto macros = dumpMacros([&compiler, &keys]() { return dumpOutput(compiler, keys); });
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

void createWatcomProfile(const QFileInfo &compiler, Settings *settings, QString profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createWatcomProfileHelper(info, settings, std::move(profileName));
}

void watcomProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect WATCOM toolchains...");

    const std::vector<ToolchainInstallInfo> allInfos = installedWatcomsFromPath();
    if (allInfos.empty()) {
        qbsInfo() << Tr::tr("No WATCOM toolchains found.");
        return;
    }

    qbs::Internal::transform(allInfos, profiles, [settings](const auto &info) {
        return createWatcomProfileHelper(info, settings); });
}
