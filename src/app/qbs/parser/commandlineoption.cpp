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
#include "commandlineoption.h"

#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/installoptions.h>

namespace qbs {
using namespace Internal;

CommandLineOption::~CommandLineOption()
{
}

void CommandLineOption::parse(CommandType command, const QString &representation, QStringList &input)
{
    m_command = command;
    doParse(representation, input);
}

CommandLineOption::CommandLineOption()
    : m_command(static_cast<CommandType>(-1))
{
}

QString CommandLineOption::getArgument(const QString &representation, QStringList &input)
{
    if (input.isEmpty()) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1': Missing argument.\nUsage: %2")
                    .arg(representation, description(command())));
    }
    return input.takeFirst();
}

QString FileOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2 <file>\n"
            "\tUse <file> as the project file.\n"
            "\tIf <file> is a directory and it contains a single file ending in '.qbs',\n"
            "\tthat file will be used.\n"
            "\tIf this option is not given at all, behavior is the same as for '-f .'.\n")
            .arg(longRepresentation(), shortRepresentation());
}

QString FileOption::shortRepresentation() const
{
    return QLatin1String("-f");
}

QString FileOption::longRepresentation() const
{
    return QLatin1String("--file");
}

void FileOption::doParse(const QString &representation, QStringList &input)
{
    m_projectFilePath = getArgument(representation, input);
}

QString BuildDirectoryOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2 <directory>\n"
            "\tBuild in the given directory. The default value is the current directory\n"
            "\tunless preferences.defaultBuildDirectory is set.\n"
            "\tRelative paths will be interpreted relative to the current directory.\n"
            "\tIf the directory does not exist, it will be created. Use the following\n"
            "\tspecial values as placeholders:\n"
            "\t%3: name of the project file excluding the extension\n"
            "\t%4: directory containing the project file\n")
            .arg(longRepresentation(), shortRepresentation(),
                 magicProjectString(), magicProjectDirString());
}

QString BuildDirectoryOption::shortRepresentation() const
{
    return QLatin1String("-d");
}

QString BuildDirectoryOption::longRepresentation() const
{
    return QLatin1String("--build-directory");
}

QString BuildDirectoryOption::magicProjectString()
{
    return QLatin1String("@project");
}

QString BuildDirectoryOption::magicProjectDirString()
{
    return QLatin1String("@path");
}

void BuildDirectoryOption::doParse(const QString &representation, QStringList &input)
{
    m_projectBuildDirectory = getArgument(representation, input);
}

QString GeneratorOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2 <generator>\n"
                  "\tUse the given build system generator.\n")
            .arg(longRepresentation(), shortRepresentation());
}

QString GeneratorOption::shortRepresentation() const
{
    return QLatin1String("-g");
}

QString GeneratorOption::longRepresentation() const
{
    return QLatin1String("--generator");
}

void GeneratorOption::doParse(const QString &representation, QStringList &input)
{
    m_generatorName = getArgument(representation, input);
    if (m_generatorName.isEmpty()) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1': No generator given.\nUsage: %2")
                    .arg(representation, description(command())));
    }
}

static QString loglevelLongRepresentation() { return QLatin1String("--log-level"); }

QString VerboseOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2\n"
            "\tBe more verbose. Increases the log level by one.\n"
            "\tThis option can be given more than once.\n"
            "\tExcessive occurrences have no effect.\n"
            "\tIf option '%3' appears anywhere on the command line in addition\n"
            "\tto this option, its value is taken as the base which to increase.\n")
            .arg(longRepresentation(), shortRepresentation(), loglevelLongRepresentation());
}

QString VerboseOption::shortRepresentation() const
{
    return QLatin1String("-v");
}

QString VerboseOption::longRepresentation() const
{
    return QLatin1String("--more-verbose");
}

QString QuietOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2\n"
            "\tBe more quiet. Decreases the log level by one.\n"
            "\tThis option can be given more than once.\n"
            "\tExcessive occurrences have no effect.\n"
            "\tIf option '%3' appears anywhere on the command line in addition\n"
            "\tto this option, its value is taken as the base which to decrease.\n")
            .arg(longRepresentation(), shortRepresentation(), loglevelLongRepresentation());
}

QString QuietOption::shortRepresentation() const
{
    return QLatin1String("-q");
}

QString QuietOption::longRepresentation() const
{
    return QLatin1String("--less-verbose");
}

QString JobsOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2 <n>\n"
            "\tUse <n> concurrent build jobs. <n> must be an integer greater than zero.\n"
            "\tThe default is the number of cores.\n")
            .arg(longRepresentation(), shortRepresentation());
}

QString JobsOption::shortRepresentation() const
{
    return QLatin1String("-j");
}

QString JobsOption::longRepresentation() const
{
    return QLatin1String("--jobs");
}

void JobsOption::doParse(const QString &representation, QStringList &input)
{
    const QString jobCountString = getArgument(representation, input);
    bool stringOk;
    m_jobCount = jobCountString.toInt(&stringOk);
    if (!stringOk || m_jobCount <= 0)
        throw ErrorInfo(Tr::tr("Invalid use of option '%1': Illegal job count '%2'.\nUsage: %3")
                    .arg(representation, jobCountString, description(command())));
}

QString KeepGoingOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2\n"
            "\tKeep going when errors occur (if at all possible).\n")
            .arg(longRepresentation(), shortRepresentation());
}

QString KeepGoingOption::shortRepresentation() const
{
    return QLatin1String("-k");
}

QString KeepGoingOption::longRepresentation() const
{
    return QLatin1String("--keep-going");
}

QString DryRunOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2\n"
            "\tDry run. No commands will be executed and no permanent changes to the\n"
            "\tbuild graph will be done.\n")
            .arg(longRepresentation(), shortRepresentation());
}

QString DryRunOption::shortRepresentation() const
{
    return QLatin1String("-n");
}

QString DryRunOption::longRepresentation() const
{
    return QLatin1String("--dry-run");
}

QString ForceProbesOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n"
            "\tForce re-execution of all Probe items' configure scripts, rather than using the\n"
            "\tcached data.\n")
            .arg(longRepresentation());
}

QString ForceProbesOption::longRepresentation() const
{
    return QLatin1String("--force-probe-execution");
}

QString NoInstallOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n"
            "\tDo not install any artifacts as part of the build process.\n")
            .arg(longRepresentation());
}

QString NoInstallOption::longRepresentation() const
{
    return QLatin1String("--no-install");
}


static QString logTimeRepresentation()
{
    return QLatin1String("--log-time");
}

QString ShowProgressOption::description(CommandType command) const
{
    Q_UNUSED(command);
    QString desc = Tr::tr("%1\n"
            "\tShow a progress bar. Implies '%2=%3'.\n").arg(longRepresentation(),
            loglevelLongRepresentation(), logLevelName(LoggerMinLevel));
    return desc += Tr::tr("\tThis option is mutually exclusive with '%1'.\n")
            .arg(logTimeRepresentation());
}

static QString showProgressRepresentation()
{
    return QLatin1String("--show-progress");
}

QString ShowProgressOption::longRepresentation() const
{
    return showProgressRepresentation();
}

void StringListOption::doParse(const QString &representation, QStringList &input)
{
    m_arguments = getArgument(representation, input).split(QLatin1Char(','));
    if (m_arguments.isEmpty()) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1': Argument list must not be empty.\n"
                           "Usage: %2").arg(representation, description(command())));
    }
    foreach (const QString &element, m_arguments) {
        if (element.isEmpty()) {
            throw ErrorInfo(Tr::tr("Invalid use of option '%1': Argument list must not contain "
                               "empty elements.\nUsage: %2")
                        .arg(representation, description(command())));
        }
    }
}

QString ChangedFilesOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1 <file>[,<file>...]\n"
                  "\tAssume these and only these files have changed.\n").arg(longRepresentation());
}

QString ChangedFilesOption::longRepresentation() const
{
    return QLatin1String("--changed-files");
}

QString ProductsOption::description(CommandType command) const
{
    const QString prefix = Tr::tr("%1|%2").arg(longRepresentation(), shortRepresentation());
    switch (command) {
    case InstallCommandType:
    case RunCommandType:
    case ShellCommandType:
        return Tr::tr("%1 <name>\n\tUse the specified product.\n").arg(prefix);
    default:
        return Tr::tr("%1 <name>[,<name>...]\n"
                      "\tTake only the specified products into account.\n").arg(prefix);
    }
}

QString ProductsOption::shortRepresentation() const
{
    return QLatin1String("-p");
}

QString ProductsOption::longRepresentation() const
{
    return QLatin1String("--products");
}

static QStringList allLogLevelStrings()
{
    QStringList result;
    for (int i = static_cast<int>(LoggerMinLevel); i <= static_cast<int>(LoggerMaxLevel); ++i)
        result << logLevelName(static_cast<LoggerLevel>(i));
    return result;
}

LogLevelOption::LogLevelOption() : m_logLevel(defaultLogLevel())
{
}

QString LogLevelOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1 <level>\n"
            "\tUse the specified log level.\n"
            "\tPossible values are '%2'.\n"
            "\tThe default is '%3'.\n").arg(longRepresentation(),
            allLogLevelStrings().join(QLatin1String("', '")), logLevelName(defaultLogLevel()));
}

QString LogLevelOption::longRepresentation() const
{
    return loglevelLongRepresentation();
}

void LogLevelOption::doParse(const QString &representation, QStringList &input)
{
    const QString levelString = getArgument(representation, input);
    const QList<LoggerLevel> levels = QList<LoggerLevel>() << LoggerError << LoggerWarning
            << LoggerInfo << LoggerDebug << LoggerTrace;
    foreach (LoggerLevel l, levels) {
        if (logLevelName(l) == levelString) {
            m_logLevel = l;
            return;
        }
    }
    throw ErrorInfo(Tr::tr("Invalid use of option '%1': Unknown log level '%2'.\nUsage: %3")
                .arg(representation, levelString, description(command())));
}

QString ForceOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tDisregard objections.\n"
                  "\tqbs might refuse to execute a given command because certain\n"
                  "\tcircumstances make it seem dubious. This option switches the\n"
                  "\trespective checks off.\n").arg(longRepresentation());
}

QString ForceOption::longRepresentation() const
{
    return QLatin1String("--force");
}

QString ForceTimeStampCheckOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tForce timestamp checks.\n"
                  "\tInstead of using the file timestamps that are stored in the build graph,\n"
                  "\tretrieve the timestamps from the file system.\n").arg(longRepresentation());
}

QString ForceTimeStampCheckOption::longRepresentation() const
{
    return QLatin1String("--check-timestamps");
}

QString ForceOutputCheckOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tForce transformer output artifact checks.\n"
                  "\tVerify that the output artifacts declared by rules in the\n"
                  "\tproject are actually created.\n").arg(longRepresentation());
}

QString ForceOutputCheckOption::longRepresentation() const
{
    return QLatin1String("--check-outputs");
}

QString BuildNonDefaultOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tBuild all products, even if their builtByDefault property is false.\n")
            .arg(longRepresentation());
}

QString BuildNonDefaultOption::longRepresentation() const
{
    return QLatin1String("--all-products");
}

QString VersionOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n"
            "\tDisplay the qbs version and exit.\n").arg(longRepresentation());
}

QString VersionOption::shortRepresentation() const
{
    return QString();
}

QString VersionOption::longRepresentation() const
{
    return QStringLiteral("--version");
}


InstallRootOption::InstallRootOption() : m_useSysroot(false)
{
}

static QString magicSysrootString() { return QLatin1String("@sysroot"); }

QString InstallRootOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1 <directory>\n"
                  "\tInstall into the given directory.\n"
                  "\tThe default value is '<build dir>/%2'.\n"
                  "\tIf the directory does not exist, it will be created. Use the special\n"
                  "\tvalue '%3' to install into the sysroot (i.e. the value of the\n"
                  "\tproperty qbs.sysroot).\n")
            .arg(longRepresentation(), InstallOptions::defaultInstallRoot(), magicSysrootString());
}

QString InstallRootOption::longRepresentation() const
{
    return QLatin1String("--install-root");
}

void InstallRootOption::doParse(const QString &representation, QStringList &input)
{
    if (input.isEmpty()) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1: Argument expected.\n"
                           "Usage: %2").arg(representation, description(command())));
    }
    const QString installRoot = input.takeFirst();
    if (installRoot == magicSysrootString())
        m_useSysroot = true;
    else
        m_installRoot = installRoot;
}

QString RemoveFirstOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tRemove the installation base directory before installing.\n")
            .arg(longRepresentation());
}

QString RemoveFirstOption::longRepresentation() const
{
    return QLatin1String("--clean-install-root");
}


QString NoBuildOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tDo not build before installing.\n")
            .arg(longRepresentation());
}

QString NoBuildOption::longRepresentation() const
{
    return QLatin1String("--no-build");
}


QString LogTimeOption::description(CommandType command) const
{
    Q_UNUSED(command);
    QString desc = Tr::tr("%1\n\tLog the time that the operations involved in this command take.\n")
            .arg(longRepresentation());
    desc += Tr::tr("\tThis option is implied in log levels '%1' and higher.\n")
            .arg(logLevelName(LoggerDebug));
    return desc += Tr::tr("\tThis option is mutually exclusive with '%1'.\n")
            .arg(showProgressRepresentation());
}

QString LogTimeOption::shortRepresentation() const
{
    return QLatin1String("-t");
}

QString LogTimeOption::longRepresentation() const
{
    return logTimeRepresentation();
}


SettingsDirOption::SettingsDirOption()
{
}

QString SettingsDirOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1 <directory>\n"
                  "\tRead all settings (such as profile information) from the given directory.\n"
                  "\tThe default value is system-specific (see the QSettings documentation).\n"
                  "\tIf the directory does not exist, it will be created.\n")
            .arg(longRepresentation());
}

QString SettingsDirOption::longRepresentation() const
{
    return QLatin1String("--settings-dir");
}

void SettingsDirOption::doParse(const QString &representation, QStringList &input)
{
    if (input.isEmpty()) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1: Argument expected.\n"
                           "Usage: %2").arg(representation, description(command())));
    }
    m_settingsDir = input.takeFirst();
}

CommandEchoModeOption::CommandEchoModeOption()
{
}

QString CommandEchoModeOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1 <mode>\n"
                  "\tKind of output to show when executing commands.\n"
                  "\tPossible values are '%2'.\n"
                  "\tThe default is '%3'.\n")
            .arg(longRepresentation(), allCommandEchoModeStrings().join(QLatin1String("', '")),
                 commandEchoModeName(defaultCommandEchoMode()));
}

QString CommandEchoModeOption::longRepresentation() const
{
    return QLatin1String("--command-echo-mode");
}

CommandEchoMode CommandEchoModeOption::commandEchoMode() const
{
    return m_echoMode;
}

void CommandEchoModeOption::doParse(const QString &representation, QStringList &input)
{
    const QString mode = getArgument(representation, input);
    if (mode.isEmpty()) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1': No command echo mode given.\nUsage: %2")
                    .arg(representation, description(command())));
    }

    if (!allCommandEchoModeStrings().contains(mode)) {
        throw ErrorInfo(Tr::tr("Invalid use of option '%1': "
                               "Invalid command echo mode '%2' given.\nUsage: %3")
                        .arg(representation, mode, description(command())));
    }

    m_echoMode = commandEchoModeFromName(mode);
}

QString WaitLockOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tWait indefinitely for other processes to release the build graph lock.\n")
            .arg(longRepresentation());
}

QString WaitLockOption::longRepresentation() const
{
    return QLatin1String("--wait-lock");
}

} // namespace qbs
