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

// Independent of architecture, build variant and language.
static QStringList commonCompilerFlags()
{
    return QStringList() << qls("-ffunction-sections") << qls("-funwind-tables")
            << qls("-no-canonical-prefixes") << qls("-Wa,--noexecstack")
            << qls("-Werror=format-security");
}

// Independent of architecture and language
static QStringList commonCompilerFlagsDebug()
{
    return commonCompilerFlags() << qls("-fno-omit-frame-pointer") << qls("-fno-strict-aliasing")
            << qls("-O0");
}

// Independent of architecture and language
static QStringList commonCompilerFlagsRelease()
{
    return commonCompilerFlags() << qls("-fomit-frame-pointer");
}

static QStringList arm64FlagsDebug()
{
    return commonCompilerFlagsDebug() << qls("-fpic") << qls("-fstack-protector")
            << qls("-funswitch-loops") << qls("-finline-limit=300");
}

static QStringList arm64FlagsRelease()
{
    return commonCompilerFlagsRelease() << qls("-fpic") << qls("-fstack-protector") << qls("-O2")
            << qls("-fstrict-aliasing") << qls("-funswitch-loops") << qls("-finline-limit=300");
}

static QStringList armeabiFlagsDebug()
{
    return commonCompilerFlagsDebug() << qls("-fpic") << qls("-fstack-protector")
            << qls("-march=armv5te") << qls("-mtune=xscale") << qls("-msoft-float")
            << qls("-finline-limit=64");
}

static QStringList armeabiFlagsRelease()
{
    return commonCompilerFlagsRelease() << qls("-fpic") << qls("-fstack-protector")
            << qls("-march=armv5te") << qls("-mtune=xscale") << qls("-msoft-float") << qls("-Os")
            << qls("-fno-strict-aliasing") << qls("-finline-limit=64");
}

static QStringList armeabiV7aFlagsDebug()
{
    return commonCompilerFlagsDebug() << qls("-fpic") << qls("-fstack-protector")
            << qls("-march=armv7-a") << qls("-mfpu=vfpv3-d16") << qls("-mfloat-abi=softfp")
            << qls("-finline-limit=64");
}

static QStringList armeabiV7aFlagsRelease()
{
    return commonCompilerFlagsRelease() << qls("-fpic") << qls("-fstack-protector")
            << qls("-march=armv7-a") << qls("-mfpu=vfpv3-d16") << qls("-mfloat-abi=softfp")
            << qls("-Os") << qls("-fno-strict-aliasing") << qls("-finline-limit=64");
}

static QStringList armeabiV7aHardFlagsDebug()
{
    return commonCompilerFlagsDebug() << qls("-fpic") << qls("-fstack-protector")
            << qls("-march=armv7-a") << qls("-mfpu=vfpv3-d16") << qls("-mhard-float")
            << qls("-finline-limit=64");
}

static QStringList armeabiV7aHardFlagsRelease()
{
    return commonCompilerFlagsRelease() << qls("-fpic") << qls("-fstack-protector")
            << qls("-march=armv7-a") << qls("-mfpu=vfpv3-d16") << qls("-mhard-float") << qls("-Os")
            << qls("-fno-strict-aliasing") << qls("-finline-limit=64");
}

static QStringList mipsFlagsDebug()
{
    return commonCompilerFlagsDebug() << qls("-fpic") << qls("-finline-functions")
            << qls("-fmessage-length=0") << qls("-fno-inline-functions-called-once")
            << qls("-fgcse-after-reload") << qls("-frerun-cse-after-loop")
            << qls("-frename-registers");
}

static QStringList mipsFlagsRelease()
{
    return commonCompilerFlagsRelease() << qls("-fpic") << qls("-fno-strict-aliasing")
            << qls("-finline-functions") << qls("-fmessage-length=0")
            << qls("-fno-inline-functions-called-once") << qls("-fgcse-after-reload")
            << qls("-frerun-cse-after-loop") << qls("-frename-registers") << qls("-O2")
            << qls("-funswitch-loops") << qls("-finline-limit=300");
}

static QStringList x86FlagsDebug()
{
    return commonCompilerFlagsDebug() << qls("-fstack-protector") << qls("-funswitch-loops")
            << qls("-finline-limit=300");
}

static QStringList x86FlagsRelease()
{
    return commonCompilerFlagsRelease() << qls("-fstack-protector") << qls("-O2")
            << qls("-fstrict-aliasing") << qls("-funswitch-loops") << qls("-finline-limit=300");
}

static QStringList commonLinkerFlags()
{
    return QStringList() << qls("-no-canonical-prefixes") << qls("-Wl,-z,noexecstack")
            << qls("-Wl,-z,relro") << qls("-Wl,-z,now");
}

static QStringList armeabiV7aLinkerFlags()
{
    return commonLinkerFlags() << qls("-march=armv7-a") << qls("-Wl,--fix-cortex-a8");
}

static QStringList armeabiV7aHardLinkerFlags()
{
    return armeabiV7aLinkerFlags() << qls("-Wl,-no-warn-mismatch");
}

class BuildProfile {
public:
    BuildProfile() : linkerFlags(commonLinkerFlags()), hardFp(false) {}

    QString profileSuffix;
    QString abi;
    QString qbsArchName;
    QString toolchainDirName;
    QString toolchainInstallPath;
    QString toolchainPrefix;
    QStringList compilerFlagsDebug;
    QStringList compilerFlagsRelease;
    QStringList linkerFlags;
    bool hardFp;
};

uint qHash(const BuildProfile &arch) { return qHash(arch.profileSuffix); }
bool operator==(const BuildProfile &a1, const BuildProfile &a2) {
    return a1.profileSuffix == a2.profileSuffix;
}
typedef QHash<QString, BuildProfile> BuildProfileMap;

// As of r10c. Let's hope they don't change it gratuitously.
static BuildProfileMap createArchMap()
{
    BuildProfileMap map;

    BuildProfile aarch64;
    aarch64.abi = qls("arm64-v8a");
    aarch64.profileSuffix = aarch64.abi;
    aarch64.qbsArchName = qls("arm64");
    aarch64.toolchainDirName = qls("aarch64-linux-android-4.9");
    aarch64.toolchainPrefix = qls("aarch64-linux-android-");
    aarch64.compilerFlagsDebug = arm64FlagsDebug();
    aarch64.compilerFlagsRelease = arm64FlagsRelease();
    map.insert(aarch64.profileSuffix, aarch64);

    BuildProfile armeabi;
    armeabi.abi = qls("armeabi");
    armeabi.profileSuffix = armeabi.abi;
    armeabi.qbsArchName = qls("arm");
    armeabi.toolchainDirName = qls("arm-linux-androideabi-4.9");
    armeabi.toolchainPrefix = qls("arm-linux-androideabi-");
    armeabi.compilerFlagsDebug = armeabiFlagsDebug();
    armeabi.compilerFlagsRelease = armeabiFlagsRelease();
    map.insert(armeabi.profileSuffix, armeabi);

    BuildProfile armeabiV7a = armeabi;
    armeabiV7a.abi = qls("armeabi-v7a");
    armeabiV7a.profileSuffix = armeabiV7a.abi;
    armeabiV7a.compilerFlagsDebug = armeabiV7aFlagsDebug();
    armeabiV7a.compilerFlagsRelease = armeabiV7aFlagsRelease();
    armeabiV7a.linkerFlags = armeabiV7aLinkerFlags();
    map.insert(armeabiV7a.profileSuffix, armeabiV7a);

    BuildProfile armeabiV7aHard = armeabiV7a;
    armeabiV7aHard.profileSuffix = qls("armeabi-v7a-hard"); // Same abi as above.
    armeabiV7aHard.compilerFlagsDebug = armeabiV7aHardFlagsDebug();
    armeabiV7aHard.compilerFlagsRelease = armeabiV7aHardFlagsRelease();
    armeabiV7aHard.linkerFlags = armeabiV7aHardLinkerFlags();
    armeabiV7aHard.hardFp = true;
    map.insert(armeabiV7aHard.profileSuffix, armeabiV7aHard);

    BuildProfile mips;
    mips.abi = qls("mips");
    mips.profileSuffix = mips.abi;
    mips.qbsArchName = qls("mipsel");
    mips.toolchainDirName = qls("mipsel-linux-android-4.9");
    mips.toolchainPrefix = qls("mipsel-linux-android-");
    mips.compilerFlagsDebug = mipsFlagsDebug();
    mips.compilerFlagsRelease = mipsFlagsRelease();
    map.insert(mips.profileSuffix, mips);

    BuildProfile mips64;
    mips64.abi = qls("mips64");
    mips64.profileSuffix = mips64.abi;
    mips64.qbsArchName = qls("mips64el");
    mips64.toolchainDirName = qls("mips64el-linux-android-4.9");
    mips64.toolchainPrefix = qls("mips64el-linux-android-");
    mips64.compilerFlagsDebug = mipsFlagsDebug();
    mips64.compilerFlagsRelease = mipsFlagsRelease();
    map.insert(mips64.profileSuffix, mips64);

    BuildProfile x86;
    x86.abi = qls("x86");
    x86.profileSuffix = x86.abi;
    x86.qbsArchName = x86.abi;
    x86.toolchainDirName = qls("x86-4.9");
    x86.toolchainPrefix = qls("i686-linux-android-");
    x86.compilerFlagsDebug = x86FlagsDebug();
    x86.compilerFlagsRelease = x86FlagsRelease();
    map.insert(x86.profileSuffix, x86);

    BuildProfile x86_64;
    x86_64.abi = qls("x86_64");
    x86_64.profileSuffix = x86_64.abi;
    x86_64.qbsArchName = x86_64.abi;
    x86_64.toolchainDirName = qls("x86_64-4.9");
    x86_64.toolchainPrefix = qls("x86_64-linux-android-");
    x86_64.compilerFlagsDebug = x86FlagsDebug();
    x86_64.compilerFlagsRelease = x86FlagsRelease();
    map.insert(x86_64.profileSuffix, x86_64);

    return map;
}


static QString subProfileName(const QString &mainProfileName, const BuildProfile &arch)
{
    return mainProfileName + QLatin1Char('_') + arch.profileSuffix;
}

static QString detectToolchainHostArch(const QString &ndkDirPath, const QString &toolchainName)
{
    const QDir tcDir(ndkDirPath + qls("/toolchains/") + toolchainName
                     + qls("/prebuilt"));
    if (!QDir(tcDir).exists()) {
        throw ErrorInfo(Tr::tr("Expected directory '%1' to be present, but it is not.")
                        .arg(QDir::toNativeSeparators(tcDir.path())));
    }
    const QStringList hostArchList = tcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (hostArchList.isEmpty()) {
        throw ErrorInfo(Tr::tr("Directory '%1' is unexpectedly empty.")
                        .arg(QDir::toNativeSeparators(tcDir.path())));
    }
    return hostArchList.first();
}

void setupSdk(qbs::Settings *settings, const QString &profileName, const QString &sdkDirPath)
{
    if (!QDir(sdkDirPath).exists()) {
        throw ErrorInfo(Tr::tr("SDK directory '%1' does not exist.")
                        .arg(QDir::toNativeSeparators(sdkDirPath)));
    }

    Profile profile(profileName, settings);
    profile.setValue(qls("Android.sdk.sdkDir"), QDir::cleanPath(sdkDirPath));
    profile.setValue(qls("Android.sdk.buildToolsVersion"),
                     detectBuildToolsVersion(sdkDirPath));
    profile.setValue(qls("Android.sdk.platform"), detectPlatform(sdkDirPath));
    profile.setValue(qls("qbs.architecture"), qls("blubb"));
    profile.setValue(qls("qbs.targetOS"), QStringList() << qls("android") << qls("linux"));
}

void setupNdk(qbs::Settings *settings, const QString &profileName, const QString &ndkDirPath)
{
    if (ndkDirPath.isEmpty())
        return;

    if (!QDir(ndkDirPath).exists()) {
        throw ErrorInfo(Tr::tr("NDK directory '%1' does not exist.")
                        .arg(QDir::toNativeSeparators(ndkDirPath)));
    }
    QDir stlLibsDir(ndkDirPath + qls("/sources/cxx-stl/stlport/libs"));
    if (!stlLibsDir.exists()) {
        throw ErrorInfo(Tr::tr("Expected directory '%1' to be present, but it is not.")
                        .arg(QDir::toNativeSeparators(stlLibsDir.path())));
    }
    const QStringList archList = stlLibsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (archList.isEmpty()) {
        throw ErrorInfo(Tr::tr("Failed to detect the architectures supported by the NDK, because "
                "directory '%1' is empty.").arg(QDir::toNativeSeparators(stlLibsDir.path())));
    }

    const BuildProfileMap archMap = createArchMap();
    const QString tcHostArchName = detectToolchainHostArch(ndkDirPath,
            archMap.value(archList.first()).toolchainDirName);
    QList<BuildProfile> architectures;
    foreach (const QString &archName, archList) {
        const BuildProfileMap::ConstIterator it = archMap.find(archName);
        if (it == archMap.constEnd()) {
            qDebug("Ignoring unknown architecture '%s'.", qPrintable(archName));
            continue;
        }
        BuildProfile arch = it.value();
        arch.toolchainInstallPath = ndkDirPath + qls("/toolchains/")
                + arch.toolchainDirName + qls("/prebuilt/") + tcHostArchName
                + qls("/bin");
        architectures << arch;
    }

    for (BuildProfileMap::ConstIterator it = archMap.constBegin();
         it != archMap.constEnd(); ++it) {
        Profile(subProfileName(profileName, it.value()), settings).removeProfile();
    }

    Profile mainProfile(profileName, settings);
    mainProfile.setValue(qls("Android.sdk.ndkDir"), ndkDirPath);
    foreach (const BuildProfile &arch, architectures) {
        Profile p(subProfileName(profileName, arch), settings);
        p.setValue(qls("Android.ndk.abi"), arch.abi);
        p.setValue(qls("Android.ndk.buildProfile"), arch.profileSuffix);
        p.setValue(qls("Android.ndk.compilerFlagsDebug"), arch.compilerFlagsDebug);
        p.setValue(qls("Android.ndk.compilerFlagsRelease"), arch.compilerFlagsRelease);
        p.setValue(qls("Android.ndk.hardFp"), arch.hardFp);
        p.setValue(qls("Android.ndk.ndkDir"), QDir::cleanPath(ndkDirPath));
        p.setValue(qls("cpp.compilerName"), qls("gcc"));
        p.setValue(qls("cpp.debugInformation"), true);
        p.setValue(qls("cpp.linkerFlags"), arch.linkerFlags);
        p.setValue(qls("cpp.linkerName"), qls("g++"));
        p.setValue(qls("cpp.toolchainInstallPath"), arch.toolchainInstallPath);
        p.setValue(qls("cpp.toolchainPrefix"), arch.toolchainPrefix);
        p.setValue(qls("qbs.architecture"), arch.qbsArchName);
        p.setValue(qls("qbs.targetOS"), QStringList() << qls("android") << qls("linux"));
        p.setValue(qls("qbs.toolchain"), QStringList(qls("gcc")));
    }
}

void setupAndroid(Settings *settings, const QString &profileName, const QString &sdkDirPath,
                  const QString &ndkDirPath)
{
    setupSdk(settings, profileName, sdkDirPath);
    setupNdk(settings, profileName, ndkDirPath);
}
