/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
#include "osxprobe.h"
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
    if (cc.contains("clang")) {
        toolchainType = "clang";
        toolchainTypes << "clang" << "llvm" << "gcc";
    } else if (cc.contains("llvm")) {
        toolchainType = "llvm-gcc";
        toolchainTypes << "llvm" << "gcc";
    } else if (cc.contains("gcc")) {
        toolchainType = "gcc";
        toolchainTypes << "gcc";
    }

    QString path       = QString::fromLocal8Bit(qgetenv("PATH"));
    QString cxx        = QString::fromLocal8Bit(qgetenv("CXX"));
    QString ld         = QString::fromLocal8Bit(qgetenv("LD"));
    QString cross      = QString::fromLocal8Bit(qgetenv("CROSS_COMPILE"));
    QString arch       = QString::fromLocal8Bit(qgetenv("ARCH"));

    QString pathToGcc;
    QString architecture;
    QString endianness;

    QString uname = qsystem("uname", QStringList() << "-m").simplified();

    if (arch.isEmpty())
        arch = uname;

    // HACK: "uname -m" reports "i386" but "gcc -dumpmachine" reports "i686" on OS X.
    if (HostOsInfo::isOsxHost() && arch == "i386")
        arch = "i686";

    if (ld.isEmpty())
        ld = "ld";
    if (cxx.isEmpty()) {
        if (toolchainType == "gcc")
            cxx = "g++";
        else if (toolchainType == "llvm-gcc")
            cxx = "llvm-g++";
        else if (toolchainType == "clang")
            cxx = "clang++";
    }
    if(!cross.isEmpty() && !cc.startsWith("/")) {
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
    if (cc.startsWith("/"))
        pathToGcc = cc;
    else
        pathToGcc = searchPath(path, cc);

    if (!QFileInfo(pathToGcc).exists()) {
        qbsInfo() << Tr::tr("%1 not found.").arg(cc);
        return;
    }

    QString compilerTriplet = qsystem(pathToGcc, QStringList() << "-dumpmachine").simplified();
    QStringList compilerTripletl = compilerTriplet.split('-');
    if (compilerTripletl.count() < 2) {
        qbsError() << QString::fromLocal8Bit("Detected '%1', but I don't understand "
                "its architecture '%2'.").arg(pathToGcc, compilerTriplet);
        return;
    }

    architecture = compilerTripletl.at(0);
    if (architecture.contains("arm")) {
        endianness = "big";
    } else {
        endianness = "little";
    }

    QStringList pathToGccL = pathToGcc.split('/');
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

    profile.setValue("qbs.toolchain", toolchainTypes);
    profile.setValue("qbs.architecture", architecture);
    profile.setValue("qbs.endianness", endianness);

    if (compilerName.contains('-')) {
        QStringList nl = compilerName.split('-');
        profile.setValue("cpp.compilerName", nl.takeLast());
        profile.setValue("cpp.toolchainPrefix", nl.join("-") + '-');
    } else {
        profile.setValue("cpp.compilerName", compilerName);
    }
    profile.setValue("cpp.toolchainInstallPath", pathToGccL.join("/"));
    profiles << profile;
}

static void mingwProbe(Settings *settings, QList<Profile> &profiles)
{
    QString mingwPath;
    QString mingwBinPath;
    QString gccPath;
    QByteArray envPath = qgetenv("PATH");
    foreach (const QByteArray &dir, envPath.split(';')) {
        QFileInfo fi(dir + "/gcc.exe");
        if (fi.exists()) {
            mingwPath = QFileInfo(dir + "/..").canonicalFilePath();
            gccPath = fi.absoluteFilePath();
            mingwBinPath = fi.absolutePath();
            break;
        }
    }
    if (gccPath.isEmpty())
        return;
    QProcess process;
    process.start(gccPath, QStringList() << "-dumpmachine");
    if (!process.waitForStarted()) {
        qbsError() << "Could not start \"gcc -dumpmachine\".";
        return;
    }
    process.waitForFinished(-1);
    QByteArray gccMachineName = process.readAll().trimmed();
    QStringList validMinGWMachines;
    validMinGWMachines << "mingw32" << "mingw64" << "i686-w64-mingw32" << "x86_64-w64-mingw32";
    if (!validMinGWMachines.contains(gccMachineName)) {
        qbsError() << QString::fromLocal8Bit("Detected gcc platform '%1' is not supported.")
                      .arg(QString::fromLocal8Bit(gccMachineName));
        return;
    }


    Profile profile(QString::fromLocal8Bit(gccMachineName), settings);
    qbsInfo() << Tr::tr("Platform '%1' detected in '%2'.").arg(profile.name(), mingwPath);
    profile.setValue("qbs.targetOS", QStringList("windows"));
    profile.setValue("cpp.toolchainInstallPath", mingwBinPath);
    profile.setValue("cpp.compilerName", QLatin1String("g++.exe"));
    profile.setValue("qbs.toolchain", QStringList() << "mingw" << "gcc");
    profiles << profile;
}

int probe(Settings *settings)
{
    QList<Profile> profiles;
    if (HostOsInfo::isWindowsHost()) {
        msvcProbe(settings, profiles);
        mingwProbe(settings, profiles);
    } else if (HostOsInfo::isOsxHost()) {
        osxProbe(settings, profiles);
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
