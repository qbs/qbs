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
#include "specialplatformssetup.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>

#include <cstdio>

namespace qbs {

SpecialPlatformsSetup::SpecialPlatformsSetup() : m_stdout(stdout), m_helpRequested(false) {}
SpecialPlatformsSetup::~SpecialPlatformsSetup() {}

void SpecialPlatformsSetup::setup()
{
    parseCommandLine();
    if (helpRequested())
        return;
    setupBaseDir();

    const QString qbsPath = QCoreApplication::applicationDirPath() + QLatin1String("/qbs");
    const QString commandLine = qbsPath + QLatin1String(" platforms print-config-base-dir");
    const QString configBaseDir = QString::fromLocal8Bit(runProcess(commandLine,
        QProcessEnvironment::systemEnvironment())).trimmed();

    foreach (const PlatformInfo &pi, gatherPlatformInfo()) {
        writeConfigFile(pi, configBaseDir);
        const QString registerCommandLine = QString::fromLocal8Bit("%1 config "
                "--global profiles.%2.qbs.platform %2").arg(qbsPath, pi.name);
        runProcess(registerCommandLine, QProcessEnvironment::systemEnvironment());
    }
}

QString SpecialPlatformsSetup::helpString() const
{
    return tr("This tool sets up qbs platform definitions for all installed "
        "targets in a given %1 installation.\n").arg(platformTypeName()) + usageString();
}

QString SpecialPlatformsSetup::usageString() const
{
    QString s = tr("Usage: ") + QCoreApplication::applicationFilePath() + ' '
        + tr("[-h|<base directory>]");
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

void SpecialPlatformsSetup::writeConfigFile(const PlatformInfo &platformInfo,
    const QString &configBaseDir)
{
    m_stdout << tr("Setting up platform '%1'...").arg(platformInfo.name) << endl;

    const QString configDir = configBaseDir + QLatin1Char('/') + platformInfo.name;

    /*
     * A more correct solution would be to recursively remove and then recreate the directory.
     * However, since it only contains one file, that currently seems like overkill.
     */
    if (!QDir::root().mkpath(configDir)) {
        throw Exception(tr("Directory '%1' could not be created.")
            .arg(QDir::toNativeSeparators(configDir)));
    }
    const QString configFilePath = configDir + QLatin1String("/setup.ini");
    if (QFileInfo(configFilePath).exists()) {
        QFile configFile(configFilePath);
        if (!configFile.remove()) {
            throw Exception(tr("Failed to remove old config file '%1': %2.")
                .arg(QDir::toNativeSeparators(configFilePath), configFile.errorString()));
        }
    }

    QSettings settings(configFilePath, QSettings::IniFormat);

    settings.setValue(QLatin1String("toolchain"), QLatin1String("gcc"));
    settings.setValue(QLatin1String("endianness"), QLatin1String("little-endian"));
    settings.setValue(QLatin1String("targetOS"), QLatin1String("linux"));
    settings.setValue(QLatin1String("sysroot"), platformInfo.sysrootDir);

    settings.beginGroup(QLatin1String("cpp"));
    settings.setValue(QLatin1String("toolchainInstallPath"), platformInfo.toolchainDir);
    settings.setValue(QLatin1String("compilerName"), platformInfo.compilerName);
    settings.setValue(QLatin1String("cFlags"), platformInfo.cFlags);
    settings.setValue(QLatin1String("cxxFlags"), platformInfo.cxxFlags);
    settings.setValue(QLatin1String("linkerFlags"), platformInfo.ldFlags);
    settings.endGroup();

    settings.beginGroup(QLatin1String("qt/core"));
    settings.setValue(QLatin1String("binPath"), platformInfo.qtBinDir);
    settings.setValue(QLatin1String("libPath"),
        platformInfo.sysrootDir + QLatin1String("/usr/lib"));
    settings.setValue(QLatin1String("incPath"), platformInfo.qtIncDir);
    settings.setValue(QLatin1String("mkspecsPath"), platformInfo.qtMkspecsDir);
    settings.setValue(QLatin1String("namespace"), QString());
    settings.setValue(QLatin1String("libInfix"), QString());
    settings.endGroup();

    settings.beginGroup(QLatin1String("environment"));
    for (QHash<QString, QString>::ConstIterator it = platformInfo.environment.constBegin();
         it != platformInfo.environment.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        throw Exception(tr("Failed to write platform config file '%1'.")
            .arg(QDir::toNativeSeparators(configFilePath)));
    }
}

} // namespace qbs
