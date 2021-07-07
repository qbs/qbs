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

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownDmcCompilerNames()
{
    return {QStringLiteral("dmc")};
}

static QString guessDmcArchitecture(const QFileInfo &compiler)
{
    const QStringList keys = {QStringLiteral("__I86__")};
    const auto macros = dumpMacros([&compiler, &keys]() {
        const QString filePath = QDir(QDir::tempPath())
                                     .absoluteFilePath(QLatin1String("dmc-dump.c"));
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
        p.start(compiler.absoluteFilePath(), {QStringLiteral("-e"), filePath});
        p.waitForFinished(3000);
        fakeIn.remove();
        const QStringList lines = QString::fromUtf8(p.readAllStandardOutput())
                                      .split(QRegularExpression(QLatin1String("\\r?\\n")));
        return lines;
    });

    for (const QString &key : keys) {
        if (macros.contains(key)) {
            bool ok = false;
            const auto value = macros.value(key).toInt(&ok);
            if (ok) {
                switch (value) {
                case 0: // 8088
                case 2: // 286
                case 3: // 386
                case 4: // 486
                    break;
                case 5: // P5
                case 6: // P6
                    return QLatin1String("x86");
                default:
                    break;
                }
            }
        }
    }

    return QLatin1String("unknown");
}

static Profile createDmcProfileHelper(const ToolchainInstallInfo &info,
                                      Settings *settings,
                                      QString profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;
    const QString architecture = guessDmcArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty()) {
        if (!info.compilerVersion.isValid()) {
            profileName = QStringLiteral("dmc-unknown-%1").arg(architecture);
        } else {
            const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                  QLatin1Char('_'));
            profileName = QStringLiteral("dmc-%1-%2").arg(
                version, architecture);
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QLatin1String("qbs.toolchainType"), QLatin1String("dmc"));
    if (!architecture.isEmpty())
        profile.setValue(QLatin1String("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
        profile.name(), compiler.absoluteFilePath());
    return profile;
}

static Version dumpDmcCompilerVersion(const QFileInfo &compiler)
{
    QProcess p;
    QStringList args;
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);
    const auto es = p.exitStatus();
    if (es != QProcess::NormalExit) {
        const QByteArray out = p.readAll();
        qbsWarning() << Tr::tr("Compiler dumping failed:\n%1")
                            .arg(QString::fromUtf8(out));
        return Version{};
    }

    const QByteArray output = p.readAllStandardOutput();
    const QRegularExpression re(QLatin1String(
        "^Digital Mars Compiler Version (\\d+)\\.?(\\d+)\\.?(\\*|\\d+)?"));
    const QRegularExpressionMatch match = re.match(QString::fromLatin1(output));
    if (!match.hasMatch())
        return Version{};

    const auto major = match.captured(1).toInt();
    const auto minor = match.captured(2).toInt();
    const auto patch = match.captured(3).toInt();
    return Version{major, minor, patch};
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
        const Version version = dumpDmcCompilerVersion(dmcPath);
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
                      QString profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createDmcProfileHelper(info, settings, std::move(profileName));
}

void dmcProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect DMC toolchains...");

    const std::vector<ToolchainInstallInfo> allInfos = installedDmcsFromPath();
    for (const ToolchainInstallInfo &info : allInfos) {
        const auto profile = createDmcProfileHelper(info, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No DMC toolchains found.");
}
