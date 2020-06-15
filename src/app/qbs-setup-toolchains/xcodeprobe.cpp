/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#include "xcodeprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qprocess.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QtCore/qsettings.h>

#include <regex>

using namespace qbs;
using Internal::Tr;

static const QString defaultDeveloperPath =
        QStringLiteral("/Applications/Xcode.app/Contents/Developer");
static const std::regex defaultDeveloperPathRegex(
        "^/Applications/Xcode([a-zA-Z0-9 _-]+)\\.app/Contents/Developer$");

static QString targetOS(const QString &applePlatformName)
{
    if (applePlatformName == QStringLiteral("macosx"))
        return QStringLiteral("macos");
    if (applePlatformName == QStringLiteral("iphoneos"))
        return QStringLiteral("ios");
    if (applePlatformName == QStringLiteral("iphonesimulator"))
        return QStringLiteral("ios-simulator");
    if (applePlatformName == QStringLiteral("appletvos"))
        return QStringLiteral("tvos");
    if (applePlatformName == QStringLiteral("appletvsimulator"))
        return QStringLiteral("tvos-simulator");
    if (applePlatformName == QStringLiteral("watchos"))
        return QStringLiteral("watchos");
    if (applePlatformName == QStringLiteral("watchsimulator"))
        return QStringLiteral("watchos-simulator");
    return {};
}

static QStringList archList(const QString &applePlatformName)
{
    QStringList archs;
    if (applePlatformName == QStringLiteral("macosx")
            || applePlatformName == QStringLiteral("iphonesimulator")
            || applePlatformName == QStringLiteral("appletvsimulator")
            || applePlatformName == QStringLiteral("watchsimulator")) {
        if (applePlatformName != QStringLiteral("appletvsimulator"))
            archs << QStringLiteral("x86");
        if (applePlatformName != QStringLiteral("watchsimulator"))
            archs << QStringLiteral("x86_64");
    } else if (applePlatformName == QStringLiteral("iphoneos")
               || applePlatformName == QStringLiteral("appletvos")) {
        if (applePlatformName != QStringLiteral("appletvos"))
            archs << QStringLiteral("armv7a");
        archs << QStringLiteral("arm64");
    } else if (applePlatformName == QStringLiteral("watchos")) {
        archs << QStringLiteral("armv7k");
    }

    return archs;
}

namespace {
class XcodeProbe
{
public:
    XcodeProbe(qbs::Settings *settings, std::vector<qbs::Profile> &profiles)
        : settings(settings), profiles(profiles)
    { }

    bool addDeveloperPath(const QString &path);
    void detectDeveloperPaths();
    void setupDefaultToolchains(const QString &devPath, const QString &xCodeName);
    void detectAll();
private:
    qbs::Settings *settings;
    std::vector<qbs::Profile> &profiles;
    QStringList developerPaths;
};

bool XcodeProbe::addDeveloperPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    QFileInfo pInfo(path);
    if (!pInfo.exists() || !pInfo.isDir())
        return false;
    if (developerPaths.contains(path))
        return false;
    developerPaths.push_back(path);
    qbsInfo() << Tr::tr("Added developer path %1").arg(path);
    return true;
}

void XcodeProbe::detectDeveloperPaths()
{
    QProcess selectedXcode;
    QString program = QStringLiteral("/usr/bin/xcode-select");
    QStringList arguments(QStringLiteral("--print-path"));
    selectedXcode.start(program, arguments, QProcess::ReadOnly);
    if (!selectedXcode.waitForFinished(-1) || selectedXcode.exitCode()) {
        qbsInfo() << Tr::tr("Could not detect selected Xcode with /usr/bin/xcode-select");
    } else {
        QString path = QString::fromLocal8Bit(selectedXcode.readAllStandardOutput());
        addDeveloperPath(path);
    }
    addDeveloperPath(defaultDeveloperPath);

    QProcess launchServices;
    program = QStringLiteral("/usr/bin/mdfind");
    arguments = QStringList(QStringLiteral("kMDItemCFBundleIdentifier == 'com.apple.dt.Xcode'"));
    launchServices.start(program, arguments, QProcess::ReadOnly);
    if (!launchServices.waitForFinished(-1) || launchServices.exitCode()) {
        qbsInfo() << Tr::tr("Could not detect additional Xcode installations with /usr/bin/mdfind");
    } else {
        const auto paths = QString::fromLocal8Bit(launchServices.readAllStandardOutput())
                .split(QLatin1Char('\n'), QBS_SKIP_EMPTY_PARTS);
        for (const QString &path : paths)
            addDeveloperPath(path + QStringLiteral("/Contents/Developer"));
    }
}

void XcodeProbe::setupDefaultToolchains(const QString &devPath, const QString &xcodeName)
{
    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(xcodeName).arg(devPath);

    Profile installationProfile(xcodeName, settings);
    installationProfile.removeProfile();
    installationProfile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("xcode"));
    if (devPath != defaultDeveloperPath)
        installationProfile.setValue(QStringLiteral("xcode.developerPath"), devPath);
    profiles.push_back(installationProfile);

    QStringList platforms;
    platforms << QStringLiteral("macosx")
              << QStringLiteral("iphoneos")
              << QStringLiteral("iphonesimulator")
              << QStringLiteral("appletvos")
              << QStringLiteral("appletvsimulator")
              << QStringLiteral("watchos")
              << QStringLiteral("watchsimulator");
    for (const QString &platform : qAsConst(platforms)) {
        Profile platformProfile(xcodeName + QLatin1Char('-') + platform, settings);
        platformProfile.removeProfile();
        platformProfile.setBaseProfile(installationProfile.name());
        platformProfile.setValue(QStringLiteral("qbs.targetPlatform"), targetOS(platform));
        profiles.push_back(platformProfile);

        const auto architectures = archList(platform);
        for (const QString &arch : architectures) {
            Profile archProfile(xcodeName + QLatin1Char('-') + platform + QLatin1Char('-') + arch,
                                settings);
            archProfile.removeProfile();
            archProfile.setBaseProfile(platformProfile.name());
            archProfile.setValue(QStringLiteral("qbs.architecture"),
                                 qbs::canonicalArchitecture(arch));
            profiles.push_back(archProfile);
        }
    }
}

void XcodeProbe::detectAll()
{
    int i = 1;
    detectDeveloperPaths();
    for (const QString &developerPath : qAsConst(developerPaths)) {
        QString profileName = QStringLiteral("xcode");
        if (developerPath != defaultDeveloperPath) {
            const auto devPath = developerPath.toStdString();
            std::smatch match;
            if (std::regex_match(devPath, match, defaultDeveloperPathRegex))
                profileName += QString::fromStdString(match[1]).toLower().replace(QLatin1Char(' '),
                                                                                  QLatin1Char('-'));
            else
                profileName += QString::number(i++);
        }
        setupDefaultToolchains(developerPath, profileName);
    }
}
} // end anonymous namespace

void xcodeProbe(qbs::Settings *settings, std::vector<qbs::Profile> &profiles)
{
    XcodeProbe probe(settings, profiles);
    probe.detectAll();
}
