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
#include "probe.h"

#include "compilerversion.h"
#include "msvcprobe.h"
#include "xcodeprobe.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/scripttools.h>
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
            << QLatin1String("i686-w64-mingw32.shared") << QLatin1String("x86_64-w64-mingw32.shared")
            << QLatin1String("i686-w64-mingw32.static") << QLatin1String("x86_64-w64-mingw32.static")
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

static QString gccMachineName(const QString &compilerFilePath)
{
    return qsystem(compilerFilePath, QStringList() << QLatin1String("-dumpmachine")).trimmed();
}

static QStringList standardCompilerFileNames()
{
    return QStringList()
            << HostOsInfo::appendExecutableSuffix(QStringLiteral("gcc"))
            << HostOsInfo::appendExecutableSuffix(QStringLiteral("g++"))
            << HostOsInfo::appendExecutableSuffix(QStringLiteral("clang"))
            << HostOsInfo::appendExecutableSuffix(QStringLiteral("clang++"));
}

static void setCommonProperties(Profile &profile, const QString &compilerFilePath,
        const QStringList &toolchainTypes, const QString &architecture)
{
    const QFileInfo cfi(compilerFilePath);
    const QString compilerName = QFileInfo(compilerFilePath).fileName();

    if (toolchainTypes.contains(QStringLiteral("mingw")))
        profile.setValue(QStringLiteral("qbs.targetOS"), QStringList(QStringLiteral("windows")));

    const QString prefix = compilerName.left(compilerName.lastIndexOf(QLatin1Char('-')) + 1);
    if (!prefix.isEmpty())
        profile.setValue(QLatin1String("cpp.toolchainPrefix"), prefix);

    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), cfi.absolutePath());
    profile.setValue(QLatin1String("qbs.toolchain"), toolchainTypes);
    profile.setValue(QLatin1String("qbs.architecture"), canonicalArchitecture(architecture));
    setCompilerVersion(compilerFilePath, toolchainTypes, profile);

    const QString suffix = compilerName.right(compilerName.size() - prefix.size());
    if (!standardCompilerFileNames().contains(suffix))
        qWarning("%s", qPrintable(
                     QString::fromLatin1("'%1' is not a standard compiler file name; "
                                            "you must set the cpp.cCompilerName and "
                                            "cpp.cxxCompilerName properties of this profile "
                                            "manually").arg(compilerName)));
}

class ToolPathSetup
{
public:
    ToolPathSetup(Profile *profile, const QString &path, const QString &toolchainPrefix)
        : m_profile(profile), m_compilerDirPath(path), m_toolchainPrefix(toolchainPrefix)
    {
    }

    void apply(const QString &toolName, const QString &propertyName) const
    {
        QString toolFileName = m_toolchainPrefix + HostOsInfo::appendExecutableSuffix(toolName);
        if (QFile::exists(m_compilerDirPath + QLatin1Char('/') + toolFileName))
            return;
        const QString toolFilePath = findExecutable(toolFileName);
        if (toolFilePath.isEmpty()) {
            qWarning("%s", qPrintable(
                         QString(QLatin1String("'%1' exists neither in '%2' nor in PATH."))
                            .arg(toolFileName, m_compilerDirPath)));
        }
        m_profile->setValue(propertyName, toolFilePath);
    }

private:
    Profile * const m_profile;
    const QString &m_compilerDirPath;
    const QString &m_toolchainPrefix;
};

static Profile createGccProfile(const QString &compilerFilePath, Settings *settings,
                                const QStringList &toolchainTypes,
                                const QString &profileName = QString())
{
    const QString machineName = gccMachineName(compilerFilePath);
    const QStringList compilerTriplet = machineName.split(QLatin1Char('-'));

    if (toolchainTypes.contains(QLatin1String("mingw"))) {
        if (!validMinGWMachines().contains(machineName)) {
            throw ErrorInfo(Tr::tr("Detected gcc platform '%1' is not supported.")
                    .arg(machineName));
        }
    } else if (compilerTriplet.count() < 2) {
        throw qbs::ErrorInfo(Tr::tr("Architecture of compiler for platform '%1' at '%2' not understood.")
                             .arg(machineName, compilerFilePath));
    }

    Profile profile(!profileName.isEmpty() ? profileName : machineName, settings);
    profile.removeProfile();
    setCommonProperties(profile, compilerFilePath, toolchainTypes, compilerTriplet.first());

    // Check whether auxiliary tools reside within the toolchain's install path.
    // This might not be the case when using icecc or another compiler wrapper.
    const QString compilerDirPath = QFileInfo(compilerFilePath).absolutePath();
    const ToolPathSetup toolPathSetup(&profile, compilerDirPath,
                                      profile.value(QStringLiteral("cpp.toolchainPrefix"))
                                      .toString());
    toolPathSetup.apply(QLatin1String("ar"), QLatin1String("cpp.archiverPath"));
    toolPathSetup.apply(QLatin1String("nm"), QLatin1String("cpp.nmPath"));
    if (HostOsInfo::isOsxHost())
        toolPathSetup.apply(QLatin1String("dsymutil"), QLatin1String("cpp.dsymutilPath"));
    else
        toolPathSetup.apply(QLatin1String("objcopy"), QLatin1String("cpp.objcopyPath"));
    toolPathSetup.apply(QLatin1String("strip"), QLatin1String("cpp.stripPath"));

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
    QFileInfo compiler(compilerFilePath);
    if (compilerFilePath == compiler.fileName() && !compiler.exists())
        compiler = QFileInfo(findExecutable(compilerFilePath));

    if (!compiler.exists()) {
        throw qbs::ErrorInfo(Tr::tr("Compiler '%1' not found")
                             .arg(compilerFilePath));
    }

    QStringList toolchainTypes;
    if (toolchainType.isEmpty())
        toolchainTypes = toolchainTypeFromCompilerName(compiler.fileName());
    else
        toolchainTypes = completeToolchainList(toolchainType);

    if (toolchainTypes.contains(QLatin1String("msvc"))) {
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: MSVC toolchains can only be created "
                                    "via the auto-detection mechanism."));
    }

    if (toolchainTypes.contains(QLatin1String("gcc")))
        createGccProfile(compiler.absoluteFilePath(), settings, toolchainTypes, profileName);
    else
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: Unknown toolchain type."));
}
