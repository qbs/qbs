/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#include "android-setup.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/version.h>

#include <QDir>
#include <QHash>
#include <QSet>
#include <QString>

#include <algorithm>

using namespace qbs;
using qbs::Internal::Tr;

static QString qls(const char *s) { return QLatin1String(s); }


static QStringList allPlatforms(const QString &baseDir)
{
    QDir platformsDir(baseDir + qls("/platforms"));
    if (!platformsDir.exists()) {
        throw ErrorInfo(Tr::tr("Expected directory '%1' to be present, but it is not.")
                        .arg(QDir::toNativeSeparators(platformsDir.path())));
    }
    const QStringList platforms = platformsDir.entryList(
                QStringList() << qls("android-*"), QDir::Dirs | QDir::NoDotAndDotDot);
    if (platforms.isEmpty()) {
        throw ErrorInfo(Tr::tr("No platforms found in '%1'.")
                        .arg(QDir::toNativeSeparators(platformsDir.path())));
    }
    return platforms;
}

static bool compareBuildToolVersions(const QString &v1, const QString &v2)
{
    return Internal::Version::fromString(v1) < Internal::Version::fromString(v2);
}

static QString detectBuildToolsVersion(const QString &sdkDir)
{
    QDir baseDir(sdkDir + qls("/build-tools"));
    if (!baseDir.exists()) {
        throw ErrorInfo(Tr::tr("Expected directory '%1' to be present, but it is not.")
                        .arg(QDir::toNativeSeparators(baseDir.path())));
    }
    QStringList versions = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (versions.isEmpty()) {
        throw ErrorInfo(Tr::tr("No build tools found in '%1'.")
                        .arg(QDir::toNativeSeparators(baseDir.path())));
    }

    std::sort(versions.begin(), versions.end(), compareBuildToolVersions);
    return versions.last();
}

static QString detectPlatform(const QString &sdkDir)
{
    const QStringList sdkPlatforms = allPlatforms(sdkDir);

    QString newestPlatform;
    int max = 0;
    foreach (const QString &platform, sdkPlatforms) {
        static const QString prefix = qls("android-");
        if (!platform.startsWith(prefix))
            continue;
        bool ok;
        const int nr = platform.mid(prefix.count()).toInt(&ok);
        if (!ok)
            continue;
        if (nr > max) {
            max = nr;
            newestPlatform = platform;
        }
    }
    if (newestPlatform.isEmpty())
        throw ErrorInfo(Tr::tr("No platforms found in SDK."));
    return newestPlatform;
}

static QStringList expectedArchs()
{
    return QStringList()
            << QStringLiteral("arm64")
            << QStringLiteral("armv5")
            << QStringLiteral("armv7")
            << QStringLiteral("mipsel")
            << QStringLiteral("mips64el")
            << QStringLiteral("x86")
            << QStringLiteral("x86_64");
}


static QString subProfileName(const QString &mainProfileName, const QString &arch)
{
    return mainProfileName + QLatin1Char('-') + arch;
}

void setupSdk(qbs::Settings *settings, const QString &profileName, const QString &sdkDirPath)
{
    if (!QDir(sdkDirPath).exists()) {
        throw ErrorInfo(Tr::tr("SDK directory '%1' does not exist.")
                        .arg(QDir::toNativeSeparators(sdkDirPath)));
    }

    Profile profile(profileName, settings);
    profile.removeProfile();
    profile.setValue(qls("Android.sdk.sdkDir"), QDir::cleanPath(sdkDirPath));
    profile.setValue(qls("Android.sdk.buildToolsVersion"),
                     detectBuildToolsVersion(sdkDirPath));
    profile.setValue(qls("Android.sdk.platform"), detectPlatform(sdkDirPath));
    profile.setValue(qls("qbs.targetOS"), QStringList() << qls("android") << qls("linux")
                     << qls("unix"));
}

void setupNdk(qbs::Settings *settings, const QString &profileName, const QString &ndkDirPath)
{
    if (ndkDirPath.isEmpty())
        return;

    if (!QDir(ndkDirPath).exists()) {
        throw ErrorInfo(Tr::tr("NDK directory '%1' does not exist.")
                        .arg(QDir::toNativeSeparators(ndkDirPath)));
    }

    Profile mainProfile(profileName, settings);
    mainProfile.setValue(qls("Android.ndk.ndkDir"), QDir::cleanPath(ndkDirPath));
    mainProfile.setValue(qls("Android.sdk.ndkDir"), QDir::cleanPath(ndkDirPath));
    mainProfile.setValue(qls("qbs.toolchain"), QStringList() << qls("gcc"));
    foreach (const QString &arch, expectedArchs()) {
        Profile p(subProfileName(profileName, arch), settings);
        p.removeProfile();
        p.setBaseProfile(mainProfile.name());
        p.setValue(qls("qbs.architecture"), arch);
    }
}

void setupAndroid(Settings *settings, const QString &profileName, const QString &sdkDirPath,
                  const QString &ndkDirPath)
{
    setupSdk(settings, profileName, sdkDirPath);
    setupNdk(settings, profileName, ndkDirPath);
}
