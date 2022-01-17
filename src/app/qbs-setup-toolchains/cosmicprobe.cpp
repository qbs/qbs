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
#include "cosmicprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qprocess.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownCosmicCompilerNames()
{
    return {QStringLiteral("cxcorm"), QStringLiteral("cxstm8"),
            QStringLiteral("cx6808"), QStringLiteral("cx6812"),
            QStringLiteral("cx332")};
}

static QString guessCosmicArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("cxcorm"))
        return QStringLiteral("arm");
    if (baseName == QLatin1String("cxstm8"))
        return QStringLiteral("stm8");
    if (baseName == QLatin1String("cx6808"))
        return QStringLiteral("hcs8");
    if (baseName == QLatin1String("cx6812"))
        return QStringLiteral("hcs12");
    if (baseName == QLatin1String("cx332"))
        return QStringLiteral("m68k");
    return {};
}

static Profile createCosmicProfileHelper(const ToolchainInstallInfo &info,
                                         Settings *settings,
                                         QString profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;
    const QString architecture = guessCosmicArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty()) {
        if (!info.compilerVersion.isValid()) {
            profileName = QStringLiteral("cosmic-unknown-%1").arg(architecture);
        } else {
            const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                  QLatin1Char('_'));
            profileName = QStringLiteral("cosmic-%1-%2").arg(
                version, architecture);
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QLatin1String("qbs.toolchainType"), QLatin1String("cosmic"));
    if (!architecture.isEmpty())
        profile.setValue(QLatin1String("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
        profile.name(), compiler.absoluteFilePath());
    return profile;
}

static Version dumpCosmicCompilerVersion(const QFileInfo &compiler)
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

    const QByteArray output = p.readAllStandardError();
    const QRegularExpression re(QLatin1String("^COSMIC.+V(\\d+)\\.?(\\d+)\\.?(\\*|\\d+)?"));
    const QRegularExpressionMatch match = re.match(QString::fromLatin1(output));
    if (!match.hasMatch())
        return Version{};

    const auto major = match.captured(1).toInt();
    const auto minor = match.captured(2).toInt();
    const auto patch = match.captured(3).toInt();
    return Version{major, minor, patch};
}

static std::vector<ToolchainInstallInfo> installedCosmicsFromPath()
{
    std::vector<ToolchainInstallInfo> infos;
    const auto compilerNames = knownCosmicCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo cosmicPath(
            findExecutable(
                HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!cosmicPath.exists())
            continue;
        const Version version = dumpCosmicCompilerVersion(cosmicPath);
        infos.push_back({cosmicPath, version});
    }
    std::sort(infos.begin(), infos.end());
    return infos;
}

bool isCosmicCompiler(const QString &compilerName)
{
    return Internal::any_of(knownCosmicCompilerNames(), [compilerName](const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createCosmicProfile(const QFileInfo &compiler, Settings *settings,
                         QString profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createCosmicProfileHelper(info, settings, std::move(profileName));
}

void cosmicProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect COSMIC toolchains...");

    const std::vector<ToolchainInstallInfo> allInfos = installedCosmicsFromPath();
    qbs::Internal::transform(allInfos, profiles, [settings](const auto &info) {
        return createCosmicProfileHelper(info, settings); });

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No COSMIC toolchains found.");
}
