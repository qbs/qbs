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
#include "specialplatformssetup.h"

#include <tools/profile.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include <cstdio>

namespace qbs {

SpecialPlatformsSetup::SpecialPlatformsSetup(Settings *settings)
    : m_stdout(stdout), m_helpRequested(false), m_settings(settings) {}
SpecialPlatformsSetup::~SpecialPlatformsSetup() {}

void SpecialPlatformsSetup::setup()
{
    parseCommandLine();
    if (helpRequested())
        return;
    setupBaseDir();

    foreach (const PlatformInfo &pi, gatherPlatformInfo())
        registerProfile(pi);
}

QString SpecialPlatformsSetup::helpString() const
{
    return tr("This tool sets up a qbs profile for each installed "
        "target in a given %1 installation.\n").arg(platformTypeName()) + usageString();
}

QString SpecialPlatformsSetup::usageString() const
{
    QString s = tr("Usage: %1 [-h|<base directory>]")
            .arg(QFileInfo(QCoreApplication::applicationFilePath()).fileName());
    const QString &defaultDir = defaultBaseDirectory();
    if (!defaultDir.isEmpty() == 1)
        s += QLatin1Char('\n') + tr("The default base directory is '%1'.").arg(defaultDir);
    return s;
}

QString SpecialPlatformsSetup::tr(const char *str)
{
    return QCoreApplication::translate("SpecialPlatformSetup", str);
}

void SpecialPlatformsSetup::parseCommandLine()
{
    QStringList args = qApp->arguments();
    args.removeFirst();
    while (!args.isEmpty()) {
        const QString arg = args.takeFirst();
        if (arg == QLatin1String("-h") || arg == QLatin1String("-help")
                || arg == QLatin1String("--help")) {
            m_helpRequested = true;
            return;
        }
        if (arg.startsWith(QLatin1Char('-')))
            throw Exception(tr("Unknown option '%1'\n%2.").arg(arg).arg(usageString()));
        m_baseDir = arg;
    }
}

void SpecialPlatformsSetup::setupBaseDir()
{
    if (m_baseDir.isEmpty()) {
        const QString &defaultDir = defaultBaseDirectory();
        if (!defaultDir.isEmpty() && QFileInfo(defaultDir).exists())
            m_baseDir = defaultDir;
    }
    if (m_baseDir.isEmpty())
        throw Exception(tr("No base directory given and none auto-detected."));
}

QByteArray SpecialPlatformsSetup::runProcess(const QString &commandLine,
    const QProcessEnvironment &env)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setProcessEnvironment(env);
    proc.start(commandLine);
    if (!proc.waitForStarted())
        handleProcessError(commandLine, proc.errorString(), QByteArray());
    if (proc.state() == QProcess::Running && !proc.waitForFinished(5000)) {
        if (proc.error() == QProcess::UnknownError)
            handleProcessError(commandLine, tr("Process hangs"), QByteArray());
        handleProcessError(commandLine, proc.errorString(), QByteArray());
    }
    const QByteArray output = proc.readAll();
    if (proc.exitStatus() == QProcess::CrashExit)
        handleProcessError(commandLine, proc.errorString(), output);
    if (proc.exitCode() != 0) {
        handleProcessError(commandLine, tr("Unexpected exit code %1").arg(proc.exitCode()),
            output);
    }
    return output;
}

void SpecialPlatformsSetup::handleProcessError(const QString &commandLine, const QString &message,
    const QByteArray &output)
{
    QString completeMsg = tr("Command '%1' failed: %2.")
        .arg(QDir::toNativeSeparators(commandLine), message);
    if (!output.isEmpty())
        completeMsg += QLatin1Char('\n') + tr("Output was: %1").arg(QString::fromLocal8Bit(output));
    throw Exception(completeMsg);
}

void SpecialPlatformsSetup::registerProfile(const PlatformInfo &platformInfo)
{
    m_stdout << tr("Setting up profile '%1'...").arg(platformInfo.name) << endl;

    Profile profile(platformInfo.name, m_settings);
    profile.removeProfile();
    profile.setValue(QLatin1String("qbs.toolchain"), QStringList(QLatin1String("gcc")));
    profile.setValue(QLatin1String("qbs.endianness"), QLatin1String("little"));
    profile.setValue(QLatin1String("qbs.targetOS"), platformInfo.targetOS);
    profile.setValue(QLatin1String("qbs.sysroot"), platformInfo.sysrootDir);

    profile.setValue(QLatin1String("cpp.toolchainInstallPath"), platformInfo.toolchainDir);
    profile.setValue(QLatin1String("cpp.compilerName"), platformInfo.compilerName);
    profile.setValue(QLatin1String("cpp.cFlags"), platformInfo.cFlags);
    profile.setValue(QLatin1String("cpp.cxxFlags"), platformInfo.cxxFlags);
    profile.setValue(QLatin1String("cpp.linkerFlags"), platformInfo.ldFlags);

    profile.setValue(QLatin1String("Qt.core.binPath"), platformInfo.qtBinDir);
    profile.setValue(QLatin1String("Qt.core.libPath"),
                     platformInfo.sysrootDir + QLatin1String("/usr/lib"));
    profile.setValue(QLatin1String("Qt.core.incPath"), platformInfo.qtIncDir);
    profile.setValue(QLatin1String("Qt.core.mkspecPath"), platformInfo.qtMkspecPath);
    profile.setValue(QLatin1String("Qt.core.namespace"), QString());
    profile.setValue(QLatin1String("Qt.core.libInfix"), QString());
}

} // namespace qbs
