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
#include <tools/vsenvironmentdetector.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstringlist.h>

#include <algorithm>
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

static void addMSVCPlatform(Settings *settings, std::vector<Profile> &profiles, QString name, MSVC *msvc)
{
    qbsInfo() << Tr::tr("Setting up profile '%1'.").arg(name);
    Profile p(std::move(name), settings);
    p.removeProfile();
    p.setValue(QStringLiteral("qbs.targetPlatform"), QStringLiteral("windows"));
    p.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("msvc"));
    p.setValue(QStringLiteral("cpp.toolchainInstallPath"), msvc->binPath);
    setQtHelperProperties(p, msvc);
    profiles.push_back(p);
}

static QString wow6432Key()
{
#ifdef Q_OS_WIN64
    return QStringLiteral("\\Wow6432Node");
#else
    return {};
#endif
}

void msvcProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Detecting MSVC toolchains...");

    // 1) Installed SDKs preferred over standalone Visual studio
    std::vector<WinSDK> winSDKs;
    WinSDK defaultWinSDK;

    const QSettings sdkRegistry(QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE") + wow6432Key()
                                + QLatin1String("\\Microsoft\\Microsoft SDKs\\Windows"),
                                QSettings::NativeFormat);
    const QString defaultSdkPath = sdkRegistry.value(QStringLiteral("CurrentInstallFolder")).toString();
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
            const auto ais = MSVC::findSupportedArchitectures(sdk);
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
    std::vector<MSVC> msvcs = MSVC::installedCompilers(ConsoleLogger::instance());

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

void createMsvcProfile(const QFileInfo &compiler, Settings *settings,
                       const QString &profileName)
{
    const auto compilerFilePath = compiler.absoluteFilePath();
    MSVC msvc(compilerFilePath, MSVC::architectureFromClPath(compilerFilePath));
    msvc.init();
    std::vector<Profile> dummy;
    addMSVCPlatform(settings, dummy, profileName, &msvc);
    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                 .arg(profileName, QDir::toNativeSeparators(compilerFilePath));
}
