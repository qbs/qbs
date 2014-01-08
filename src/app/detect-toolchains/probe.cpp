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
#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QStringList>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

static QString searchPath(const QString &path, const QString &me)
{
    foreach (const QString &ppath, path.split(HostOsInfo::pathListSeparator())) {
        const QString fullPath = ppath + QLatin1Char('/') + me;
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
    p.waitForStarted();
    p.waitForFinished();
    return QString::fromLocal8Bit(p.readAll());
}

static void specific_probe(Settings *settings, QList<Profile> &profiles, QString cc)
{
    qbsInfo() << Tr::tr("Trying to detect %1...").arg(cc);

    QString toolchainType;
    QStringList toolchainTypes;
    if (cc.contains(QLatin1String("clang"))) {
        toolchainType = QLatin1String("clang");
        toolchainTypes << QLatin1String("clang") << QLatin1String("llvm") << QLatin1String("gcc");
    } else if (cc.contains(QLatin1String("llvm"))) {
        toolchainType = QLatin1String("llvm-gcc");
        toolchainTypes << QLatin1String("llvm") << QLatin1String("gcc");
    } else if (cc.contains(QLatin1String("gcc"))) {
        toolchainType = QLatin1String("gcc");
        toolchainTypes << QLatin1String("gcc");
    }

    QString path       = QString::fromLocal8Bit(qgetenv("PATH"));
    QString cxx        = QString::fromLocal8Bit(qgetenv("CXX"));
    QString ld         = QString::fromLocal8Bit(qgetenv("LD"));
    QString cross      = QString::fromLocal8Bit(qgetenv("CROSS_COMPILE"));

    QString pathToGcc;

    if (ld.isEmpty())
        ld = QLatin1String("ld");
    if (cxx.isEmpty()) {
        if (toolchainType == QLatin1String("gcc"))
            cxx = QLatin1String("g++");
        else if (toolchainType == QLatin1String("llvm-gcc"))
            cxx = QLatin1String("llvm-g++");
        else if (toolchainType == QLatin1String("clang"))
            cxx = QLatin1String("clang++");
    }
    if (!cross.isEmpty() && !cc.startsWith(QLatin1Char('/'))) {
        pathToGcc = searchPath(path, cross + cc);
        if (QFileInfo(pathToGcc).exists()) {
            if (!cc.contains(cross))
                cc.prepend(cross);
            if (!cxx.contains(cross))
                cxx.prepend(cross);
            if (!ld.contains(cross))
                ld.prepend(cross);
        }
    }
    if (cc.startsWith(QLatin1Char('/')))
        pathToGcc = cc;
    else
        pathToGcc = searchPath(path, cc);

    if (!QFileInfo(pathToGcc).exists()) {
        qbsInfo() << Tr::tr("%1 not found.").arg(cc);
        return;
    }

    QString compilerTriplet = qsystem(pathToGcc, QStringList()
                                      << QLatin1String("-dumpmachine")).simplified();
    QStringList compilerTripletl = compilerTriplet.split(QLatin1Char('-'));
    if (compilerTripletl.count() < 2) {
        qbsError() << QString::fromLocal8Bit("Detected '%1', but I don't understand "
                "its architecture '%2'.").arg(pathToGcc, compilerTriplet);
        return;
    }

    const QString architecture = compilerTripletl.at(0);

    QStringList pathToGccL = pathToGcc.split(QLatin1Char('/'));
    QString compilerName = pathToGccL.takeLast().replace(cc, cxx);

    qbsInfo() << Tr::tr("Toolchain detected:\n"
                        "    binary: %1\n"
                        "    triplet: %2\n"
                        "    arch: %3\n"
                        "    cc: %4").arg(pathToGcc, compilerTriplet, architecture, cc);
    if (!cxx.isEmpty())
        qbsInfo() << Tr::tr("    cxx: %1").arg(cxx);
    if (!ld.isEmpty())
        qbsInfo() << Tr::tr("    ld: %1").arg(ld);

    Profile profile(toolchainType, settings);
    profile.removeProfile();

    profile.setValue(QLatin1String("qbs.toolchain"), toolchainTypes);
    profile.setValue(QLatin1String("qbs.architecture"),
                     HostOsInfo::canonicalArchitecture(architecture));
    profile.setValue(QLatin1String("qbs.endianness"),
                     HostOsInfo::defaultEndianness(architecture));

    if (compilerName.contains(QLatin1Char('-'))) {
        QStringList nl = compilerName.split(QLatin1Char('-'));
        profile.setValue(QLatin1String("cpp.compilerName"), nl.takeLast());
        profile.setValue(QLatin1String("cpp.toolchainPrefix"),
                         nl.join(QLatin1String("-")) + QLatin1Char('-'));
    } else {
        profile.setValue(QLatin1String("cpp.compilerName"), compilerName);
    }
    profile.setValue(QLatin1String("cpp.toolchainInstallPath"),
                     pathToGccL.join(QLatin1String("/")));
    profiles << profile;
}

static void mingwProbe(Settings *settings, QList<Profile> &profiles)
{
    QString mingwPath;
    QString mingwBinPath;
    QString gccPath;
    QByteArray envPath = qgetenv("PATH");
    foreach (const QByteArray &dir, envPath.split(';')) {
        QFileInfo fi(QString::fromLocal8Bit(dir) + QLatin1String("/gcc.exe"));
        if (fi.exists()) {
            mingwPath = QFileInfo(QString::fromLocal8Bit(dir)
                                  + QLatin1String("/..")).canonicalFilePath();
            gccPath = fi.absoluteFilePath();
            mingwBinPath = fi.absolutePath();
            break;
        }
    }
    if (gccPath.isEmpty())
        return;
    QProcess process;
    process.start(gccPath, QStringList() << QLatin1String("-dumpmachine"));
    if (!process.waitForStarted()) {
        qbsError() << "Could not start \"gcc -dumpmachine\".";
        return;
    }
    process.waitForFinished(-1);
    QByteArray gccMachineName = process.readAll().trimmed();
    QStringList validMinGWMachines;
    validMinGWMachines << QLatin1String("mingw32") << QLatin1String("mingw64")
                       << QLatin1String("i686-w64-mingw32") << QLatin1String("x86_64-w64-mingw32");
    if (!validMinGWMachines.contains(QString::fromLocal8Bit(gccMachineName))) {
        qbsError() << QString::fromLocal8Bit("Detected gcc platform '%1' is not supported.")
                      .arg(QString::fromLocal8Bit(gccMachineName));
        return;
    }

    QByteArray architecture = gccMachineName.split('-').first();
    if (architecture == "mingw32")
        architecture = "x86";
    else if (architecture == "mingw64")
        architecture = "x86_64";

    Profile profile(QString::fromLocal8Bit(gccMachineName), settings);
    qbsInfo() << Tr::tr("Platform '%1' detected in '%2'.").arg(profile.name(), mingwPath);
    profile.setValue(QLatin1String("qbs.targetOS"), QStringList(QLatin1String("windows")));
    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), mingwBinPath);
    profile.setValue(QLatin1String("cpp.compilerName"), QLatin1String("g++.exe"));
    profile.setValue(QLatin1String("qbs.toolchain"), QStringList() << QLatin1String("mingw")
                                                                   << QLatin1String("gcc"));
    profile.setValue(QLatin1String("qbs.architecture"),
                     HostOsInfo::canonicalArchitecture(QString::fromLatin1(architecture)));
    profile.setValue(QLatin1String("qbs.endianness"),
                     HostOsInfo::defaultEndianness(QString::fromLatin1(architecture)));
    profiles << profile;
}

int probe(Settings *settings)
{
    QList<Profile> profiles;
    if (HostOsInfo::isWindowsHost()) {
        msvcProbe(settings, profiles);
        mingwProbe(settings, profiles);
    } else if (HostOsInfo::isOsxHost()) {
        xcodeProbe(settings, profiles);
        specific_probe(settings, profiles, QLatin1String("gcc"));
        specific_probe(settings, profiles, QLatin1String("clang"));
    } else {
        specific_probe(settings, profiles, QLatin1String("gcc"));
        specific_probe(settings, profiles, QLatin1String("clang"));
    }

    if (profiles.isEmpty()) {
        qbsWarning() << Tr::tr("Could not detect any toolchains. No profile created.");
    } else if (profiles.count() == 1 && settings->defaultProfile().isEmpty()) {
        const QString profileName = profiles.first().name();
        qbsInfo() << Tr::tr("Making profile '%1' the default.").arg(profileName);
        settings->setValue(QLatin1String("defaultProfile"), profileName);
    }
    return 0;
}
