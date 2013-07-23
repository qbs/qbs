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
            "\tIf <file> is a directory and it contains a single file ending in '.qbs', "
            "that file will be used.\n"
            "\tIf this option is not given at all, the behavior is the same as for '-f .'.\n")
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

static QString loglevelLongRepresentation() { return QLatin1String("--log-level"); }

QString VerboseOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1|%2\n"
            "\tBe more verbose. Increases the log level by one.\n"
            "\tThis option can be given more than once. Excessive occurrences have no effect.\n"
            "\tIf the option '%3' appears anywhere on the command line in addition to this option,\n"
            "\tits value is taken as the base which to increase.\n")
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
            "\tThis option can be given more than once. Excessive occurrences have no effect.\n"
            "\tIf option '%3' appears anywhere on the command line in addition to this option,\n"
            "\tits value is taken as the base which to decrease.\n")
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
            "\tUse <n> concurrent build jobs. <n> must be an integer greater than zero. "
                  "The default is the number of cores.\n")
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
            "\tDry run. No commands will be executed and no permanent changes to the "
                  "build graph will be done.\n").arg(longRepresentation(), shortRepresentation());
}

QString DryRunOption::shortRepresentation() const
{
    return QLatin1String("-n");
}

QString DryRunOption::longRepresentation() const
{
    return QLatin1String("--dry-run");
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
            "\tUse the specified log level. Possible values are '%2'.\n"
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

QString AllArtifactsOption::description(CommandType command) const
{
    Q_UNUSED(command);
    Q_ASSERT(command == CleanCommandType);
    return Tr::tr("%1\n\tRemove all build artifacts, not just intermediate ones.\n")
            .arg(longRepresentation());
}

QString AllArtifactsOption::longRepresentation() const
{
    return QLatin1String("--all-artifacts");
}

QString ForceOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tDisregard objections.\n"
                  "\tqbs might refuse to execute a given command because "
                  "certain circumstances make it seem dubious. This option switches the "
                  "respective checks off.\n").arg(longRepresentation());
}

QString ForceOption::longRepresentation() const
{
    return QLatin1String("--force");
}

QString ForceTimeStampCheckOption::description(CommandType command) const
{
    Q_UNUSED(command);
    return Tr::tr("%1\n\tForce timestamp checks.\n"
                  "\tInstead of using the file timestamps that are stored in the build graph, "
                  "retrieve the timestamps from the file system.\n").arg(longRepresentation());
}

QString ForceTimeStampCheckOption::longRepresentation() const
{
    return QLatin1String("--check-timestamps");
}


InstallRootOption::InstallRootOption() : m_useSysroot(false)
{
}

static QString magicSysrootString() { return QLatin1String("@sysroot"); }

QString InstallRootOption::description(CommandType command) const
{
    Q_ASSERT(command == InstallCommandType || command == RunCommandType);
    Q_UNUSED(command);
    return Tr::tr("%1 <directory>\n"
                  "\tInstall into the given directory. The default value is '<build dir>/%2'.\n"
                  "\tIf the directory does not exist, it will be created. Use the special value "
                  "'%3' to install into the sysroot (i.e. the value of the property "
                  "qbs.sysroot).\n")
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
    Q_ASSERT(command == InstallCommandType || command == RunCommandType);
    Q_UNUSED(command);
    return Tr::tr("%1\n\tRemove the installation base directory before installing.\n")
            .arg(longRepresentation());
}

QString RemoveFirstOption::longRepresentation() const
{
    return QLatin1String("--remove-first");
}


QString NoBuildOption::description(CommandType command) const
{
    Q_ASSERT(command == InstallCommandType || command == RunCommandType);
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

} // namespace qbs
