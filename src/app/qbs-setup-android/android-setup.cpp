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

#include "android-setup.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/version.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qhash.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstring.h>

#include <algorithm>

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

static QString mapArch(const QString &androidName)
{
    if (androidName == qls("arm64-v8a"))
        return qls("arm64");
    if (androidName == qls("armeabi"))
        return qls("armv5te");
    if (androidName == qls("armeabi-v7a"))
        return qls("armv7a");
    return androidName;
}

struct QtAndroidInfo {
    bool isValid() const { return !arch.isEmpty(); }

    QString qmakePath;
    QString arch;
    QString platform;
};

static QtAndroidInfo getInfoForQtDir(const QString &qtDir)
{
    QtAndroidInfo info;
    info.qmakePath = qbs::Internal::HostOsInfo::appendExecutableSuffix(qtDir + qls("/bin/qmake"));
    if (!QFile::exists(info.qmakePath))
        return info;
    QFile qdevicepri(qtDir + qls("/mkspecs/qdevice.pri"));
    if (!qdevicepri.open(QIODevice::ReadOnly))
        return info;
    while (!qdevicepri.atEnd()) {
        const QByteArray line = qdevicepri.readLine().simplified();
        const bool isArchLine = line.startsWith("DEFAULT_ANDROID_TARGET_ARCH");
        const bool isPlatformLine = line.startsWith("DEFAULT_ANDROID_PLATFORM");
        if (!isArchLine && !isPlatformLine)
            continue;
        const QList<QByteArray> elems = line.split('=');
        if (elems.count() != 2)
            continue;
        const QString rhs = QString::fromLatin1(elems.at(1).trimmed());
        if (isArchLine)
            info.arch = mapArch(rhs);
        else
            info.platform = rhs;
    }
    return info;
}

using QtInfoPerArch = QHash<QString, QtAndroidInfo>;
static QtInfoPerArch getQtAndroidInfo(const QString &qtSdkDir)
{
    QtInfoPerArch archs;
    if (qtSdkDir.isEmpty())
        return archs;

    QStringList qtDirs(qtSdkDir);
    QDirIterator dit(qtSdkDir, QStringList() << QLatin1String("android_*"), QDir::Dirs);
    while (dit.hasNext())
        qtDirs << dit.next();
    for (auto it = qtDirs.cbegin(); it != qtDirs.cend(); ++it) {
        const QtAndroidInfo info = getInfoForQtDir(*it);
        if (info.isValid())
            archs.insert(info.arch, info);
    }
    return archs;
}

static QString maximumPlatform(const QString &platform1, const QString &platform2)
{
    if (platform1.isEmpty())
        return platform2;
    if (platform2.isEmpty())
        return platform1;
    static const QString prefix = qls("android-");
    const QString numberString1 = platform1.mid(prefix.count());
    const QString numberString2 = platform2.mid(prefix.count());
    bool ok;
    const int value1 = numberString1.toInt(&ok);
    if (!ok) {
        qWarning("Ignoring malformed Android platform string '%s'.", qPrintable(platform1));
        return platform2;
    }
    const int value2 = numberString2.toInt(&ok);
    if (!ok) {
        qWarning("Ignoring malformed Android platform string '%s'.", qPrintable(platform2));
        return platform1;
    }
    return prefix + QString::number(std::max(value1, value2));
}

static void setupNdk(qbs::Settings *settings, const QString &profileName, const QString &ndkDirPath,
                     const QString &qtSdkDirPath)
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
    const QStringList archs = expectedArchs();
    const QtInfoPerArch infoPerArch = getQtAndroidInfo(qtSdkDirPath);
    mainProfile.setValue(qls("qbs.architectures"), infoPerArch.isEmpty()
                         ? archs : QStringList(infoPerArch.keys()));
    QStringList searchPaths;
    QString platform;
    for (const QString &arch : archs) {
        const QtAndroidInfo qtAndroidInfo = infoPerArch.value(arch);
        if (!qtAndroidInfo.isValid())
            continue;
        const QString subProName = subProfileName(profileName, arch);
        const QString setupQtPath = qApp->applicationDirPath() + qls("/qbs-setup-qt");
        QProcess setupQt;
        setupQt.start(setupQtPath, QStringList({ qtAndroidInfo.qmakePath, subProName }));
        if (!setupQt.waitForStarted()) {
            throw ErrorInfo(Tr::tr("Setting up Qt profile failed: '%1' "
                                   "could not be started.").arg(setupQtPath));
        }
        if (!setupQt.waitForFinished()) {
            throw ErrorInfo(Tr::tr("Setting up Qt profile failed: Error running '%1' (%2)")
                            .arg(setupQtPath, setupQt.errorString()));
        }
        if (setupQt.exitCode() != 0) {
            throw ErrorInfo(Tr::tr("Setting up Qt profile failed: '%1' returned with "
                                   "exit code %2.").arg(setupQtPath).arg(setupQt.exitCode()));
        }
        settings->sync();
        qbs::Internal::TemporaryProfile p(subProName, settings);
        searchPaths << p.p.value(qls("preferences.qbsSearchPaths")).toStringList();
        platform = maximumPlatform(platform, qtAndroidInfo.platform);
    }
    if (!searchPaths.isEmpty())
        mainProfile.setValue(qls("preferences.qbsSearchPaths"), searchPaths);
    if (!platform.isEmpty())
        mainProfile.setValue(qls("Android.ndk.platform"), platform);
}

void setupAndroid(Settings *settings, const QString &profileName, const QString &sdkDirPath,
                  const QString &ndkDirPath, const QString &qtSdkDirPath)
{
    setupSdk(settings, profileName, sdkDirPath);
    setupNdk(settings, profileName, ndkDirPath, qtSdkDirPath);
}
