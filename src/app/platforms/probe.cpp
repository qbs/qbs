/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/
#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QSettings>

#include <tools/platform.h>

using namespace qbs;

static QString searchPath(const QString &path, const QString &me)
{
    //TODO: use native seperator
    foreach (const QString &ppath, path.split(":")) {
        if (QFileInfo(ppath +  "/"  +  me).exists()) {
            return QDir::cleanPath(ppath +  "/"  +  me);
        }
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
        QHash<QString, Platform*> &platforms,
        int (* ask)(const QString &msg, const QString &choices),
        QString ( *prompt)(const QString &msg),
        QString cc,
                          bool printComfortingMessage = false
        )
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

    bool isACrossCompiler = false;

    QString uname = qsystem("uname", QStringList() << "-m").simplified();

    if (arch.isEmpty())
        arch = uname;

#ifdef Q_OS_MAC
    // HACK: "uname -m" reports "i386" but "gcc -dumpmachine" reports "i686" on MacOS.
    if (arch == "i386")
        arch = "i686";
#endif

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

    Platform *s = 0;
    foreach (Platform *p, platforms.values()) {
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

    if (architecture != arch) {
        isACrossCompiler = true;
    }
    if (!cross.isEmpty()) {
        isACrossCompiler = true;
    }

    if (isACrossCompiler && !cross.contains(compilerTriplet)) {
        if (s) {
            compilerTriplet = s->settings.value(Platform::internalKey() + "/target-triplet").toString();
        } else {
            qDebug("==> detected %s (%s), but it doesn't seem to fit your cross target (%s)",
                    qPrintable(pathToGcc),
                    qPrintable(compilerTriplet),
                    qPrintable(cross)
                  );
            if (ask("    assume this compiler produces "+cross+" and carry on?", "ny") == 0)
                return 2;

            compilerTriplet = cross;
        }
        compilerTripletl = compilerTriplet.split('-');
        architecture = compilerTripletl.at(0);
    }

    if (
            (isACrossCompiler) &&
            (!cflags.contains("--sysroot"))
       ) {
        qDebug("==> detected cross compiler %s (%s), but CFLAGS don't contain a --syroot option "
             "\n    If you did not want to cross-compiler, or you are in a chroot that is not arch %s,"
             "\n    try overriding uname -m by setting ARCH=%s in the environment and/or unset CROSS_COMPILE"
             "\n    ",
                qPrintable(pathToGcc), qPrintable(compilerTriplet), qPrintable(arch), qPrintable(architecture));
        return 3;
    } else {
    }

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
            << " " << (isACrossCompiler ? "cross compiler" :  "native compiler")
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
        while (name.isEmpty())
            name =  prompt("give a name to this platform:");
       s =  new Platform(name, settingsPath + "/" + name);
    }

    // fixme should be cpp.toolchain
    // also there is no toolchain:clang
    s->settings.setValue("toolchain", "gcc");
    s->settings.setValue(Platform::internalKey() + "/completeccpath", pathToGcc);
    s->settings.setValue(Platform::internalKey() + "/target-triplet", compilerTriplet);
    s->settings.setValue("architecture", architecture);
    s->settings.setValue("endianness", endianness);

#if defined(Q_OS_MAC)
    s->settings.setValue("targetOS", "mac");
#elif defined(Q_OS_LINUX)
    s->settings.setValue("targetOS", "linux");
#else
    s->settings.setValue("targetOS", "unknown"); //fixme
#endif

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

    s->settings.sync();
    return 0;
}

#ifdef Q_OS_WIN
static void msvcProbe(const QString &settingsPath, QHash<QString, Platform*> &platforms)
{
    QTextStream qstdout(stdout);

    QString vcInstallDir = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("VCINSTALLDIR")));
    if (vcInstallDir.endsWith('/'))
        vcInstallDir.chop(1);
    if (vcInstallDir.isEmpty())
        return;
    QString winSDKPath = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("WindowsSdkDir")));
    if (winSDKPath.endsWith('/'))
        winSDKPath.chop(1);
    QString clOutput = qsystem(vcInstallDir + "/bin/cl.exe");
    if (clOutput.isEmpty())
        return;

    QRegExp rex("C/C\\+\\+ Optimizing Compiler Version ((\\d|\\.)+) for ((x|\\d)+)");
    int idx = rex.indexIn(clOutput);
    if (idx < 0)
        return;

    QStringList clVersion = rex.cap(1).split(".");
    if (clVersion.isEmpty())
        return;
    QString clArch = rex.cap(3);
    QString msvcVersion = "msvc";
    switch (clVersion.first().toInt()) {
    case 14:
        msvcVersion += "2005";
        break;
    case 15:
        msvcVersion += "2008";
        break;
    case 16:
        msvcVersion += "2010";
        break;
    default:
        return;
    }

    QString vsInstallDir = vcInstallDir;
    idx = vsInstallDir.lastIndexOf("/");
    if (idx < 0)
        return;
    vsInstallDir.truncate(idx);

    Platform *platform = platforms.value(msvcVersion);
    if (!platform) {
       platform = new Platform(msvcVersion, settingsPath + "/" + msvcVersion);
       platforms.insert(platform->name, platform);
    }
    platform->settings.setValue("targetOS", "windows");
    platform->settings.setValue("cpp/toolchainInstallPath", vsInstallDir);
    platform->settings.setValue("toolchain", "msvc");
    platform->settings.setValue("cpp/windowsSDKPath", winSDKPath);
    qstdout << "Detected platform " << msvcVersion << " for " << clArch << ".\n";
    qstdout << "When building projects, the architecture can be chosen by passing\narchitecture:x86 or architecture:x86_64 to qbs.\n";
    platform->settings.sync();
}

static void mingwProbe(const QString &settingsPath, QHash<QString, Platform*> &platforms)
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

    Platform *platform = platforms.value(gccMachineName);
    printf("Platform '%s' detected in '%s'.", gccMachineName.data(), qPrintable(QDir::toNativeSeparators(mingwPath)));
    if (!platform) {
       platform = new Platform(gccMachineName, settingsPath + "/" + gccMachineName);
       platforms.insert(platform->name, platform);
    }
    platform->settings.setValue("targetOS", "windows");
    platform->settings.setValue("cpp/toolchainInstallPath", QDir::toNativeSeparators(mingwBinPath));
    platform->settings.setValue("toolchain", "mingw");
    platform->settings.sync();
}
#endif

int probe (const QString &settingsPath,
        QHash<QString, Platform*> &platforms,
        int (* ask)(const QString &msg, const QString &choices),
        QString ( *prompt)(const QString &msg)
        )
{
#ifdef Q_OS_WIN
    Q_UNUSED(prompt);
    Q_UNUSED(ask);
    msvcProbe(settingsPath, platforms);
    mingwProbe(settingsPath, platforms);
#else
    QString cc = QString::fromLocal8Bit(qgetenv("CC"));
    if (cc.isEmpty()) {
        bool somethingFound = false;
        if (specific_probe(settingsPath, platforms, ask, prompt, "gcc") == 0)
            somethingFound = true;
        specific_probe(settingsPath, platforms, ask, prompt, "clang", somethingFound);
    } else {
        specific_probe(settingsPath, platforms, ask, prompt, cc);
    }
#endif
    if (platforms.isEmpty())
        fprintf(stderr, "Could not detect any platforms.\n");
    return 0;
}
