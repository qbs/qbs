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

#include "osxprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/hostosinfo.h>

#include <QStringList>
#include <QProcess>
#include <QByteArray>
#include <QFileInfo>
#include <QDir>
#include <QSettings>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

namespace {
static QString qsystem(const QString &exe, const QStringList &args = QStringList())
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(exe, args);
    p.waitForFinished();
    return QString::fromLocal8Bit(p.readAll());
}

class OsxProbe
{
public:
    qbs::Settings *settings;
    QList<qbs::Profile> &profiles;
    QStringList developerPaths;
    OsxProbe(qbs::Settings *settings, QList<qbs::Profile> &profiles)
        : settings(settings), profiles(profiles)
    { }

    bool addDeveloperPath(const QString &path)
    {
        if (path.isEmpty())
            return false;
        QFileInfo pInfo(path);
        if (!pInfo.exists() || !pInfo.isDir())
            return false;
        if (developerPaths.contains(path))
            return false;
        developerPaths.append(path);
        qbsInfo() << Tr::tr("Added developer path %1").arg(path);
        return true;
    }

    void detectDeveloperPaths()
    {
        QProcess selectedXcode;
        QString program = "/usr/bin/xcode-select";
        QStringList arguments(QLatin1String("--print-path"));
        selectedXcode.start(program, arguments, QProcess::ReadOnly);
        if (!selectedXcode.waitForFinished() || selectedXcode.exitCode()) {
            qbsInfo() << Tr::tr("Could not detect selected xcode with /usr/bin/xcode-select");
        } else {
            QString path = QString::fromLocal8Bit(selectedXcode.readAllStandardOutput());
            addDeveloperPath(path);
        }
        addDeveloperPath(QLatin1String("/Applications/Xcode.app/Contents/Developer"));
    }

    void setArch(Profile *profile, const QString &pathToGcc, const QStringList &extraFlags)
    {
        // setting architecture and endianness only here, bercause the same compiler
        // can support several ones
        QStringList flags(extraFlags);
        flags << QLatin1String("-dumpmachine");
        QString compilerTriplet = qsystem(pathToGcc, flags).simplified();
        QStringList compilerTripletl = compilerTriplet.split('-');
        if (compilerTripletl.count() < 2) {
            qbsError() << QString::fromLocal8Bit("Detected '%1', but I don't understand "
                                                 "its architecture '%2'.")
                          .arg(pathToGcc, compilerTriplet);
            return;
        }

        QString endianness, architecture;
        architecture = compilerTripletl.at(0);
        if (architecture.contains("arm"))
            endianness = "big-endian";
        else
            endianness = "little-endian";

        qbsInfo() << Tr::tr("Toolchain %1 detected:\n"
                            "    binary: %2\n"
                            "    triplet: %3\n"
                            "    arch: %4").arg(profile->name(), pathToGcc, compilerTriplet,
                                                architecture);

        profile->setValue("qbs.endianness", endianness);
        profile->setValue("qbs.architecture", architecture);

    }

    void setupDefaultToolchains(const QString &devPath, const QString &xCodeName)
    {
        qbsInfo() << Tr::tr("Setting up profile '%1'.").arg(xCodeName);
        // default toolchain
        QString toolchainType = xCodeName + QLatin1String("_clang");
        QString pathToGcc;
        {
            QString path       = devPath
                    + QLatin1String("/Toolchains/XcodeDefault.xctoolchain/usr/bin");
            QString cc         = path + QLatin1String("/clang");
            QString cxx        = path + QLatin1String("/clang++");


            QString uname = qsystem("uname", QStringList() << "-m").simplified();
            //QSettings toolchainInfo(devPath
            //                        + QLatin1String("/Toolchains/XcodeDefault.xctoolchain/ToolchainInfo.plist")
            //                        , QSettings::NativeFormat);

            pathToGcc = cxx;

            if (!QFileInfo(pathToGcc).exists()) {
                qbsInfo() << Tr::tr("%1 not found.").arg(cc);
                return;
            }

            QStringList pathToGccL = pathToGcc.split('/');
            QString compilerName = pathToGccL.takeLast();

            Profile profile(toolchainType, settings);
            profile.removeProfile();

            // fixme should be cpp.toolchain
            // also there is no toolchain:clang
            profile.setValue("qbs.toolchain", "gcc");

            if (compilerName.contains('-')) {
                QStringList nl = compilerName.split('-');
                profile.setValue("cpp.compilerName", nl.takeLast());
                profile.setValue("cpp.toolchainPrefix", nl.join("-") + '-');
            } else {
                profile.setValue("cpp.compilerName", compilerName);
            }
            profile.setValue("cpp.toolchainInstallPath", pathToGccL.join("/"));
            qbsInfo() << Tr::tr("Adding default toolchain %1").arg(toolchainType);
            profiles << profile;
        }

        // Platforms
        QDir platformsDir(devPath + QLatin1String("/Platforms"));

        QFileInfoList platforms = platformsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (const QFileInfo &fInfo, platforms) {
            if (fInfo.isDir() && fInfo.suffix() == QLatin1String("platform")) {
                qbsInfo() << Tr::tr("Setting up %1").arg(fInfo.fileName());
                QSettings infoSettings(fInfo.absoluteFilePath() + QLatin1String("/Info.plist")
                                       , QSettings::NativeFormat);
                if (!infoSettings.contains(QLatin1String("Name"))) {
                    qbsInfo() << Tr::tr("Missing Name in Info.plist of %1")
                                 .arg(fInfo.absoluteFilePath());
                    continue;
                }
                QString name = infoSettings.value(QLatin1String("Name")).toString();
                QString fullName = xCodeName + QLatin1String("_") + name + QLatin1String("_clang");

                Profile p(fullName, settings);
                p.removeProfile();
                p.setValue("qbs.targetOS", name);
                p.setValue("qbs.baseProfile", toolchainType);
                QStringList extraFlags;
                // it seems that using group/key to access subvalues is broken, using explicit access...
                QVariantMap defaultProp = infoSettings.value(QLatin1String("DefaultProperties"))
                        .toMap();
                if (defaultProp.contains(QLatin1String("ARCHS"))) {
                    QString arch = defaultProp.value(QLatin1String("ARCHS")).toString();
                    if (arch == QLatin1String("$(NATIVE_ARCH_32_BIT)"))
                        extraFlags << QLatin1String("-arch") << QLatin1String("i386");
                }
                if (defaultProp.contains(QLatin1String("NATIVE_ARCH"))) {
                    QString arch = defaultProp.value("NATIVE_ARCH").toString();
                    if (!arch.startsWith(QLatin1String("arm")))
                        qbsInfo() << Tr::tr("Expected arm architecture, not %1").arg(arch);
                    extraFlags << QLatin1String("-arch") << arch;
                }
                if (defaultProp.contains(QLatin1String("SDKROOT"))) {
                    QString sdkName = defaultProp.value(QLatin1String("SDKROOT")).toString();
                    QString sdkPath;
                    QDir sdks(fInfo.absoluteFilePath() + QLatin1String("/Developer/SDKs"));
                    foreach (const QFileInfo &sdkDirInfo, sdks.entryInfoList(QDir::Dirs
                                                                      | QDir::NoDotAndDotDot)) {
                        QSettings sdkInfo(sdkDirInfo.absoluteFilePath()
                                          + QLatin1String("/SDKSettings.plist")
                                          , QSettings::NativeFormat);
                        if (sdkInfo.value(QLatin1String("CanonicalName")) == sdkName) {
                            sdkPath = sdkDirInfo.canonicalFilePath();
                            break;
                        }
                    }
                    if (!sdkPath.isEmpty())
                        extraFlags << "-isysroot " << sdkPath;
                    else
                        qbsInfo() << Tr::tr("Failed to find sysroot %1").arg(sdkName);
                }
                if (!extraFlags.isEmpty()) {
                    foreach (const QString &flags, QStringList()
                             << QString::fromLatin1("CFlags")
                             << QString::fromLatin1("CxxFlags")
                             << QString::fromLatin1("ObjcFlags")
                             << QString::fromLatin1("ObjcxxFlags")
                             << QString::fromLatin1("LinkerFlags"))
                    p.setValue("qbs.platform" + flags, extraFlags);
                }
                setArch(&p, pathToGcc, extraFlags);
                profiles << p;

                // add gcc
                QFileInfo specialGcc(fInfo.absoluteFilePath() + QLatin1String("/Developer/usr/bin/g++"));
                if (!specialGcc.exists())
                    specialGcc = QFileInfo(devPath + QLatin1String("/usr/bin/g++"));
                if (specialGcc.exists()) {
                    QString gccFullName = xCodeName + QLatin1Char('_') + name + QLatin1String("_gcc");
                    Profile pGcc(gccFullName, settings);
                    pGcc.removeProfile();
                    // use the arm-apple-darwin10-llvm-* variant if available???
                    pGcc.setValue("cpp.compilerName", specialGcc.baseName());
                    pGcc.setValue("cpp.toolchainInstallPath", specialGcc.canonicalPath());
                    setArch(&pGcc, specialGcc.canonicalFilePath(), extraFlags);
                    profiles << pGcc;
                }
            }
        }
    }

    void detectAll()
    {
        detectDeveloperPaths();
        for (int iXcode = 0; iXcode < developerPaths.count(); ++iXcode) {
            setupDefaultToolchains(developerPaths.value(iXcode),
                                   QString::fromLatin1("xcode%1").arg(iXcode));
        }
    }
};
} // end anonymous namespace

void osxProbe(qbs::Settings *settings, QList<qbs::Profile> &profiles)
{
    OsxProbe probe(settings, profiles);
    probe.detectAll();
}

