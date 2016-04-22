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
#include <QString>

using namespace qbs;
using qbs::Internal::Tr;

static QString qls(const char *s) { return QLatin1String(s); }

static QStringList expectedArchs()
{
    return QStringList()
            << QStringLiteral("arm64")
            << QStringLiteral("armv5te")
            << QStringLiteral("armv7a")
            << QStringLiteral("mips")
            << QStringLiteral("mips64")
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
    if (!sdkDirPath.isEmpty())
        profile.setValue(qls("Android.sdk.sdkDir"), QDir::cleanPath(sdkDirPath));
    profile.setValue(qls("qbs.targetOS"), QStringList() << qls("android") << qls("linux")
                     << qls("unix"));
}

void setupNdk(qbs::Settings *settings, const QString &profileName, const QString &ndkDirPath)
{
    if (!QDir(ndkDirPath).exists()) {
        throw ErrorInfo(Tr::tr("NDK directory '%1' does not exist.")
                        .arg(QDir::toNativeSeparators(ndkDirPath)));
    }

    Profile mainProfile(profileName, settings);
    if (!ndkDirPath.isEmpty()) {
        mainProfile.setValue(qls("Android.ndk.ndkDir"), QDir::cleanPath(ndkDirPath));
        mainProfile.setValue(qls("Android.sdk.ndkDir"), QDir::cleanPath(ndkDirPath));
    }
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
