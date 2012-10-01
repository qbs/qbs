/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "msvcprobe.h"

#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QDir>
#include <QDebug>

using namespace qbs;

struct WinSDK
{
    QString version;
    QString installPath;
    bool isDefault;
    bool hasCompiler;
};

struct MSVC
{
    QString version;
    QString installPath;
};

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(WinSDK, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(MSVC, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

static void addMSVCPlatform(const QString &settingsPath, QHash<QString, Platform::Ptr> &platforms, const QString &platformName, const QString &installPath, const QString &winSDKPath)
{
    Platform::Ptr platform = platforms.value(platformName);
    if (!platform) {
       platform = Platform::Ptr(new Platform(platformName, settingsPath + "/" + platformName));
       platforms.insert(platform->name, platform);
    }
    platform->settings.setValue("targetOS", "windows");
    platform->settings.setValue("cpp/toolchainInstallPath", installPath);
    platform->settings.setValue("toolchain", "msvc");
    platform->settings.setValue("cpp/windowsSDKPath", winSDKPath);
    QTextStream qstdout(stdout);
    qstdout << "Setting up platform " << platformName << endl;
}

void msvcProbe(const QString &settingsPath, QHash<QString, Platform::Ptr> &platforms)
{
    QTextStream qstdout(stdout);
    qstdout << "Detecting MSVC toolchains..." << endl;

    // 1) Installed SDKs preferred over standalone Visual studio
    QVector<WinSDK> winSDKs;
    WinSDK defaultWinSDK;
    const QSettings sdkRegistry(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows"),
                                QSettings::NativeFormat);
    const QString defaultSdkPath = sdkRegistry.value(QLatin1String("CurrentInstallFolder")).toString();
    if (!defaultSdkPath.isEmpty()) {
        foreach (const QString &sdkKey, sdkRegistry.childGroups()) {
            WinSDK sdk;
            sdk.version = sdkRegistry.value(sdkKey + QLatin1String("/ProductVersion")).toString();
            sdk.installPath = sdkRegistry.value(sdkKey + QLatin1String("/InstallationFolder")).toString();
            sdk.isDefault = (sdk.installPath == defaultSdkPath);
            if (sdk.installPath.isEmpty())
                continue;
            if (sdk.installPath.endsWith(QLatin1Char('\\')))
                sdk.installPath.chop(1);
            sdk.hasCompiler = QFile::exists(sdk.installPath + "/bin/cl.exe");
            if (sdk.isDefault)
                defaultWinSDK = sdk;
            winSDKs += sdk;
        }
    }

    foreach (const WinSDK &sdk, winSDKs) {
        qstdout << "  Windows SDK detected:\n"
                << "    version " << sdk.version << endl
                << "    installed in " << sdk.installPath << endl;
        if (sdk.hasCompiler)
            qstdout << "    This SDK contains C++ compiler(s)." << endl;
        if (sdk.isDefault)
            qstdout << "    This is the default SDK on this machine." << endl;
    }

    // 2) Installed MSVCs
    QVector<MSVC> msvcs;
    const QSettings vsRegistry(
#ifdef Q_OS_WIN64
                QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7"),
#else
                QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7"),
#endif
                QSettings::NativeFormat);
    foreach (const QString &vsName, vsRegistry.allKeys()) {
        // Scan for version major.minor
        const int dotPos = vsName.indexOf(QLatin1Char('.'));
        if (dotPos == -1)
            continue;

        MSVC msvc;
        msvc.installPath = vsRegistry.value(vsName).toString();
        if (msvc.installPath.endsWith(QLatin1Char('\\')))
            msvc.installPath.chop(1);
        // remove the "\\VC" from the end
        msvc.installPath.chop(3);

        int nVersion = vsName.left(dotPos).toInt();
        switch (nVersion) {
        case 8:
            msvc.version = QLatin1String("2005");
            break;
        case 9:
            msvc.version = QLatin1String("2008");
            break;
        case 10:
            msvc.version = QLatin1String("2010");
            break;
        case 11:
            msvc.version = QLatin1String("2012");
            break;
        }

        if (msvc.version.isEmpty()) {
            qstdout << "  Unknown MSVC version " << nVersion << " found." << endl;
            continue;
        }

        // Check existence of various install scripts
        const QString vcvars32bat = msvc.installPath + QLatin1String("/VC/bin/vcvars32.bat");
        if (!QFileInfo(vcvars32bat).isFile())
            continue;

        msvcs += msvc;
    }

    foreach (const MSVC &msvc, msvcs)
        qstdout << "  MSVC detected:\n"
                << "    version " << msvc.version << endl
                << "    installed in " << msvc.installPath << endl;

    if (winSDKs.isEmpty() && msvcs.isEmpty()) {
        qstdout << "Could not detect an installation of the Windows SDK nor Visual Studio." << endl;
        return;
    }

    foreach (const WinSDK &sdk, winSDKs)
        if (sdk.hasCompiler)
            addMSVCPlatform(settingsPath, platforms, QLatin1String("WinSDK") + sdk.version, sdk.installPath, defaultWinSDK.installPath);

    foreach (const MSVC &msvc, msvcs)
        addMSVCPlatform(settingsPath, platforms, QLatin1String("MSVC") + msvc.version, msvc.installPath, defaultWinSDK.installPath);
}
