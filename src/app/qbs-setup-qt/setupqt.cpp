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

#include "setupqt.h"

#include "../shared/logging/consolelogger.h"
#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/set.h>
#include <tools/settings.h>
#include <tools/stlutils.h>
#include <tools/toolchains.h>
#include <tools/version.h>

#include <QtCore/qbytearraymatcher.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>

#include <algorithm>
#include <regex>

namespace qbs {
using Internal::none_of;
using Internal::contains;
using Internal::HostOsInfo;
using Internal::Tr;

static QStringList qmakeExecutableNames()
{
    const QString baseName = HostOsInfo::appendExecutableSuffix(QStringLiteral("qmake"));
    QStringList lst(baseName);
    if (HostOsInfo::isLinuxHost()) {
        // Some distributions ship binaries called qmake-qt5 or qmake-qt4.
        lst << baseName + QLatin1String("-qt5") << baseName + QLatin1String("-qt4");
    }
    return lst;
}

static QStringList collectQmakePaths()
{
    const QStringList qmakeExeNames = qmakeExecutableNames();
    QStringList qmakePaths;
    QByteArray environmentPath = qgetenv("PATH");
    const QList<QByteArray> environmentPaths
            = environmentPath.split(HostOsInfo::pathListSeparator().toLatin1());
    for (const QByteArray &path : environmentPaths) {
        for (const QString &qmakeExecutableName : qmakeExeNames) {
            QFileInfo pathFileInfo(QDir(QLatin1String(path)), qmakeExecutableName);
            if (pathFileInfo.exists()) {
                QString qmakePath = pathFileInfo.absoluteFilePath();
                if (!qmakePaths.contains(qmakePath))
                    qmakePaths.push_back(qmakePath);
            }
        }
    }

    return qmakePaths;
}

bool SetupQt::isQMakePathValid(const QString &qmakePath)
{
    QFileInfo qmakeFileInfo(qmakePath);
    return qmakeFileInfo.exists() && qmakeFileInfo.isFile() && qmakeFileInfo.isExecutable();
}

std::vector<QtEnvironment> SetupQt::fetchEnvironments()
{
    std::vector<QtEnvironment> qtEnvironments;
    const auto qmakePaths = collectQmakePaths();
    for (const QString &qmakePath : qmakePaths) {
        const QtEnvironment env = fetchEnvironment(qmakePath);
        if (none_of(qtEnvironments, [&env](const QtEnvironment &otherEnv) {
                        return env.qmakeFilePath == otherEnv.qmakeFilePath;
                    })) {
            qtEnvironments.push_back(env);
        }
    }
    return qtEnvironments;
}

// These functions work only for Qt from installer.
static QStringList qbsToolchainFromDirName(const QString &dir)
{
    if (dir.startsWith(QLatin1String("msvc")))
        return {QStringLiteral("msvc")};
    if (dir.startsWith(QLatin1String("mingw")))
        return {QStringLiteral("mingw"), QStringLiteral("gcc")};
    if (dir.startsWith(QLatin1String("clang")))
        return {QStringLiteral("clang"), QStringLiteral("llvm"), QStringLiteral("gcc")};
    if (dir.startsWith(QLatin1String("gcc")))
        return {QStringLiteral("gcc")};
    return {};
}

static Version msvcVersionFromDirName(const QString &dir)
{
    static const std::regex regexp("^msvc(\\d\\d\\d\\d).*$");
    std::smatch match;
    const std::string dirString = dir.toStdString();
    if (!std::regex_match(dirString, match, regexp))
        return Version{};
    QMap<std::string, std::string> mapping{
        std::make_pair("2005", "14"), std::make_pair("2008", "15"), std::make_pair("2010", "16"),
        std::make_pair("2012", "17"), std::make_pair("2013", "18"), std::make_pair("2015", "19"),
        std::make_pair("2017", "19.1"), std::make_pair("2019", "19.2")
    };
    return Version::fromString(QString::fromStdString(mapping.value(match[1].str())));
}

static QString archFromDirName(const QString &dir)
{
    static const std::regex regexp("^[^_]+_(.*).*$");
    std::smatch match;
    const std::string dirString = dir.toStdString();
    if (!std::regex_match(dirString, match, regexp))
        return {};
    const QString arch = QString::fromStdString(match[1]);
    if (arch == QLatin1String("32"))
        return QStringLiteral("x86");
    if (arch == QLatin1String("64"))
        return QStringLiteral("x86_64");
    if (arch.contains(QLatin1String("arm64")))
        return QStringLiteral("arm64");
    return arch;
}

static QString platformFromDirName(const QString &dir)
{
    if (dir.startsWith(QLatin1String("android")))
        return QStringLiteral("android");
    if (dir == QLatin1String("Boot2Qt"))
        return QStringLiteral("linux");
    return QString::fromStdString(HostOsInfo::hostOSIdentifier());
}

QtEnvironment SetupQt::fetchEnvironment(const QString &qmakePath)
{
    QtEnvironment env;
    env.qmakeFilePath = qmakePath;
    QDir qtDir = QFileInfo(qmakePath).dir();
    if (qtDir.dirName() == QLatin1String("bin")) {
        qtDir.cdUp();
        env.qbsToolchain = qbsToolchainFromDirName(qtDir.dirName());
        env.msvcVersion = msvcVersionFromDirName(qtDir.dirName());
        env.architecture = archFromDirName(qtDir.dirName());
        if (env.msvcVersion.isValid() && env.architecture.isEmpty())
            env.architecture = QStringLiteral("x86");
        env.targetPlatform = platformFromDirName(qtDir.dirName());
        qtDir.cdUp();
        env.qtVersion = Version::fromString(qtDir.dirName());
    }
    return env;
}

static bool isToolchainProfile(const Profile &profile)
{
    const auto actual = Internal::Set<QString>::fromList(
                profile.allKeys(Profile::KeySelectionRecursive));
    Internal::Set<QString> expected{ QStringLiteral("qbs.toolchainType") };
    if (HostOsInfo::isMacosHost())
        expected.insert(QStringLiteral("qbs.targetPlatform")); // match only Xcode profiles
    return Internal::Set<QString>(actual).unite(expected) == actual;
}

static bool isQtProfile(const Profile &profile)
{
    if (!profile.value(QStringLiteral("moduleProviders.Qt.qmakeFilePaths")).toStringList()
            .empty()) {
        return true;
    }

    // For Profiles created with setup-qt < 5.13.
    const QStringList searchPaths
            = profile.value(QStringLiteral("preferences.qbsSearchPaths")).toStringList();
    return std::any_of(searchPaths.cbegin(), searchPaths.cend(), [] (const QString &path) {
        return QFileInfo(path + QStringLiteral("/modules/Qt")).isDir();
    });
}

template <typename T> bool areProfilePropertiesIncompatible(const T &set1, const T &set2)
{
    // Two objects are only considered incompatible if they are both non empty and compare inequal
    // This logic is used for comparing target OS, toolchain lists, and architectures
    return set1.size() > 0 && set2.size() > 0 && set1 != set2;
}

enum Match { MatchFull, MatchPartial, MatchNone };

static Match compatibility(const QtEnvironment &env, const Profile &toolchainProfile)
{
    Match match = MatchFull;

    const auto toolchainType =
            toolchainProfile.value(QStringLiteral("qbs.toolchainType")).toString();
    const auto toolchain = !toolchainType.isEmpty()
            ? canonicalToolchain(toolchainType)
            : toolchainProfile.value(QStringLiteral("qbs.toolchain")).toStringList();

    const auto toolchainNames = Internal::Set<QString>::fromList(toolchain);
    const auto qtToolchainNames = Internal::Set<QString>::fromList(env.qbsToolchain);
    if (areProfilePropertiesIncompatible(toolchainNames, qtToolchainNames)) {
        auto intersection = toolchainNames;
        intersection.intersect(qtToolchainNames);
        if (!intersection.empty())
            match = MatchPartial;
        else
            return MatchNone;
    }

    const auto targetPlatform = toolchainProfile.value(
                QStringLiteral("qbs.targetPlatform")).toString();
    if (!targetPlatform.isEmpty() && targetPlatform != env.targetPlatform)
        return MatchNone;

    const QString toolchainArchitecture = toolchainProfile.value(QStringLiteral("qbs.architecture"))
            .toString();
    if (areProfilePropertiesIncompatible(canonicalArchitecture(env.architecture),
                                         canonicalArchitecture(toolchainArchitecture)))
        return MatchNone;

    if (env.msvcVersion.isValid()) {
        // We want to know for sure that MSVC compiler versions match,
        // because it's especially important for this toolchain
        const Version compilerVersion = Version::fromString(
            toolchainProfile.value(QStringLiteral("cpp.compilerVersion")).toString());

        static const Version vs2017Version{19, 1};
        if (env.msvcVersion >= vs2017Version) {
            if (env.msvcVersion.majorVersion() != compilerVersion.majorVersion()
                    || compilerVersion < vs2017Version) {
                return MatchNone;
            }
        } else if (env.msvcVersion.majorVersion() != compilerVersion.majorVersion()
                || env.msvcVersion.minorVersion() != compilerVersion.minorVersion()) {
            return MatchNone;
        }
    }

    return match;
}

QString profileNameWithoutHostArch(const QString &profileName)
{
    QString result;
    int i = profileName.indexOf(QLatin1Char('-'));
    if (i == -1)
        return result;
    ++i;
    int j = profileName.indexOf(QLatin1Char('_'), i);
    if (j == -1)
        return result;
    result = profileName.mid(0, i) + profileName.mid(j + 1);
    return result;
}

// "Compressing" MSVC profiles means that if MSVC2017-x64 and MSVC2017-x86_x64 fully match,
// then we drop the crosscompiling toolchain MSVC2017-x86_x64.
static void compressMsvcProfiles(QStringList &profiles)
{
    auto it = std::remove_if(profiles.begin(), profiles.end(),
                             [&profiles] (const QString &profileName) {
        int idx = profileName.indexOf(QLatin1Char('_'));
        if (idx == -1)
            return false;
        return contains(profiles, profileNameWithoutHostArch(profileName));
    });
    if (it != profiles.end())
        profiles.erase(it, profiles.end());
}

void SetupQt::saveToQbsSettings(const QString &qtVersionName,
                                const QtEnvironment &qtEnvironment,
                                Settings *settings)
{
    const QString cleanQtVersionName = Profile::cleanName(qtVersionName);
    QString msg = QCoreApplication::translate("SetupQt", "Creating profile '%1'.")
            .arg(cleanQtVersionName);
    printf("%s\n", qPrintable(msg));

    Profile profile(cleanQtVersionName, settings);
    profile.removeProfile();
    profile.setValue(QStringLiteral("moduleProviders.Qt.qmakeFilePaths"),
                     QStringList(qtEnvironment.qmakeFilePath));
    if (!profile.baseProfile().isEmpty())
        return;
    if (isToolchainProfile(profile))
        return;

    QStringList fullMatches;
    QStringList partialMatches;
    const auto profileNames = settings->profiles();
    for (const QString &profileName : profileNames) {
        const Profile otherProfile(profileName, settings);
        if (profileName == profile.name()
                || !isToolchainProfile(otherProfile)
                || isQtProfile(otherProfile))
            continue;

        switch (compatibility(qtEnvironment, otherProfile)) {
        case MatchFull:
            fullMatches << profileName;
            break;
        case MatchPartial:
            partialMatches << profileName;
            break;
        default:
            break;
        }
    }

    if (fullMatches.size() > 1)
        compressMsvcProfiles(fullMatches);

    QString bestMatch;
    if (fullMatches.size() == 1)
        bestMatch = fullMatches.front();
    else if (fullMatches.empty() && partialMatches.size() == 1)
        bestMatch = partialMatches.front();
    if (bestMatch.isEmpty()) {
        QString message = Tr::tr("You may want to set up toolchain information "
                                 "for the generated Qt profile. ");
        if (!fullMatches.empty() || !partialMatches.empty()) {
            message += Tr::tr("Consider setting one of these profiles as this profile's base "
                              "profile: %1.").arg((fullMatches + partialMatches)
                                                  .join(QLatin1String(", ")));
        }
        qbsInfo() << message;
    } else {
        profile.setBaseProfile(bestMatch);
        qbsInfo() << Tr::tr("Setting profile '%1' as the base profile for this profile.")
                     .arg(bestMatch);
    }
}

bool SetupQt::checkIfMoreThanOneQtWithTheSameVersion(const Version &qtVersion,
        const std::vector<QtEnvironment> &qtEnvironments)
{
    bool foundOneVersion = false;
    for (const QtEnvironment &qtEnvironment : qtEnvironments) {
        if (qtEnvironment.qtVersion == qtVersion) {
            if (foundOneVersion)
                return true;
            foundOneVersion = true;
        }
    }

    return false;
}

} // namespace qbs
