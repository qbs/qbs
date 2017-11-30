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

#include "msvcprobe.h"

#include "probe.h"
#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/error.h>
#include <tools/msvcinfo.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>
#include <tools/version.h>
#include <tools/visualstudioversioninfo.h>
#include <tools/vsenvironmentdetector.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstringlist.h>

#include <vector>

using namespace qbs;
using namespace qbs::Internal;
using Internal::Tr;

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(WinSDK, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(MSVC, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

// Not necessary but helps setup-qt automatically associate base profiles
static void setQtHelperProperties(Profile &p, const MSVC *msvc)
{
    QString targetArch = msvc->architecture.split(QLatin1Char('_')).last();
    if (targetArch.isEmpty())
        targetArch = QStringLiteral("x86");
    if (targetArch == QStringLiteral("arm"))
        targetArch = QStringLiteral("armv7");

    p.setValue(QStringLiteral("qbs.architecture"), canonicalArchitecture(targetArch));
    p.setValue(QStringLiteral("cpp.compilerVersion"), msvc->compilerVersion.toString());
}

static void addMSVCPlatform(Settings *settings, QList<Profile> &profiles, QString name, MSVC *msvc)
{
    qbsInfo() << Tr::tr("Setting up profile '%1'.").arg(name);
    Profile p(name, settings);
    p.removeProfile();
    p.setValue(QLatin1String("qbs.targetPlatform"), QLatin1String("windows"));
    p.setValue(QLatin1String("qbs.toolchain"), QStringList(QLatin1String("msvc")));
    p.setValue(QLatin1String("cpp.toolchainInstallPath"), msvc->binPath);
    setQtHelperProperties(p, msvc);
    profiles.push_back(p);
}

struct MSVCArchInfo
{
    QString arch;
    QString binPath;
};

static std::vector<MSVCArchInfo> findSupportedArchitectures(const MSVC &msvc)
{
    std::vector<MSVCArchInfo> result;
    if (msvc.internalVsVersion.majorVersion() < 15) {
        static const QStringList knownArchitectures = QStringList()
            << QStringLiteral("x86")
            << QStringLiteral("amd64_x86")
            << QStringLiteral("amd64")
            << QStringLiteral("x86_amd64")
            << QStringLiteral("ia64")
            << QStringLiteral("x86_ia64")
            << QStringLiteral("x86_arm")
            << QStringLiteral("amd64_arm");
        for (const QString &knownArchitecture : knownArchitectures) {
            MSVCArchInfo ai;
            ai.arch = knownArchitecture;
            ai.binPath = msvc.binPathForArchitecture(knownArchitecture);
            if (QFile::exists(ai.binPath + QLatin1String("/cl.exe")))
                result.push_back(ai);
        }
    } else {
        QDir vcInstallDir(msvc.vcInstallPath);
        const auto hostArchs = vcInstallDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &hostArch : hostArchs) {
            QDir subdir = vcInstallDir;
            if (!subdir.cd(hostArch))
                continue;
            const QString shortHostArch = hostArch.mid(4).toLower();
            const auto archs = subdir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &arch : archs) {
                MSVCArchInfo ai;
                ai.binPath = subdir.absoluteFilePath(arch);
                if (shortHostArch == arch)
                    ai.arch = arch;
                else
                    ai.arch = shortHostArch + QLatin1Char('_') + arch;
                result.push_back(ai);
            }
        }
    }
    return result;
}

static QString wow6432Key()
{
#ifdef Q_OS_WIN64
    return QLatin1String("\\Wow6432Node");
#else
    return QString();
#endif
}

struct MSVCRegistryEntry
{
    QString version;
    QString installDir;
};

static std::vector<MSVCRegistryEntry> installedMSVCsFromRegistry()
{
    std::vector<MSVCRegistryEntry> result;

    // Detect Visual Studio
    const QSettings vsRegistry(
                QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                + QStringLiteral("\\Microsoft\\VisualStudio\\SxS\\VS7"),
                QSettings::NativeFormat);
    const auto vsNames = vsRegistry.childKeys();
    for (const QString &vsName : vsNames) {
        MSVCRegistryEntry entry;
        entry.version = vsName;
        entry.installDir = vsRegistry.value(vsName).toString();
        result.push_back(entry);
    }

    // Detect Visual C++ Build Tools
    QSettings vcbtRegistry(
                QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                + QStringLiteral("\\Microsoft\\VisualCppBuildTools"),
                QSettings::NativeFormat);
    const QStringList &vcbtRegistryChildGroups = vcbtRegistry.childGroups();
    for (const QString &childGroup : vcbtRegistryChildGroups) {
        vcbtRegistry.beginGroup(childGroup);
        bool ok;
        int installed = vcbtRegistry.value(QStringLiteral("Installed")).toInt(&ok);
        if (ok && installed) {
            MSVCRegistryEntry entry;
            entry.version = childGroup;
            const QSettings vsRegistry(
                        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                        + QStringLiteral("\\Microsoft\\VisualStudio\\") + childGroup
                        + QStringLiteral("\\Setup\\VC"),
                        QSettings::NativeFormat);
            entry.installDir = vsRegistry.value(QStringLiteral("ProductDir")).toString();
            result.push_back(entry);
        }
        vcbtRegistry.endGroup();
    }

    return result;
}

static std::vector<MSVC> installedMSVCs()
{
    std::vector<MSVC> msvcs;
    const std::vector<MSVCRegistryEntry> &registryEntries = installedMSVCsFromRegistry();
    for (const MSVCRegistryEntry &registryEntry : registryEntries) {
        MSVC msvc;
        msvc.internalVsVersion = Version::fromString(registryEntry.version);
        if (!msvc.internalVsVersion.isValid())
            continue;

        QDir vsInstallDir(registryEntry.installDir);
        msvc.vsInstallPath = vsInstallDir.absolutePath();
        if (vsInstallDir.dirName() != QStringLiteral("VC")
                && !vsInstallDir.cd(QStringLiteral("VC"))) {
            continue;
        }

        msvc.version = QString::number(Internal::VisualStudioVersionInfo(
            Version::fromString(registryEntry.version)).marketingVersion());
        if (msvc.version.isEmpty()) {
            qbsWarning() << Tr::tr("Unknown MSVC version %1 found.").arg(registryEntry.version);
            continue;
        }

        if (msvc.internalVsVersion.majorVersion() < 15) {
            QDir vcInstallDir = vsInstallDir;
            if (!vcInstallDir.cd(QStringLiteral("bin")))
                continue;
            msvc.vcInstallPath = vcInstallDir.absolutePath();
            msvcs.push_back(msvc);
        } else {
            QDir vcInstallDir = vsInstallDir;
            vcInstallDir.cd(QStringLiteral("Tools/MSVC"));
            const auto vcVersionStrs = vcInstallDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &vcVersionStr : vcVersionStrs) {
                const Version vcVersion = Version::fromString(vcVersionStr);
                if (!vcVersion.isValid())
                    continue;
                QDir specificVcInstallDir = vcInstallDir;
                if (!specificVcInstallDir.cd(vcVersionStr)
                    || !specificVcInstallDir.cd(QStringLiteral("bin"))) {
                    continue;
                }
                msvc.vcInstallPath = specificVcInstallDir.absolutePath();
                msvcs.push_back(msvc);
            }
        }
    }
    return msvcs;
}

void msvcProbe(Settings *settings, QList<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Detecting MSVC toolchains...");

    // 1) Installed SDKs preferred over standalone Visual studio
    std::vector<WinSDK> winSDKs;
    WinSDK defaultWinSDK;

    const QSettings sdkRegistry(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                                + QLatin1String("\\Microsoft\\Microsoft SDKs\\Windows"),
                                QSettings::NativeFormat);
    const QString defaultSdkPath = sdkRegistry.value(QLatin1String("CurrentInstallFolder")).toString();
    if (!defaultSdkPath.isEmpty()) {
        const auto sdkKeys = sdkRegistry.childGroups();
        for (const QString &sdkKey : sdkKeys) {
            WinSDK sdk;
            sdk.version = sdkKey;
            sdk.vcInstallPath = sdkRegistry.value(sdkKey + QLatin1String("/InstallationFolder")).toString();
            sdk.isDefault = (sdk.vcInstallPath == defaultSdkPath);
            if (sdk.vcInstallPath.isEmpty())
                continue;
            if (sdk.vcInstallPath.endsWith(QLatin1Char('\\')))
                sdk.vcInstallPath.chop(1);
            if (sdk.isDefault)
                defaultWinSDK = sdk;
            const auto ais = findSupportedArchitectures(sdk);
            for (const MSVCArchInfo &ai : ais) {
                WinSDK specificSDK = sdk;
                specificSDK.architecture = ai.arch;
                specificSDK.binPath = ai.binPath;
                winSDKs.push_back(specificSDK);
            }
        }
    }

    for (const WinSDK &sdk : qAsConst(winSDKs)) {
        qbsInfo() << Tr::tr("  Windows SDK %1 detected:\n"
                            "    installed in %2").arg(sdk.version, sdk.vcInstallPath);
        if (sdk.isDefault)
            qbsInfo() << Tr::tr("    This is the default SDK on this machine.");
    }

    // 2) Installed MSVCs
    std::vector<MSVC> msvcs;
    const auto instMsvcs = installedMSVCs();
    for (const MSVC &msvc : instMsvcs) {
        if (msvc.internalVsVersion.majorVersion() < 15) {
            // Check existence of various install scripts
            const QString vcvars32bat = msvc.vcInstallPath + QLatin1String("/vcvars32.bat");
            if (!QFileInfo(vcvars32bat).isFile())
                continue;
        }

        const auto ais = findSupportedArchitectures(msvc);
        for (const MSVCArchInfo &ai : ais) {
            MSVC specificMSVC = msvc;
            specificMSVC.architecture = ai.arch;
            specificMSVC.binPath = ai.binPath;
            msvcs.push_back(specificMSVC);
        }
    }

    for (const MSVC &msvc : qAsConst(msvcs)) {
        qbsInfo() << Tr::tr("  MSVC %1 (%2) detected in\n"
                            "    %3").arg(msvc.version, msvc.architecture,
                                          QDir::toNativeSeparators(msvc.binPath));
    }

    if (winSDKs.empty() && msvcs.empty()) {
        qbsInfo() << Tr::tr("Could not detect an installation of "
                            "the Windows SDK or Visual Studio.");
        return;
    }

    qbsInfo() << Tr::tr("Detecting build environment...");
    std::vector<MSVC *> msvcPtrs;
    msvcPtrs.resize(winSDKs.size() + msvcs.size());
    std::transform(winSDKs.begin(), winSDKs.end(), msvcPtrs.begin(),
                   [] (WinSDK &sdk) -> MSVC * { return &sdk; });
    std::transform(msvcs.begin(), msvcs.end(), msvcPtrs.begin() + winSDKs.size(),
                   [] (MSVC &msvc) -> MSVC * { return &msvc; });

    VsEnvironmentDetector envDetector;
    envDetector.start(msvcPtrs);

    for (WinSDK &sdk : winSDKs) {
        const QString name = QLatin1String("WinSDK") + sdk.version + QLatin1Char('-')
                + sdk.architecture;
        try {
            sdk.init();
            addMSVCPlatform(settings, profiles, name, &sdk);
        } catch (const ErrorInfo &error) {
            qbsWarning() << Tr::tr("Failed to set up %1: %2").arg(name, error.toString());
        }
    }

    for (MSVC &msvc : msvcs) {
        const QString name = QLatin1String("MSVC") + msvc.version + QLatin1Char('-')
                + msvc.architecture;
        try {
            msvc.init();
            addMSVCPlatform(settings, profiles, name, &msvc);
        } catch (const ErrorInfo &error) {
            qbsWarning() << Tr::tr("Failed to set up %1: %2").arg(name, error.toString());
        }
    }
}

void createMsvcProfile(const QString &profileName, const QString &compilerFilePath,
                       Settings *settings)
{
    MSVC msvc(compilerFilePath);
    msvc.init();
    QList<Profile> dummy;
    addMSVCPlatform(settings, dummy, profileName, &msvc);
    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                 .arg(profileName, QDir::toNativeSeparators(compilerFilePath));
}
