/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "probe.h"
#include "keilprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlist.h>
#include <QtCore/qsettings.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

namespace {

static QStringList knownKeilCompilerNames()
{
    return {QStringLiteral("c51"), QStringLiteral("armcc")};
}

static QString guessKeilArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("c51"))
        return QStringLiteral("mcs51");
    if (baseName == QLatin1String("armcc"))
        return QStringLiteral("arm");
    return {};
}

static Profile createKeilProfileHelper(const QFileInfo &compiler, Settings *settings,
                                       QString profileName = QString())
{
    const QString architecture = guessKeilArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty())
        profileName = QLatin1String("keil-") + architecture;

    Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("keil"));
    if (!architecture.isEmpty())
        profile.setValue(QStringLiteral("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                     profile.name(), compiler.absoluteFilePath());
    return profile;
}

static std::vector<KeilInstallInfo> installedKeilsFromPath()
{
    std::vector<KeilInstallInfo> infos;
    const auto compilerNames = knownKeilCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo keilPath(
                    findExecutable(
                        HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!keilPath.exists())
            continue;
        infos.push_back({keilPath.absoluteFilePath(), {}});
    }
    return infos;
}

static std::vector<KeilInstallInfo> installedKeilsFromRegistry()
{
    std::vector<KeilInstallInfo> infos;

    if (HostOsInfo::isWindowsHost()) {

#ifdef Q_OS_WIN64
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Keil\\Products";
#else
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Keil\\Products";
#endif

        // Dictionary for know toolchains.
        static const struct Entry {
            QString productKey;
            QString subExePath;
        } knowToolchains[] = {
            {QStringLiteral("MDK"), QStringLiteral("\\ARMCC\\bin\\armcc.exe")},
            {QStringLiteral("C51"), QStringLiteral("\\BIN\\c51.exe")},
        };

        QSettings registry(QLatin1String(kRegistryNode), QSettings::NativeFormat);
        const auto productGroups = registry.childGroups();
        for (const QString &productKey : productGroups) {
            const auto entryEnd = std::end(knowToolchains);
            const auto entryIt = std::find_if(std::begin(knowToolchains), entryEnd,
                                              [productKey](const Entry &entry) {
                return entry.productKey == productKey;
            });
            if (entryIt == entryEnd)
                continue;

            registry.beginGroup(productKey);
            const QString rootPath = registry.value(QStringLiteral("Path"))
                    .toString();
            if (!rootPath.isEmpty()) {
                // Build full compiler path.
                const QFileInfo keilPath(rootPath + entryIt->subExePath);
                if (keilPath.exists()) {
                    QString version = registry.value(QStringLiteral("Version"))
                            .toString();
                    if (version.startsWith(QLatin1Char('V')))
                        version.remove(0, 1);
                    infos.push_back({keilPath.absoluteFilePath(), version});
                }
            }
            registry.endGroup();
        }

    }

    return infos;
}

} // end of anonymous namespace

bool isKeilCompiler(const QString &compilerName)
{
    return Internal::any_of(knownKeilCompilerNames(), [compilerName](
                            const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createKeilProfile(const QFileInfo &compiler, Settings *settings,
                       QString profileName)
{
    createKeilProfileHelper(compiler, settings, profileName);
}

void keilProbe(Settings *settings, QList<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect KEIL toolchains...");

    std::vector<KeilInstallInfo> allInfos = installedKeilsFromRegistry();
    const std::vector<KeilInstallInfo> pathInfos = installedKeilsFromPath();
    allInfos.insert(std::end(allInfos), std::begin(pathInfos), std::end(pathInfos));

    for (const KeilInstallInfo &info : allInfos) {
        const auto profile = createKeilProfileHelper(info.compilerPath, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No KEIL toolchains found.");
}
