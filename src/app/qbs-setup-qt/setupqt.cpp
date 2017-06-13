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
#include <qtprofilesetup.h>
#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/set.h>
#include <tools/settings.h>
#include <tools/stlutils.h>
#include <tools/version.h>

#include <QtCore/qbytearraymatcher.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregexp.h>
#include <QtCore/qstringlist.h>

#include <algorithm>

namespace qbs {
using Internal::contains;
using Internal::HostOsInfo;
using Internal::Tr;
using Internal::Version;

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
    QList<QByteArray> environmentPaths
            = environmentPath.split(HostOsInfo::pathListSeparator().toLatin1());
    foreach (const QByteArray &path, environmentPaths) {
        foreach (const QString &qmakeExecutableName, qmakeExeNames) {
            QFileInfo pathFileInfo(QDir(QLatin1String(path)), qmakeExecutableName);
            if (pathFileInfo.exists()) {
                QString qmakePath = pathFileInfo.absoluteFilePath();
                if (!qmakePaths.contains(qmakePath))
                    qmakePaths.append(qmakePath);
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

QList<QtEnvironment> SetupQt::fetchEnvironments()
{
    QList<QtEnvironment> qtEnvironments;

    foreach (const QString &qmakePath, collectQmakePaths()) {
        const QtEnvironment env = fetchEnvironment(qmakePath);
        if (std::find_if(qtEnvironments.constBegin(), qtEnvironments.constEnd(),
                         [&env](const QtEnvironment &otherEnv) {
                                 return env.includePath == otherEnv.includePath;
                          }) == qtEnvironments.constEnd()) {
            qtEnvironments.append(env);
        }
    }

    return qtEnvironments;
}

void SetupQt::addQtBuildVariant(QtEnvironment *env, const QString &buildVariantName)
{
    if (env->qtConfigItems.contains(buildVariantName))
        env->buildVariant << buildVariantName;
}

static QMap<QByteArray, QByteArray> qmakeQueryOutput(const QString &qmakePath)
{
    QProcess qmakeProcess;
    qmakeProcess.start(qmakePath, QStringList() << QLatin1String("-query"));
    if (!qmakeProcess.waitForStarted())
        throw ErrorInfo(SetupQt::tr("%1 cannot be started.").arg(qmakePath));
    qmakeProcess.waitForFinished();
    const QByteArray output = qmakeProcess.readAllStandardOutput();

    QMap<QByteArray, QByteArray> ret;
    foreach (const QByteArray &line, output.split('\n')) {
        int idx = line.indexOf(':');
        if (idx >= 0)
            ret[line.left(idx)] = line.mid(idx + 1).trimmed();
    }
    return ret;
}

static QByteArray readFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QFile::ReadOnly))
        return file.readAll();

    return QByteArray();
}

static QString configVariable(const QByteArray &configContent, const QString &key)
{
    QRegExp regexp(QLatin1String("\\s*") + key + QLatin1String("\\s*\\+{0,1}=(.*)"),
                   Qt::CaseSensitive);

    QList<QByteArray> configContentLines = configContent.split('\n');

    bool success = false;

    foreach (const QByteArray &configContentLine, configContentLines) {
        success = regexp.exactMatch(QString::fromLocal8Bit(configContentLine));
        if (success)
            break;
    }

    if (success)
        return regexp.capturedTexts()[1].simplified();

    return QString();
}

static QStringList configVariableItems(const QByteArray &configContent, const QString &key)
{
    return configVariable(configContent, key).split(QLatin1Char(' '), QString::SkipEmptyParts);
}

typedef QMap<QByteArray, QByteArray> QueryMap;

static QString pathQueryValue(const QueryMap &queryMap, const QByteArray &key)
{
    return QDir::fromNativeSeparators(QString::fromLocal8Bit(queryMap.value(key)));
}

static const QString msvcPrefix = QLatin1String("win32-msvc");

static bool isMsvcQt(const QtEnvironment &env)
{
    return env.mkspecName.startsWith(msvcPrefix);
}

static Version msvcCompilerVersionForYear(int year)
{
    switch (year)
    {
    case 2005:
        return Version(14);
    case 2008:
        return Version(15);
    case 2010:
        return Version(16);
    case 2012:
        return Version(17);
    case 2013:
        return Version(18);
    case 2015:
        return Version(19);
    case 2017:
        return Version(19, 1);
    default:
        return Version();
    }
}

static Version msvcCompilerVersionFromMkspecName(const QString &mkspecName)
{
    return msvcCompilerVersionForYear(mkspecName.mid(msvcPrefix.size()).toInt());
}

QtEnvironment SetupQt::fetchEnvironment(const QString &qmakePath)
{
    QtEnvironment qtEnvironment;
    QueryMap queryOutput = qmakeQueryOutput(qmakePath);

    qtEnvironment.installPrefixPath = pathQueryValue(queryOutput, "QT_INSTALL_PREFIX");
    qtEnvironment.documentationPath = pathQueryValue(queryOutput, "QT_INSTALL_DOCS");
    qtEnvironment.includePath = pathQueryValue(queryOutput, "QT_INSTALL_HEADERS");
    qtEnvironment.libraryPath = pathQueryValue(queryOutput, "QT_INSTALL_LIBS");
    qtEnvironment.binaryPath = pathQueryValue(queryOutput, "QT_HOST_BINS");
    if (qtEnvironment.binaryPath.isEmpty())
        qtEnvironment.binaryPath = pathQueryValue(queryOutput, "QT_INSTALL_BINS");
    qtEnvironment.documentationPath = pathQueryValue(queryOutput, "QT_INSTALL_DOCS");
    qtEnvironment.pluginPath = pathQueryValue(queryOutput, "QT_INSTALL_PLUGINS");
    qtEnvironment.qmlPath = pathQueryValue(queryOutput, "QT_INSTALL_QML");
    qtEnvironment.qmlImportPath = pathQueryValue(queryOutput, "QT_INSTALL_IMPORTS");
    qtEnvironment.qtVersion = QString::fromLocal8Bit(queryOutput.value("QT_VERSION"));

    const Version qtVersion = Version::fromString(qtEnvironment.qtVersion);

    QString mkspecsBaseSrcPath;
    if (qtVersion.majorVersion() >= 5) {
        qtEnvironment.mkspecBasePath
                = pathQueryValue(queryOutput, "QT_HOST_DATA") + QLatin1String("/mkspecs");
        mkspecsBaseSrcPath
                = pathQueryValue(queryOutput, "QT_HOST_DATA/src") + QLatin1String("/mkspecs");
    } else {
        qtEnvironment.mkspecBasePath
                = pathQueryValue(queryOutput, "QT_INSTALL_DATA") + QLatin1String("/mkspecs");
    }

    if (!QFile::exists(qtEnvironment.mkspecBasePath))
        throw ErrorInfo(tr("Cannot extract the mkspecs directory."));

    const QByteArray qconfigContent = readFileContent(qtEnvironment.mkspecBasePath
                                                      + QLatin1String("/qconfig.pri"));
    qtEnvironment.qtMajorVersion = configVariable(qconfigContent,
                                                  QLatin1String("QT_MAJOR_VERSION")).toInt();
    qtEnvironment.qtMinorVersion = configVariable(qconfigContent,
                                                  QLatin1String("QT_MINOR_VERSION")).toInt();
    qtEnvironment.qtPatchVersion = configVariable(qconfigContent,
                                                  QLatin1String("QT_PATCH_VERSION")).toInt();
    qtEnvironment.qtNameSpace = configVariable(qconfigContent, QLatin1String("QT_NAMESPACE"));
    qtEnvironment.qtLibInfix = configVariable(qconfigContent, QLatin1String("QT_LIBINFIX"));
    qtEnvironment.architecture = configVariable(qconfigContent, QLatin1String("QT_TARGET_ARCH"));
    if (qtEnvironment.architecture.isEmpty())
        qtEnvironment.architecture = configVariable(qconfigContent, QLatin1String("QT_ARCH"));
    if (qtEnvironment.architecture.isEmpty())
        qtEnvironment.architecture = QLatin1String("x86");
    qtEnvironment.configItems = configVariableItems(qconfigContent, QLatin1String("CONFIG"));
    qtEnvironment.qtConfigItems = configVariableItems(qconfigContent, QLatin1String("QT_CONFIG"));

    // retrieve the mkspec
    if (qtVersion.majorVersion() >= 5) {
        const QString mkspecName = QString::fromLocal8Bit(queryOutput.value("QMAKE_XSPEC"));
        qtEnvironment.mkspecName = mkspecName;
        qtEnvironment.mkspecPath = qtEnvironment.mkspecBasePath + QLatin1Char('/') + mkspecName;
        if (!mkspecsBaseSrcPath.isEmpty() && !QFile::exists(qtEnvironment.mkspecPath))
            qtEnvironment.mkspecPath = mkspecsBaseSrcPath + QLatin1Char('/') + mkspecName;
    } else {
        if (HostOsInfo::isWindowsHost()) {
            const QString baseDirPath = qtEnvironment.mkspecBasePath + QLatin1String("/default/");
            const QByteArray fileContent = readFileContent(baseDirPath
                                                           + QLatin1String("qmake.conf"));
            qtEnvironment.mkspecPath = configVariable(fileContent,
                                                      QLatin1String("QMAKESPEC_ORIGINAL"));
            if (!QFile::exists(qtEnvironment.mkspecPath)) {
                // Work around QTBUG-28792.
                // The value of QMAKESPEC_ORIGINAL is wrong for MinGW packages. Y u h8 me?
                const QRegExp rex(QLatin1String("\\binclude\\(([^)]+)/qmake\\.conf\\)"));
                if (rex.indexIn(QString::fromLocal8Bit(fileContent)) != -1)
                    qtEnvironment.mkspecPath = QDir::cleanPath(baseDirPath + rex.cap(1));
            }
        } else {
            qtEnvironment.mkspecPath = QFileInfo(qtEnvironment.mkspecBasePath
                                                 + QLatin1String("/default")).symLinkTarget();
        }

        // E.g. in qmake.conf for Qt 4.8/mingw we find this gem:
        //    QMAKESPEC_ORIGINAL=C:\\Qt\\Qt\\4.8\\mingw482\\mkspecs\\win32-g++
        qtEnvironment.mkspecPath = QDir::cleanPath(qtEnvironment.mkspecPath);

        qtEnvironment.mkspecName = qtEnvironment.mkspecPath;
        int idx = qtEnvironment.mkspecName.lastIndexOf(QLatin1Char('/'));
        if (idx != -1)
            qtEnvironment.mkspecName.remove(0, idx + 1);
    }

    // determine MSVC version
    if (isMsvcQt(qtEnvironment)) {
        bool ok;
        qtEnvironment.msvcVersion.setMajorVersion(
                    configVariable(qconfigContent,
                                   QLatin1String("QT_MSVC_MAJOR_VERSION")).toInt(&ok));
        if (ok) {
            qtEnvironment.msvcVersion.setMinorVersion(
                        configVariable(qconfigContent,
                                       QLatin1String("QT_MSVC_MINOR_VERSION")).toInt(&ok));
        }
        if (ok) {
            qtEnvironment.msvcVersion.setPatchLevel(
                        configVariable(qconfigContent,
                                       QLatin1String("QT_MSVC_PATCH_VERSION")).toInt(&ok));
        }
        if (!ok)
            qtEnvironment.msvcVersion = msvcCompilerVersionFromMkspecName(qtEnvironment.mkspecName);
    }

    // determine whether we have a framework build
    qtEnvironment.frameworkBuild = qtEnvironment.mkspecPath.contains(QLatin1String("macx"))
            && qtEnvironment.configItems.contains(QLatin1String("qt_framework"));

    // determine whether Qt is built with debug, release or both
    addQtBuildVariant(&qtEnvironment, QLatin1String("debug"));
    addQtBuildVariant(&qtEnvironment, QLatin1String("release"));

    if (!QFileInfo(qtEnvironment.mkspecPath).exists())
        throw ErrorInfo(tr("mkspec '%1' does not exist").arg(qtEnvironment.mkspecPath));

    return qtEnvironment;
}

static bool isToolchainProfile(const Profile &profile)
{
    const auto actual = Internal::Set<QString>::fromList(
                profile.allKeys(Profile::KeySelectionRecursive));
    Internal::Set<QString> expected = Internal::Set<QString> { QLatin1String("qbs.toolchain") };
    if (HostOsInfo::isMacosHost())
        expected.insert(QLatin1String("qbs.targetOS")); // match only Xcode profiles
    return Internal::Set<QString>(actual).unite(expected) == actual;
}

static bool isQtProfile(const Profile &profile)
{
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
    return !set1.isEmpty() && !set2.isEmpty() && set1 != set2;
}

static QStringList qbsTargetOsFromQtMkspec(const QString &mkspec)
{
    if (mkspec.startsWith(QLatin1String("aix-")))
        return QStringList()  << QLatin1String("aix") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("android-")))
        return QStringList()  << QLatin1String("android") << QLatin1String("linux")
                              << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("blackberry")))
        return QStringList()  << QLatin1String("blackberry") << QLatin1String("qnx")
                              << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("darwin-")))
        return QStringList() << QLatin1String("darwin") << QLatin1String("bsd")
                             << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("freebsd-")))
        return QStringList() << QLatin1String("freebsd") << QLatin1String("bsd")
                             << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("haiku-")))
        return QStringList() << QLatin1String("haiku");
    if (mkspec.startsWith(QLatin1String("hpux-")) || mkspec.startsWith(QLatin1String("hpuxi-")))
        return QStringList() << QLatin1String("hpux") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("hurd-")))
        return QStringList() << QLatin1String("hurd") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("irix-")))
        return QStringList() << QLatin1String("irix") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("linux-")))
        return QStringList() << QLatin1String("linux") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("lynxos-")))
        return QStringList() << QLatin1String("lynx") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("macx-")))
        return QStringList() << (mkspec.startsWith(QLatin1String("macx-ios-"))
                                 ? QLatin1String("ios") : QLatin1String("macos"))
                             << QLatin1String("darwin") << QLatin1String("bsd")
                             << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("nacl-")) || mkspec.startsWith(QLatin1String("nacl64-")))
        return QStringList() << QLatin1String("nacl");
    if (mkspec.startsWith(QLatin1String("netbsd-")))
        return QStringList() << QLatin1String("netbsd") << QLatin1String("bsd")
                             << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("openbsd-")))
        return QStringList() << QLatin1String("openbsd") << QLatin1String("bsd")
                             << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("qnx-")))
        return QStringList() << QLatin1String("qnx") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("sco-")))
        return QStringList() << QLatin1String("sco") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("solaris-")))
        return QStringList() << QLatin1String("solaris") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("tru64-")))
        return QStringList() << QLatin1String("tru64") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("unixware-")))
        return QStringList() << QLatin1String("unixware") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("vxworks-")))
        return QStringList() << QLatin1String("vxworks") << QLatin1String("unix");
    if (mkspec.startsWith(QLatin1String("win32-")))
        return QStringList() << QLatin1String("windows");
    if (mkspec.startsWith(QLatin1String("wince")))
        return QStringList() << QLatin1String("windowsce") << QLatin1String("windows");
    if (mkspec.startsWith(QLatin1String("winphone-")))
        return QStringList() << QLatin1String("windowsphone") << QLatin1String("winrt") << QLatin1String("windows");
    if (mkspec.startsWith(QLatin1String("winrt-")))
        return QStringList() << QLatin1String("winrt") << QLatin1String("windows");
    return QStringList();
}

static QStringList qbsToolchainFromQtMkspec(const QString &mkspec)
{
    if (mkspec.contains(QLatin1String("-msvc")))
        return QStringList() << QLatin1String("msvc");
    if (mkspec == QLatin1String("win32-g++"))
        return QStringList() << QLatin1String("mingw") << QLatin1String("gcc");

    if (mkspec.contains(QLatin1String("-clang")))
        return QStringList() << QLatin1String("clang") << QLatin1String("llvm")
                             << QLatin1String("gcc");
    if (mkspec.contains(QLatin1String("-llvm")))
        return QStringList() << QLatin1String("llvm") << QLatin1String("gcc");
    if (mkspec.contains(QLatin1String("-g++")))
        return QStringList() << QLatin1String("gcc");

    // Worry about other, less common toolchains (ICC, QCC, etc.) later...
    return QStringList();
}

enum Match { MatchFull, MatchPartial, MatchNone };

static Match compatibility(const QtEnvironment &env, const Profile &toolchainProfile)
{
    Match match = MatchFull;

    const auto toolchainNames = Internal::Set<QString>::fromList(
                toolchainProfile.value(QLatin1String("qbs.toolchain")).toStringList());
    const auto mkspecToolchainNames = Internal::Set<QString>::fromList(
                qbsToolchainFromQtMkspec(env.mkspecName));
    if (areProfilePropertiesIncompatible(toolchainNames, mkspecToolchainNames)) {
        auto intersection = toolchainNames;
        intersection.intersect(mkspecToolchainNames);
        if (!intersection.isEmpty())
            match = MatchPartial;
        else
            return MatchNone;
    }

    const auto targetOsNames = Internal::Set<QString>::fromList(
                toolchainProfile.value(QLatin1String("qbs.targetOS")).toStringList());
    if (areProfilePropertiesIncompatible(
                Internal::Set<QString>::fromList(qbsTargetOsFromQtMkspec(env.mkspecName)),
                targetOsNames))
        return MatchNone;

    const QString toolchainArchitecture = toolchainProfile.value(QLatin1String("qbs.architecture"))
            .toString();
    if (areProfilePropertiesIncompatible(canonicalArchitecture(env.architecture),
                                         canonicalArchitecture(toolchainArchitecture)))
        return MatchNone;

    if (env.msvcVersion.isValid()) {
        // We want to know for sure that MSVC compiler versions match,
        // because it's especially important for this toolchain
        const Version compilerVersion = Version::fromString(
            toolchainProfile.value(QLatin1String("cpp.compilerVersion")).toString());
        if (env.msvcVersion.majorVersion() != compilerVersion.majorVersion()
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

void SetupQt::saveToQbsSettings(const QString &qtVersionName, const QtEnvironment &qtEnvironment,
                                Settings *settings)
{
    const QString cleanQtVersionName = Profile::cleanName(qtVersionName);
    QString msg = QCoreApplication::translate("SetupQt", "Creating profile '%1'.")
            .arg(cleanQtVersionName);
    printf("%s\n", qPrintable(msg));

    const ErrorInfo errorInfo = setupQtProfile(cleanQtVersionName, settings, qtEnvironment);
    if (errorInfo.hasError())
        throw errorInfo;

    // If this profile does not specify a toolchain and we find exactly one profile that looks
    // like it might have been added by qbs-setup-toolchains, let's use that one as our
    // base profile.
    Profile profile(cleanQtVersionName, settings);
    if (!profile.baseProfile().isEmpty())
        return;
    if (isToolchainProfile(profile))
        return;

    QStringList fullMatches;
    QStringList partialMatches;
    foreach (const QString &profileName, settings->profiles()) {
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
    if (fullMatches.count() == 1)
        bestMatch = fullMatches.first();
    else if (fullMatches.isEmpty() && partialMatches.count() == 1)
        bestMatch = partialMatches.first();
    if (bestMatch.isEmpty()) {
        QString message = Tr::tr("You need to set up toolchain information before you can "
                                 "use this Qt version for building. ");
        if (fullMatches.isEmpty() && partialMatches.isEmpty()) {
            message += Tr::tr("However, no toolchain profile was found. Either create one "
                              "using qbs-setup-toolchains and set it as this profile's "
                              "base profile or add the toolchain settings manually "
                              "to this profile.");
        } else {
            message += Tr::tr("Consider setting one of these profiles as this profile's base "
                              "profile: %1.").arg((fullMatches + partialMatches)
                                                  .join(QLatin1String(", ")));
        }
        qbsWarning() << message;
    } else {
        profile.setBaseProfile(bestMatch);
        qbsInfo() << Tr::tr("Setting profile '%1' as the base profile for this profile.")
                     .arg(bestMatch);
    }
}

bool SetupQt::checkIfMoreThanOneQtWithTheSameVersion(const QString &qtVersion,
        const QList<QtEnvironment> &qtEnvironments)
{
    bool foundOneVersion = false;
    foreach (const QtEnvironment &qtEnvironment, qtEnvironments) {
        if (qtEnvironment.qtVersion == qtVersion) {
            if (foundOneVersion)
                return true;
            foundOneVersion = true;
        }
    }

    return false;
}

} // namespace qbs
