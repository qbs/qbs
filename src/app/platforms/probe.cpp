/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

#include <tools/hostosinfo.h>
#include <tools/platform.h>
#include "msvcprobe.h"

using namespace qbs;

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

static int specific_probe(const QString &settingsPath,
                          QHash<QString, Platform::Ptr> &platforms,
                          QString cc,
                          bool printComfortingMessage = false)
{
    QTextStream qstdout(stdout);

    QString toolchainType;
    if(cc.contains("clang"))
        toolchainType = "clang";
    else if (cc.contains("gcc"))
        toolchainType = "gcc";

    QString path       = QString::fromLocal8Bit(qgetenv("PATH"));
    QString cxx        = QString::fromLocal8Bit(qgetenv("CXX"));
    QString ld         = QString::fromLocal8Bit(qgetenv("LD"));
    QString cflags     = QString::fromLocal8Bit(qgetenv("CFLAGS"));
    QString cxxflags   = QString::fromLocal8Bit(qgetenv("CXXFLAGS"));
    QString ldflags    = QString::fromLocal8Bit(qgetenv("LDFLAGS"));
    QString cross      = QString::fromLocal8Bit(qgetenv("CROSS_COMPILE"));
    QString arch       = QString::fromLocal8Bit(qgetenv("ARCH"));

    QString pathToGcc;
    QString architecture;
    QString endianness;

    QString name;
    QString sysroot;

    QString uname = qsystem("uname", QStringList() << "-m").simplified();

    if (arch.isEmpty())
        arch = uname;

    // HACK: "uname -m" reports "i386" but "gcc -dumpmachine" reports "i686" on MacOS.
    if (HostOsInfo::isMacHost() && arch == "i386")
        arch = "i686";

    if (ld.isEmpty())
        ld = "ld";
    if (cxx.isEmpty()) {
        if (toolchainType == "gcc")
            cxx = "g++";
        else if(toolchainType == "clang")
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
        fprintf(stderr, "Cannot find %s.", qPrintable(cc));
        if (printComfortingMessage)
            fprintf(stderr, " But that's not a problem. I've already found other platforms.\n");
        else
            fprintf(stderr, "\n");
        return 1;
    }

    Platform::Ptr s;
    foreach (Platform::Ptr p, platforms.values()) {
        QString path = p->settings.value(Platform::internalKey() + "/completeccpath").toString();
        if (path == pathToGcc) {
            name = p->name;
            s = p;
            name = s->name;
            break;
        }
    }

    QString compilerTriplet = qsystem(pathToGcc, QStringList() << "-dumpmachine").simplified();
    QStringList compilerTripletl = compilerTriplet.split('-');
    if (compilerTripletl.count() < 2 ||
            !(compilerTripletl.at(0).contains(QRegExp(".86")) ||
              compilerTripletl.at(0).contains("arm") )
            ) {
        qDebug("detected %s , but i don't understand it's architecture: %s",
                qPrintable(pathToGcc), qPrintable(compilerTriplet));
                return 12;
    }

    architecture = compilerTripletl.at(0);
    if (architecture.contains("arm")) {
        endianness = "big-endian";
    } else {
        endianness = "little-endian";
    }

    QStringList pathToGccL = pathToGcc.split('/');
    QString compilerName = pathToGccL.takeLast().replace(cc, cxx);

    if (cflags.contains("--sysroot")) {
        QStringList flagl = cflags.split(' ');

        bool nextitis = false;
        foreach (const QString &flag, flagl) {
            if (nextitis) {
                sysroot = flag;
                break;
            }
            if (flag == "--sysroot") {
                nextitis = true;
            }
        }
    }

    qstdout << "==> " << (s?"reconfiguring " + name :"detected")
            << " " << pathToGcc << "\n"
            << "     triplet:  " << compilerTriplet << "\n"
            << "     arch:     " << architecture << "\n"
            << "     bin:      " << pathToGccL.join("/") << "\n"
            << "     cc:       " << cc << "\n"
            ;

    if (!cxx.isEmpty())
       qstdout << "     cxx:      " << cxx << "\n";
    if (!ld.isEmpty())
       qstdout << "     ld:       " << ld << "\n";

    if (!sysroot.isEmpty())
        qstdout << "     sysroot:  " << sysroot << "\n";
    if (!cflags.isEmpty())
            qstdout << "     CFLAGS:   " << cflags << "\n";
    if (!cxxflags.isEmpty())
            qstdout << "     CXXFLAGS: " << cxxflags << "\n";
    if (!ldflags.isEmpty())
            qstdout << "     CXXFLAGS: " << ldflags << "\n";

    qstdout  << flush;

    if (!s) {
        if (name.isEmpty())
            name = toolchainType;
       s = Platform::Ptr(new Platform(name, settingsPath + "/" + name));
    }

    // fixme should be cpp.toolchain
    // also there is no toolchain:clang
    s->settings.setValue("toolchain", "gcc");
    s->settings.setValue(Platform::internalKey() + "/completeccpath", pathToGcc);
    s->settings.setValue(Platform::internalKey() + "/target-triplet", compilerTriplet);
    s->settings.setValue("architecture", architecture);
    s->settings.setValue("endianness", endianness);

    if (HostOsInfo::isMacHost())
        s->settings.setValue("targetOS", "mac");
    else if (HostOsInfo::isLinuxHost())
        s->settings.setValue("targetOS", "linux");
    else
        s->settings.setValue("targetOS", "unknown"); //fixme

    if (compilerName.contains('-')) {
        QStringList nl = compilerName.split('-');
        s->settings.setValue("cpp/compilerName", nl.takeLast());
        s->settings.setValue("cpp/toolchainPrefix", nl.join("-") + '-');
    } else {
        s->settings.setValue("cpp/compilerName", compilerName);
    }
    s->settings.setValue("cpp/toolchainInstallPath", pathToGccL.join("/"));

    if (!cross.isEmpty())
        s->settings.setValue("environment/CROSS_COMPILE", cross);
    if (!cflags.isEmpty())
        s->settings.setValue("environment/CFLAGS", cflags);
    if (!cxxflags.isEmpty())
        s->settings.setValue("environment/CXXFLAGS", cxxflags);
    if (!ldflags.isEmpty())
        s->settings.setValue("environment/LDFLAGS", ldflags);

    platforms.insert(s->name, s);
    return 0;
}

static void mingwProbe(const QString &settingsPath, QHash<QString, Platform::Ptr> &platforms)
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
        fprintf(stderr, "Could not start \"gcc -dumpmachine\".\n");
        return;
    }
    process.waitForFinished(-1);
    QByteArray gccMachineName = process.readAll().trimmed();
    if (gccMachineName != "mingw32" && gccMachineName != "mingw64") {
        fprintf(stderr, "Detected gcc platform '%s' is not supported.\n", gccMachineName.data());
        return;
    }

    Platform::Ptr platform = platforms.value(gccMachineName);
    printf("Platform '%s' detected in '%s'.", gccMachineName.data(), qPrintable(QDir::toNativeSeparators(mingwPath)));
    if (!platform) {
       platform = Platform::Ptr(new Platform(gccMachineName, settingsPath + "/" + gccMachineName));
       platforms.insert(platform->name, platform);
    }
    platform->settings.setValue("targetOS", "windows");
    platform->settings.setValue("cpp/toolchainInstallPath", QDir::toNativeSeparators(mingwBinPath));
    platform->settings.setValue("toolchain", "mingw");
}

int probe(const QString &settingsPath, QHash<QString, Platform::Ptr> &platforms)
{
    if (HostOsInfo::isWindowsHost()) {
        msvcProbe(settingsPath, platforms);
        mingwProbe(settingsPath, platforms);
    } else {
        bool somethingFound = false;
        if (specific_probe(settingsPath, platforms, "gcc") == 0)
            somethingFound = true;
        specific_probe(settingsPath, platforms, "clang", somethingFound);
    }
    if (platforms.isEmpty())
        fprintf(stderr, "Could not detect any platforms.\n");
    return 0;
}
