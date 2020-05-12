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
#include <tools/qttools.h>

#include <QtCore/qbytearraylist.h>
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
    return {QStringLiteral("arm64"), QStringLiteral("armv7a"),
            QStringLiteral("x86"), QStringLiteral("x86_64")};
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
    profile.setValue(qls("qbs.targetPlatform"), qls("android"));
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
    bool isValid() const { return !archs.isEmpty(); }

    QString qmakePath;
    QStringList archs;
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
        // For Qt < 5.14 use DEFAULT_ANDROID_TARGET_ARCH (which is the abi) to compute
        // the architecture
        // DEFAULT_ANDROID_ABIS doesn't exit
        // For Qt >= 5.14:
        // DEFAULT_ANDROID_TARGET_ARCH doesn't exist, use DEFAULT_ANDROID_ABIS to compute
        // the architectures
        const QByteArray line = qdevicepri.readLine().simplified();
        const bool isArchLine = line.startsWith("DEFAULT_ANDROID_TARGET_ARCH");
        const bool isAbisLine = line.startsWith("DEFAULT_ANDROID_ABIS");
        const bool isPlatformLine = line.startsWith("DEFAULT_ANDROID_PLATFORM");
        if (!isArchLine && !isPlatformLine && !isAbisLine)
            continue;
        const QList<QByteArray> elems = line.split('=');
        if (elems.size() != 2)
            continue;
        const QString rhs = QString::fromLatin1(elems.at(1).trimmed());
        if (isArchLine) {
            info.archs << mapArch(rhs);
        } else if (isAbisLine) {
            const auto abis = rhs.split(QLatin1Char(' '));
            for (const QString &abi: abis)
                info.archs << mapArch(abi);
        } else {
            info.platform = rhs;
        }
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
    const QStringList nameFilters{QStringLiteral("android_*"), QStringLiteral("android")};
    QDirIterator dit(qtSdkDir, nameFilters, QDir::Dirs);
    while (dit.hasNext())
        qtDirs << dit.next();
    for (const auto &qtDir : qAsConst(qtDirs)) {
        const QtAndroidInfo info = getInfoForQtDir(qtDir);
        if (info.isValid()) {
            for (const QString &arch: info.archs)
                archs.insert(arch, info);
        }
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
    const QString numberString1 = platform1.mid(prefix.size());
    const QString numberString2 = platform2.mid(prefix.size());
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

static QString getToolchainType(const QString &ndkDirPath)
{
    QFile sourceProperties(ndkDirPath + qls("/source.properties"));
    if (!sourceProperties.open(QIODevice::ReadOnly))
        return QStringLiteral("gcc"); // <= r10
    while (!sourceProperties.atEnd()) {
        const QByteArray curLine = sourceProperties.readLine().simplified();
        static const QByteArray prefix = "Pkg.Revision = ";
        if (!curLine.startsWith(prefix))
            continue;
        qbs::Version ndkVersion = qbs::Version::fromString(
                    QString::fromLatin1(curLine.mid(prefix.size())));
        if (!ndkVersion.isValid()) {
            qWarning("Unexpected format of NDK revision string in '%s'",
                     qPrintable(sourceProperties.fileName()));
            return QStringLiteral("clang");
        }
        return qls(ndkVersion.majorVersion() >= 18 ? "clang" : "gcc");
    }
    qWarning("No revision entry found in '%s'", qPrintable(sourceProperties.fileName()));
    return QStringLiteral("clang");
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
    mainProfile.setValue(qls("qbs.toolchainType"), getToolchainType(ndkDirPath));
    const QStringList archs = expectedArchs();
    const QtInfoPerArch infoPerArch = getQtAndroidInfo(qtSdkDirPath);
    const QStringList archsForProfile = infoPerArch.empty()
            ? archs : QStringList(infoPerArch.keys());
    if (archsForProfile.size() == 1)
        mainProfile.setValue(qls("qbs.architecture"), archsForProfile.front());
    else
        mainProfile.setValue(qls("qbs.architectures"), archsForProfile);
    QStringList qmakeFilePaths;
    QString platform;
    for (const QString &arch : archs) {
        const QtAndroidInfo qtAndroidInfo = infoPerArch.value(arch);
        if (!qtAndroidInfo.isValid())
            continue;
        qmakeFilePaths << qtAndroidInfo.qmakePath;
        platform = maximumPlatform(platform, qtAndroidInfo.platform);
    }
    if (!qmakeFilePaths.empty()) {
        qmakeFilePaths.removeDuplicates();
        mainProfile.setValue(qls("moduleProviders.Qt.qmakeFilePaths"), qmakeFilePaths);
    }
    if (!platform.isEmpty())
        mainProfile.setValue(qls("Android.ndk.platform"), platform);
}

void setupAndroid(Settings *settings, const QString &profileName, const QString &sdkDirPath,
                  const QString &ndkDirPath, const QString &qtSdkDirPath)
{
    setupSdk(settings, profileName, sdkDirPath);
    setupNdk(settings, profileName, ndkDirPath, qtSdkDirPath);
}
