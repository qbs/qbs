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
#include "probe.h"

#include "msvcprobe.h"
#include "xcodeprobe.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/toolchains.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtextstream.h>

#include <cstdio>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

static QTextStream qStdout(stdout);
static QTextStream qStderr(stderr);

static QString findExecutable(const QString &fileName)
{
    QString fullFileName = fileName;
    if (HostOsInfo::isWindowsHost()
            && !fileName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        fullFileName += QLatin1String(".exe");
    }
    const QString path = QString::fromLocal8Bit(qgetenv("PATH"));
    foreach (const QString &ppath, path.split(HostOsInfo::pathListSeparator())) {
        const QString fullPath = ppath + QLatin1Char('/') + fullFileName;
        if (QFileInfo::exists(fullPath))
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

static QStringList toolchainTypeFromCompilerName(const QString &compilerName)
{
    if (compilerName == QLatin1String("cl.exe"))
        return canonicalToolchain(QLatin1String("msvc"));
    foreach (const QString &type, (QStringList() << QLatin1String("clang") << QLatin1String("llvm")
                                                 << QLatin1String("mingw") << QLatin1String("gcc")))
        if (compilerName.contains(type))
            return canonicalToolchain(type);
    if (compilerName == QLatin1String("g++"))
        return canonicalToolchain(QLatin1String("gcc"));
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
        const QStringList &toolchainTypes)
{
    const QFileInfo cfi(compilerFilePath);
    const QString compilerName = cfi.fileName();

    if (toolchainTypes.contains(QStringLiteral("mingw")))
        profile.setValue(QStringLiteral("qbs.targetOS"), QStringList(QStringLiteral("windows")));

    const QString prefix = compilerName.left(compilerName.lastIndexOf(QLatin1Char('-')) + 1);
    if (!prefix.isEmpty())
        profile.setValue(QLatin1String("cpp.toolchainPrefix"), prefix);

    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), cfi.absolutePath());
    profile.setValue(QLatin1String("qbs.toolchain"), toolchainTypes);

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
    QString m_compilerDirPath;
    QString m_toolchainPrefix;
};

static Profile createGccProfile(const QString &compilerFilePath, Settings *settings,
                                const QStringList &toolchainTypes,
                                const QString &profileName = QString())
{
    const QString machineName = gccMachineName(compilerFilePath);

    if (toolchainTypes.contains(QLatin1String("mingw"))) {
        if (!validMinGWMachines().contains(machineName)) {
            throw ErrorInfo(Tr::tr("Detected gcc platform '%1' is not supported.")
                    .arg(machineName));
        }
    }

    Profile profile(!profileName.isEmpty() ? profileName : machineName, settings);
    profile.removeProfile();
    setCommonProperties(profile, compilerFilePath, toolchainTypes);

    // Check whether auxiliary tools reside within the toolchain's install path.
    // This might not be the case when using icecc or another compiler wrapper.
    const QString compilerDirPath = QFileInfo(compilerFilePath).absolutePath();
    const ToolPathSetup toolPathSetup(&profile, compilerDirPath,
                                      profile.value(QStringLiteral("cpp.toolchainPrefix"))
                                      .toString());
    toolPathSetup.apply(QLatin1String("ar"), QLatin1String("cpp.archiverPath"));
    toolPathSetup.apply(QLatin1String("as"), QLatin1String("cpp.assemblerPath"));
    toolPathSetup.apply(QLatin1String("nm"), QLatin1String("cpp.nmPath"));
    if (profile.value(QStringLiteral("qbs.targetOS"))
            .toStringList().contains(QStringLiteral("darwin")))
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
    QFileInfo cfi(compilerFilePath);
    if (!cfi.exists()) {
        qStderr << Tr::tr("%1 not found.").arg(compilerName) << endl;
        return;
    }
    const QString profileName = cfi.completeBaseName();
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
                                         canonicalToolchain(QLatin1String("mingw")));
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

        if (HostOsInfo::isMacosHost()) {
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
        toolchainTypes = canonicalToolchain(toolchainType);

    if (toolchainTypes.contains(QLatin1String("msvc")))
        createMsvcProfile(profileName, compiler.absoluteFilePath(), settings);
    else if (toolchainTypes.contains(QLatin1String("gcc")))
        createGccProfile(compiler.absoluteFilePath(), settings, toolchainTypes, profileName);
    else
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: Unknown toolchain type."));
}
