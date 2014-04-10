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
#include "probe.h"

#include "msvcprobe.h"
#include "xcodeprobe.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTextStream>

#include <cstdio>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

static QTextStream qStdout(stdout);
static QTextStream qStderr(stderr);

static QString findExecutable(const QString &fileName)
{
    const QString path = QString::fromLocal8Bit(qgetenv("PATH"));
    foreach (const QString &ppath, path.split(HostOsInfo::pathListSeparator())) {
        const QString fullPath = ppath + QLatin1Char('/') + fileName;
        if (QFileInfo(fullPath).exists())
            return QDir::cleanPath(fullPath);
    }
    return QString();
}

static QString qsystem(const QString &exe, const QStringList &args = QStringList())
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(exe, args);
    if (!p.waitForStarted()) {
        throw qbs::ErrorInfo(Tr::tr("Failed to start compiler '%1': %2")
                             .arg(exe, p.errorString()));
    }
    if (!p.waitForFinished() || p.exitCode() != 0)
        throw qbs::ErrorInfo(Tr::tr("Failed to run compiler '%1': %2").arg(exe, p.errorString()));
    return QString::fromLocal8Bit(p.readAll());
}

static QStringList validMinGWMachines()
{
    // List of MinGW machine names (gcc -dumpmachine) recognized by Qbs
    return QStringList()
            << QLatin1String("mingw32") << QLatin1String("mingw64")
            << QLatin1String("i686-w64-mingw32") << QLatin1String("x86_64-w64-mingw32")
            << QLatin1String("i586-mingw32msvc") << QLatin1String("amd64-mingw32msvc");
}

static QStringList completeToolchainList(const QString &toolchainName)
{
    QStringList toolchains(toolchainName);
    if (toolchainName == QLatin1String("clang"))
        toolchains << completeToolchainList(QLatin1String("llvm"));
    else if (toolchainName == QLatin1String("llvm") ||
             toolchainName == QLatin1String("mingw")) {
        toolchains << completeToolchainList(QLatin1String("gcc"));
    }
    return toolchains;
}

static QStringList toolchainTypeFromCompilerName(const QString &compilerName)
{
    if (compilerName == QLatin1String("cl.exe"))
        return completeToolchainList(QLatin1String("msvc"));
    foreach (const QString &type, (QStringList() << QLatin1String("clang") << QLatin1String("llvm")
                                                 << QLatin1String("mingw") << QLatin1String("gcc")))
        if (compilerName.contains(type))
            return completeToolchainList(type);
    return QStringList();
}

static QString actualCompilerFilePath(const QString &compilerFilePath)
{
    const QFileInfo cfi(compilerFilePath);
    QString compilerFileName = cfi.fileName();

    // People usually want to compile C++. Since we currently link via
    // the compiler executable, using the plain C compiler will likely lead to linking problems.
    compilerFileName.replace(QLatin1String("gcc"), QLatin1String("g++"));
    compilerFileName.replace(QLatin1String("clang"), QLatin1String("clang++"));
    return cfi.absolutePath() + QLatin1Char('/') + compilerFileName;
}

static QString gccMachineName(const QString &compilerFilePath)
{
    return qsystem(compilerFilePath, QStringList() << QLatin1String("-dumpmachine")).trimmed();
}

static void setCommonProperties(Profile &profile, const QString &compilerFilePath,
                                const QStringList &toolchainTypes, const QString &architecture)
{
    QFileInfo cfi(compilerFilePath);
    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), cfi.absolutePath());

    // Correctly set toolchain prefix for GCC cross compilers
    QString compilerName = cfi.fileName();
    QString installPrefix = compilerName;
    if (installPrefix.endsWith(QLatin1String("-gcc")) ||
        installPrefix.endsWith(QLatin1String("-g++"))) {
        compilerName = installPrefix.right(3);
        installPrefix.chop(3);
        profile.setValue(QLatin1String("cpp.toolchainPrefix"), installPrefix);
    }

    profile.setValue(QLatin1String("cpp.compilerName"), compilerName);
    profile.setValue(QLatin1String("qbs.toolchain"), toolchainTypes);
    profile.setValue(QLatin1String("qbs.architecture"),
                     HostOsInfo::canonicalArchitecture(architecture));
    profile.setValue(QLatin1String("qbs.endianness"),
                     HostOsInfo::defaultEndianness(architecture));
}

static Profile createGccProfile(const QString &_compilerFilePath, Settings *settings,
                                const QStringList &toolchainTypes,
                                const QString &profileName = QString())
{
    const QString compilerFilePath = actualCompilerFilePath(_compilerFilePath);
    const QString machineName = gccMachineName(compilerFilePath);
    const QStringList compilerTriplet = machineName.split(QLatin1Char('-'));
    const bool isMingw = toolchainTypes.contains(QLatin1String("mingw"));

    if (isMingw && !validMinGWMachines().contains(machineName)) {
        throw qbs::ErrorInfo(Tr::tr("Detected gcc platform '%1' is not supported.")
                .arg(machineName));
    } else if (compilerTriplet.count() < 2) {
        throw qbs::ErrorInfo(Tr::tr("Architecture of compiler for platform '%1' at '%2' not understood.")
                             .arg(machineName, compilerFilePath));
    }

    Profile profile(!profileName.isEmpty() ? profileName : machineName, settings);
    profile.removeProfile();
    if (isMingw) {
        profile.setValue(QLatin1String("qbs.targetOS"), QStringList(QLatin1String("windows")));
    }

    setCommonProperties(profile, compilerFilePath, toolchainTypes, compilerTriplet.first());
    const QString compilerName = QFileInfo(compilerFilePath).fileName();
    if (compilerName.contains(QLatin1Char('-'))) {
        QStringList nameParts = compilerName.split(QLatin1Char('-'));
        profile.setValue(QLatin1String("cpp.compilerName"), nameParts.takeLast());
        profile.setValue(QLatin1String("cpp.toolchainPrefix"),
                         nameParts.join(QLatin1String("-")) + QLatin1Char('-'));
    }
    qStdout << Tr::tr("Profile '%1' created for '%2'.").arg(profile.name(), compilerFilePath)
            << endl;
    return profile;
}

static void gccProbe(Settings *settings, QList<Profile> &profiles, const QString &compilerName)
{
    qStdout << Tr::tr("Trying to detect %1...").arg(compilerName) << endl;

    const QString crossCompilePrefix = QString::fromLocal8Bit(qgetenv("CROSS_COMPILE"));
    const QString compilerFilePath = findExecutable(crossCompilePrefix + compilerName);
    if (!QFileInfo(compilerFilePath).exists()) {
        qStderr << Tr::tr("%1 not found.").arg(compilerName) << endl;
        return;
    }
    const QString profileName = QFileInfo(compilerFilePath).completeBaseName();
    const QStringList toolchainTypes = toolchainTypeFromCompilerName(compilerName);
    profiles << createGccProfile(compilerFilePath, settings, toolchainTypes, profileName);
}

static void mingwProbe(Settings *settings, QList<Profile> &profiles)
{
    // List of possible compiler binary names for this platform
    QStringList compilerNames;
    if (HostOsInfo::isWindowsHost()) {
        compilerNames << QLatin1String("gcc");
    } else {
        foreach (const QString &machineName, validMinGWMachines()) {
            compilerNames << machineName + QLatin1String("-gcc");
        }
    }

    foreach (const QString &compilerName, compilerNames) {
        const QString gccPath
                = findExecutable(HostOsInfo::appendExecutableSuffix(compilerName));
        if (!gccPath.isEmpty())
            profiles << createGccProfile(gccPath, settings,
                                         completeToolchainList(QLatin1String("mingw")));
    }
}

void probe(Settings *settings)
{
    QList<Profile> profiles;
    if (HostOsInfo::isWindowsHost()) {
        msvcProbe(settings, profiles);
    } else {
        gccProbe(settings, profiles, QLatin1String("gcc"));
        gccProbe(settings, profiles, QLatin1String("clang"));

        if (HostOsInfo::isOsxHost()) {
            xcodeProbe(settings, profiles);
        }
    }

    mingwProbe(settings, profiles);

    if (profiles.isEmpty()) {
        qStderr << Tr::tr("Could not detect any toolchains. No profile created.") << endl;
    } else if (profiles.count() == 1 && settings->defaultProfile().isEmpty()) {
        const QString profileName = profiles.first().name();
        qStdout << Tr::tr("Making profile '%1' the default.").arg(profileName) << endl;
        settings->setValue(QLatin1String("defaultProfile"), profileName);
    }
}

void createProfile(const QString &profileName, const QString &toolchainType,
                   const QString &compilerFilePath, Settings *settings)
{
    QStringList toolchainTypes;
    if (toolchainType.isEmpty())
        toolchainTypes = toolchainTypeFromCompilerName(QFileInfo(compilerFilePath).fileName());
    else
        toolchainTypes = completeToolchainList(toolchainType);

    if (toolchainTypes.contains(QLatin1String("msvc"))) {
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: MSVC toolchains can only be created "
                                    "via the auto-detection mechanism."));
    }

    if (toolchainTypes.contains(QLatin1String("gcc")))
        createGccProfile(compilerFilePath, settings, toolchainTypes, profileName);
    else
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: Unknown toolchain type."));
}
