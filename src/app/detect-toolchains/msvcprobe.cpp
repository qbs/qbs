/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "msvcprobe.h"

#include "probe.h"
#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QVector>

using namespace qbs;
using Internal::Tr;

class MSVC
{
public:
    QString version;
    QString installPath;
    QStringList architectures;
};

class WinSDK : public MSVC
{
public:
    bool isDefault;
};

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(WinSDK, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(MSVC, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

static void addMSVCPlatform(const MSVC &msvc, Settings *settings, QList<Profile> &profiles,
        QString name, const QString &installPath, const QString &winSDKPath,
        const QString &architecture)
{
    name.append(QLatin1Char('_') + architecture);
    qbsInfo() << Tr::tr("Setting up profile '%1'.").arg(name);
    Profile p(name, settings);
    p.removeProfile();
    p.setValue("qbs.targetOS", QStringList("windows"));
    p.setValue("cpp.toolchainInstallPath", installPath);
    p.setValue("qbs.toolchain", QStringList("msvc"));
    p.setValue("cpp.windowsSDKPath", winSDKPath);
    p.setValue("qbs.architecture", canonicalizeArchitecture(architecture));
    if (msvc.version.toInt() >= 2013) {
        const QStringList flags(QLatin1String("/FS"));
        p.setValue("cpp.platformCFlags", flags);
        p.setValue("cpp.platformCxxFlags", flags);
    }
    profiles << p;
}

static void findSupportedArchitectures(MSVC *msvc, const QString &pathPrefix = QString())
{
    if (QFile::exists(msvc->installPath + pathPrefix + QLatin1String("/cl.exe")))
        msvc->architectures += QLatin1String("x86");
    if (QFile::exists(msvc->installPath + pathPrefix + QLatin1String("/amd64/cl.exe")))
        msvc->architectures += QLatin1String("x86_64");
}

static QString wow6432Key()
{
#ifdef Q_OS_WIN64
    return QLatin1String("\\Wow6432Node");
#else
    return QString();
#endif
}

void msvcProbe(Settings *settings, QList<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Detecting MSVC toolchains...");

    // 1) Installed SDKs preferred over standalone Visual studio
    QVector<WinSDK> winSDKs;
    WinSDK defaultWinSDK;

    const QSettings sdkRegistry(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                                + QLatin1String("\\Microsoft\\Microsoft SDKs\\Windows"),
                                QSettings::NativeFormat);
    const QString defaultSdkPath = sdkRegistry.value(QLatin1String("CurrentInstallFolder")).toString();
    if (!defaultSdkPath.isEmpty()) {
        foreach (const QString &sdkKey, sdkRegistry.childGroups()) {
            WinSDK sdk;
            sdk.version = sdkKey;
            sdk.installPath = sdkRegistry.value(sdkKey + QLatin1String("/InstallationFolder")).toString();
            sdk.isDefault = (sdk.installPath == defaultSdkPath);
            if (sdk.installPath.isEmpty())
                continue;
            if (sdk.installPath.endsWith(QLatin1Char('\\')))
                sdk.installPath.chop(1);
            findSupportedArchitectures(&sdk, QLatin1String("/bin"));
            if (sdk.isDefault)
                defaultWinSDK = sdk;
            winSDKs += sdk;
        }
    }

    foreach (const WinSDK &sdk, winSDKs) {
        qbsInfo() << Tr::tr("  Windows SDK %1 detected:\n"
                            "    installed in %2").arg(sdk.version, sdk.installPath);
        if (!sdk.architectures.isEmpty())
            qbsInfo() << Tr::tr("    This SDK contains C++ compiler(s).");
        if (sdk.isDefault)
            qbsInfo() << Tr::tr("    This is the default SDK on this machine.");
    }

    // 2) Installed MSVCs
    QVector<MSVC> msvcs;
    const QSettings vsRegistry(
                QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                + QLatin1String("\\Microsoft\\VisualStudio\\SxS\\VC7"),
                QSettings::NativeFormat);
    foreach (const QString &vsName, vsRegistry.allKeys()) {
        // Scan for version major.minor
        const int dotPos = vsName.indexOf(QLatin1Char('.'));
        if (dotPos == -1)
            continue;

        MSVC msvc;
        msvc.installPath = vsRegistry.value(vsName).toString();
        if (!msvc.installPath.endsWith(QLatin1Char('\\')))
            msvc.installPath += QLatin1Char('\\');
        msvc.installPath += QLatin1String("bin");
        findSupportedArchitectures(&msvc);

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
        case 12:
            msvc.version = QLatin1String("2013");
            break;
        }

        if (msvc.version.isEmpty()) {
            qbsInfo() << Tr::tr("  Unknown MSVC version %1 found.").arg(nVersion);
            continue;
        }

        // Check existence of various install scripts
        const QString vcvars32bat = msvc.installPath + QLatin1String("/vcvars32.bat");
        if (!QFileInfo(vcvars32bat).isFile())
            continue;

        msvcs += msvc;
    }

    foreach (const MSVC &msvc, msvcs) {
        qbsInfo() << Tr::tr("  MSVC detected:\n"
                            "    version %1\n"
                            "    installed in %2").arg(msvc.version, msvc.installPath);
    }

    if (winSDKs.isEmpty() && msvcs.isEmpty()) {
        qbsInfo() << Tr::tr("Could not detect an installation of "
                            "the Windows SDK or Visual Studio.");
        return;
    }

    foreach (const WinSDK &sdk, winSDKs) {
        foreach (const QString &arch, sdk.architectures) {
            addMSVCPlatform(sdk, settings, profiles, QLatin1String("WinSDK") + sdk.version,
                    sdk.installPath + QLatin1String("\\bin"), defaultWinSDK.installPath, arch);
        }
    }

    foreach (const MSVC &msvc, msvcs) {
        foreach (const QString &arch, msvc.architectures) {
            addMSVCPlatform(msvc, settings, profiles, QLatin1String("MSVC") + msvc.version,
                    msvc.installPath, defaultWinSDK.installPath, arch);
        }
    }
}
