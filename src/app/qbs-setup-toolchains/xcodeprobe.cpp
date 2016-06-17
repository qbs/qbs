/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "probe.h"
#include "xcodeprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QStringList>
#include <QProcess>
#include <QByteArray>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QRegularExpression>

using namespace qbs;
using Internal::Tr;

namespace {
static const QString defaultDeveloperPath =
        QStringLiteral("/Applications/Xcode.app/Contents/Developer");
static const QRegularExpression defaultDeveloperPathRegex(
        QStringLiteral("^/Applications/Xcode([a-zA-Z0-9 _-]+)\\.app/Contents/Developer$"));

class XcodeProbe
{
public:
    XcodeProbe(qbs::Settings *settings, QList<qbs::Profile> &profiles)
        : settings(settings), profiles(profiles)
    { }

    bool addDeveloperPath(const QString &path);
    void detectDeveloperPaths();
    void setupDefaultToolchains(const QString &devPath, const QString &xCodeName);
    void detectAll();
private:
    qbs::Settings *settings;
    QList<qbs::Profile> &profiles;
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
    developerPaths.append(path);
    qbsInfo() << Tr::tr("Added developer path %1").arg(path);
    return true;
}

void XcodeProbe::detectDeveloperPaths()
{
    QProcess selectedXcode;
    QString program = QLatin1String("/usr/bin/xcode-select");
    QStringList arguments(QLatin1String("--print-path"));
    selectedXcode.start(program, arguments, QProcess::ReadOnly);
    if (!selectedXcode.waitForFinished() || selectedXcode.exitCode()) {
        qbsInfo() << Tr::tr("Could not detect selected Xcode with /usr/bin/xcode-select");
    } else {
        QString path = QString::fromLocal8Bit(selectedXcode.readAllStandardOutput());
        addDeveloperPath(path);
    }
    addDeveloperPath(defaultDeveloperPath);

    QProcess launchServices;
    program = QLatin1String("/usr/bin/mdfind");
    arguments = QStringList(QLatin1String("kMDItemCFBundleIdentifier == 'com.apple.dt.Xcode'"));
    launchServices.start(program, arguments, QProcess::ReadOnly);
    if (!launchServices.waitForFinished() || launchServices.exitCode()) {
        qbsInfo() << Tr::tr("Could not detect additional Xcode installations with /usr/bin/mdfind");
    } else {
        for (const QString &path : QString::fromLocal8Bit(launchServices.readAllStandardOutput())
             .split(QLatin1Char('\n'), QString::SkipEmptyParts))
            addDeveloperPath(path + QStringLiteral("/Contents/Developer"));
    }
}

static QStringList targetOSList(const QString &applePlatformName)
{
    QStringList targetOS;
    if (applePlatformName == QStringLiteral("macosx")) {
        targetOS << QStringLiteral("macos");
    } else if (applePlatformName == QStringLiteral("iphoneos")) {
        targetOS << QStringLiteral("ios");
    } else if (applePlatformName == QStringLiteral("iphonesimulator")) {
        targetOS << QStringLiteral("ios") << QStringLiteral("ios-simulator");
    } else if (applePlatformName == QStringLiteral("appletvos")) {
        targetOS << QStringLiteral("tvos");
    } else if (applePlatformName == QStringLiteral("appletvsimulator")) {
        targetOS << QStringLiteral("tvos") << QStringLiteral("tvos-simulator");
    } else if (applePlatformName == QStringLiteral("watchos")) {
        targetOS << QStringLiteral("watchos");
    } else if (applePlatformName == QStringLiteral("watchsimulator")) {
        targetOS << QStringLiteral("watchos") << QStringLiteral("watchos-simulator");
    }

    targetOS << QStringLiteral("darwin") << QStringLiteral("bsd") << QStringLiteral("unix");
    return targetOS;
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
            archs << QStringLiteral("armv7");
        archs << QStringLiteral("arm64");
    } else if (applePlatformName == QStringLiteral("watchos")) {
        archs << QStringLiteral("armv7k");
    }

    return archs;
}

void XcodeProbe::setupDefaultToolchains(const QString &devPath, const QString &xcodeName)
{
    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(xcodeName).arg(devPath);

    Profile installationProfile(xcodeName, settings);
    installationProfile.removeProfile();
    installationProfile.setValue(QStringLiteral("cpp.compilerName"), QStringLiteral("clang++"));
    installationProfile.setValue(QStringLiteral("cpp.linkerName"), QStringLiteral("clang++"));
    installationProfile.setValue(QStringLiteral("qbs.toolchain"), QStringList()
                                 << QLatin1String("xcode")
                                 << QLatin1String("clang")
                                 << QLatin1String("llvm")
                                 << QLatin1String("gcc"));
    if (devPath != defaultDeveloperPath)
        installationProfile.setValue(QStringLiteral("xcode.developerPath"), devPath);
    profiles << installationProfile;

    QStringList platforms;
    platforms << QStringLiteral("macosx")
              << QStringLiteral("iphoneos")
              << QStringLiteral("iphonesimulator")
              << QStringLiteral("appletvos")
              << QStringLiteral("appletvsimulator")
              << QStringLiteral("watchos")
              << QStringLiteral("watchsimulator");
    for (const QString &platform : platforms) {
        Profile platformProfile(xcodeName + QLatin1Char('-') + platform, settings);
        platformProfile.removeProfile();
        platformProfile.setBaseProfile(installationProfile.name());
        platformProfile.setValue(QStringLiteral("qbs.targetOS"), targetOSList(platform));
        profiles << platformProfile;

        for (const QString &arch : archList(platform)) {
            Profile archProfile(xcodeName + QLatin1Char('-') + platform + QLatin1Char('-') + arch,
                                settings);
            archProfile.removeProfile();
            archProfile.setBaseProfile(platformProfile.name());
            archProfile.setValue(QStringLiteral("qbs.architecture"),
                                 qbs::canonicalArchitecture(arch));
            profiles << archProfile;
        }
    }
}

void XcodeProbe::detectAll()
{
    int i = 1;
    detectDeveloperPaths();
    for (const QString &developerPath : developerPaths) {
        QRegularExpressionMatch match(defaultDeveloperPathRegex.match(developerPath));
        QString profileName = QLatin1String("xcode");
        if (developerPath != defaultDeveloperPath) {
            profileName += match.hasMatch()
                    ? match.capturedTexts().value(1).toLower().replace(QLatin1Char(' '),
                                                                       QLatin1Char('-'))
                    : QString::number(i++);
        }
        setupDefaultToolchains(developerPath, profileName);
    }
}
} // end anonymous namespace

void xcodeProbe(qbs::Settings *settings, QList<qbs::Profile> &profiles)
{
    XcodeProbe probe(settings, profiles);
    probe.detectAll();
}
