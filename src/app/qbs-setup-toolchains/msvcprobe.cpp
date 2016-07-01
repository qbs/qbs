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

#include "msvcprobe.h"

#include "probe.h"
#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/error.h>
#include <tools/msvcinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/visualstudioversioninfo.h>
#include <tools/vsenvironmentdetector.h>

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QVector>

using namespace qbs;
using namespace qbs::Internal;
using Internal::Tr;

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(WinSDK, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(MSVC, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

// Not necessary but helps setup-qt automatically associate base profiles
static void setQtHelperProperties(Profile &p, const QString &architecture,
                                  const QString &compilerFilePath)
{
    MSVC msvc(compilerFilePath);
    VsEnvironmentDetector envdetector(&msvc);
    if (!envdetector.start()) {
        qbsWarning() << (QStringLiteral("Detecting the MSVC build environment failed: ")
                         + envdetector.errorString());
        return;
    }

    QString targetArch = architecture.split(QLatin1Char('_')).last();
    if (targetArch.isEmpty())
        targetArch = QStringLiteral("x86");
    if (targetArch == QStringLiteral("arm"))
        targetArch = QStringLiteral("armv7");

    p.setValue(QStringLiteral("qbs.architecture"), canonicalArchitecture(targetArch));
    try {
        const QVariantMap defines = msvc.compilerDefines(compilerFilePath);
        p.setValue(QStringLiteral("cpp.compilerVersionMajor"),
                   defines[QStringLiteral("_MSC_FULL_VER")].toString().mid(0, 2).toInt());
    } catch (const ErrorInfo &error) {
        p.removeProfile();
        qDebug("Warning: Failed to retrieve compiler defines: %s", qPrintable(error.toString()));
    }
}

static void addMSVCPlatform(Settings *settings, QList<Profile> &profiles,
        QString name, const QString &installPath, const QString &architecture,
        bool appendArchToName = true)
{
    QStringList toolchainInstallPath = QStringList() << installPath;
    if (!architecture.isEmpty()) {
        toolchainInstallPath.append(architecture);
        if (appendArchToName)
            name.append(QLatin1Char('-') + architecture);
    }
    qbsInfo() << Tr::tr("Setting up profile '%1'.").arg(name);
    Profile p(name, settings);
    p.removeProfile();
    p.setValue(QLatin1String("qbs.targetOS"), QStringList(QLatin1String("windows")));
    p.setValue(QLatin1String("qbs.toolchain"), QStringList(QLatin1String("msvc")));
    p.setValue(QLatin1String("cpp.toolchainInstallPath"),
               toolchainInstallPath.join(QDir::separator()));
    setQtHelperProperties(p, architecture,
                          toolchainInstallPath.join(QDir::separator()) + QLatin1String("/cl.exe"));
    profiles << p;
}

static void findSupportedArchitectures(MSVC *msvc)
{
    static const QStringList knownArchitectures = QStringList()
            << QString() // x86_x86
            << QStringLiteral("amd64_x86")
            << QStringLiteral("amd64")
            << QStringLiteral("x86_amd64")
            << QStringLiteral("ia64")
            << QStringLiteral("x86_ia64")
            << QStringLiteral("x86_arm")
            << QStringLiteral("amd64_arm");
    for (const QString &knownArchitecture : knownArchitectures) {
        if (QFile::exists(msvc->clPath(knownArchitecture)))
            msvc->architectures += knownArchitecture;
    }
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
            findSupportedArchitectures(&sdk);
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

        msvc.version = QString::number(Internal::VisualStudioVersionInfo(
            Internal::Version::fromString(vsName)).marketingVersion());
        if (msvc.version.isEmpty()) {
            qbsWarning() << Tr::tr("Unknown MSVC version %1 found.").arg(vsName);
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
            addMSVCPlatform(settings, profiles, QLatin1String("WinSDK") + sdk.version,
                    sdk.installPath + QLatin1String("\\bin"), arch);
        }
    }

    foreach (const MSVC &msvc, msvcs) {
        foreach (const QString &arch, msvc.architectures) {
            addMSVCPlatform(settings, profiles, QLatin1String("MSVC") + msvc.version,
                    msvc.installPath, arch);
        }
    }
}

void createMsvcProfile(const QString &profileName, const QString &compilerFilePath,
                       Settings *settings)
{
    MSVC msvc(compilerFilePath);
    QList<Profile> dummy;
    addMSVCPlatform(settings, dummy, profileName, msvc.installPath,
                    msvc.architectures.first(), false);
    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                 .arg(profileName, QDir::toNativeSeparators(compilerFilePath));
}
