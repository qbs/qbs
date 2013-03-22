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

#include "commandlineparser.h"

#include "command.h"
#include "commandlineoption.h"
#include "commandlineoptionpool.h"
#include "commandpool.h"
#include "../qbstool.h"
#include "../../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/buildoptions.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qbs {
using Internal::Tr;

class CommandLineParser::CommandLineParserPrivate
{
public:
    CommandLineParserPrivate();

    void doParse();
    Command *commandFromString(const QString &commandString) const;
    QList<Command *> allCommands() const;
    QString generalHelp() const;

    void setupProjectFile();
    void setupProgress();
    void setupLogLevel();
    void setupBuildOptions();

    QString propertyName(const QString &aCommandLineName) const;

    QStringList commandLine;
    Settings *settings;
    Command *command;
    QString projectFilePath;
    BuildOptions buildOptions;
    bool showProgress;
    CommandLineOptionPool optionPool;
    CommandPool commandPool;
};

CommandLineParser::CommandLineParser() : d(0)
{
}

CommandLineParser::~CommandLineParser()
{
    delete d;
}

void CommandLineParser::printHelp() const
{
    QTextStream stream(stdout);

    Q_ASSERT(d->command == d->commandPool.getCommand(HelpCommandType));
    const HelpCommand * const helpCommand = static_cast<HelpCommand *>(d->command);
    if (helpCommand->commandToDescribe().isEmpty()) {
        stream << "qbs " QBS_VERSION "\n";
        stream << d->generalHelp();
    } else {
        const Command * const commandToDescribe
                = d->commandFromString(helpCommand->commandToDescribe());
        if (commandToDescribe) {
            stream << commandToDescribe->longDescription();
        } else if (!QbsTool::tryToRunTool(helpCommand->commandToDescribe(),
                                          QStringList(QLatin1String("--help")))) {
            throw Error(Tr::tr("No such command '%1'.\n%2")
                        .arg(helpCommand->commandToDescribe(), d->generalHelp()));
        }
    }
}

CommandType CommandLineParser::command() const
{
    return d->command->type();
}

QString CommandLineParser::projectFilePath() const
{
    return d->projectFilePath;
}

BuildOptions CommandLineParser::buildOptions() const
{
    return d->buildOptions;
}

CleanOptions CommandLineParser::cleanOptions() const
{
    Q_ASSERT(command() == CleanCommandType);
    CleanOptions options;
    options.cleanType = d->optionPool.allArtifactsOption()->enabled()
            ? CleanOptions::CleanupAll : CleanOptions::CleanupTemporaries;
    options.dryRun = buildOptions().dryRun;
    options.keepGoing = buildOptions().keepGoing;
    return options;
}

InstallOptions CommandLineParser::installOptions() const
{
    Q_ASSERT(command() == InstallCommandType || command() == RunCommandType);
    InstallOptions options;
    options.removeFirst = d->optionPool.removeFirstoption()->enabled();
    options.installRoot = d->optionPool.installRootOption()->installRoot();
    options.dryRun = buildOptions().dryRun;
    options.keepGoing = buildOptions().keepGoing;
    return options;
}

bool CommandLineParser::force() const
{
    return d->optionPool.forceOption()->enabled();
}

QStringList CommandLineParser::runArgs() const
{
    Q_ASSERT(d->command->type() == RunCommandType);
    return static_cast<RunCommand *>(d->command)->targetParameters();
}

QStringList CommandLineParser::products() const
{
    return d->optionPool.productsOption()->arguments();
}

bool CommandLineParser::showProgress() const
{
    return d->showProgress;
}

QString CommandLineParser::commandName() const
{
    return d->command->representation();
}

QString CommandLineParser::commandDescription() const
{
    return d->command->longDescription();
}

QList<QVariantMap> CommandLineParser::buildConfigurations() const
{
    // Key: build variant, value: properties. Empty key used for global properties.
    typedef QMap<QString, QVariantMap> PropertyMaps;
    PropertyMaps properties;

    const QString buildVariantKey = QLatin1String("qbs.buildVariant");
    QString currentKey;
    QVariantMap currentProperties;
    foreach (const QString &arg, d->command->additionalArguments()) {
        const int sepPos = arg.indexOf(QLatin1Char(':'));
        if (sepPos == -1) { // New build variant found.
            properties.insert(currentKey, currentProperties);
            currentKey = arg;
            currentProperties.clear();
            continue;
        }
        const QString property = d->propertyName(arg.left(sepPos));
        if (property.isEmpty())
            qbsWarning() << Tr::tr("Ignoring empty property.");
        else if (property == buildVariantKey)
            qbsWarning() << Tr::tr("Refusing to overwrite special property '%1'.").arg(buildVariantKey);
        else
            currentProperties.insert(property, arg.mid(sepPos + 1));
    }
    properties.insert(currentKey, currentProperties);

    if (properties.count() == 1) // No build variant specified on command line.
        properties.insert(QLatin1String("debug"), QVariantMap());

    const PropertyMaps::Iterator globalMapIt = properties.find(QString());
    Q_ASSERT(globalMapIt != properties.end());
    const QVariantMap globalProperties = globalMapIt.value();
    properties.erase(globalMapIt);

    QList<QVariantMap> buildConfigs;
    for (PropertyMaps::ConstIterator mapsIt = properties.constBegin();
             mapsIt != properties.constEnd(); ++mapsIt) {
        QVariantMap properties = mapsIt.value();
        for (QVariantMap::ConstIterator globalPropIt = globalProperties.constBegin();
                 globalPropIt != globalProperties.constEnd(); ++globalPropIt) {
            properties.insert(globalPropIt.key(), globalPropIt.value());
        }
        properties.insert(buildVariantKey, mapsIt.key());
        buildConfigs << properties;
    }

    return buildConfigs;
}

bool CommandLineParser::parseCommandLine(const QStringList &args, Settings *settings)
{
    delete d;
    d = new CommandLineParserPrivate;
    d->commandLine = args;
    d->settings = settings;
    try {
        d->doParse();
        return true;
    } catch (const Error &error) {
        qbsError() << error.toString();
        return false;
    }
}


CommandLineParser::CommandLineParserPrivate::CommandLineParserPrivate()
    : command(0), showProgress(false), commandPool(optionPool)
{
}

void CommandLineParser::CommandLineParserPrivate::doParse()
{
    if (commandLine.isEmpty()) { // No command given, use default.
        command = commandPool.getCommand(BuildCommandType);
    } else {
        command = commandFromString(commandLine.first());
        if (command) {
            commandLine.removeFirst();
        } else { // No command given.
            // As an exception to the command-based syntax, we allow -h or --help as the
            // sole contents of the command line, because people are used to this working.
            if (commandLine.count() == 1 && (commandLine.first() == QLatin1String("-h")
                                             || commandLine.first() == QLatin1String("--help"))) {
                command = commandPool.getCommand(HelpCommandType);
                commandLine.clear();
            } else {
                command = commandPool.getCommand(BuildCommandType);
            }
        }
    }
    command->parse(commandLine);

    if (command->type() == HelpCommandType)
        return;

    setupProjectFile();
    setupProgress();
    setupLogLevel();
    setupBuildOptions();
}

Command *CommandLineParser::CommandLineParserPrivate::commandFromString(const QString &commandString) const
{
    foreach (Command * const command, allCommands()) {
        if (command->representation() == commandString) {
            return command;
        }
    }
    return 0;
}

QList<Command *> CommandLineParser::CommandLineParserPrivate::allCommands() const
{
    return QList<Command *>()
            << commandPool.getCommand(BuildCommandType)
            << commandPool.getCommand(CleanCommandType)
            << commandPool.getCommand(RunCommandType)
            << commandPool.getCommand(ShellCommandType)
            << commandPool.getCommand(PropertiesCommandType)
            << commandPool.getCommand(StatusCommandType)
            << commandPool.getCommand(UpdateTimestampsCommandType)
            << commandPool.getCommand(InstallCommandType)
            << commandPool.getCommand(HelpCommandType);
}

QString CommandLineParser::CommandLineParserPrivate::generalHelp() const
{
    QString help = Tr::tr("Usage: qbs [command] [command parameters]\n");
    help += Tr::tr("Internal commands:\n");
    const int rhsIndentation = 30;
    foreach (const Command * command, allCommands()) {
        help.append(QLatin1String("  ")).append(command->representation());
        const QString whitespace
                = QString(rhsIndentation - 2 - command->representation().count(), QLatin1Char(' '));
        help.append(whitespace).append(command->shortDescription()).append(QLatin1Char('\n'));
    }

    const QStringList &toolNames = QbsTool::allToolNames();
    if (!toolNames.isEmpty()) {
        help.append('\n').append(Tr::tr("Auxiliary commands:\n"));
        foreach (const QString &toolName, toolNames) {
            help.append(QLatin1String("  ")).append(toolName);
            const QString whitespace = QString(rhsIndentation - 2 - toolName.count(),
                                               QLatin1Char(' '));
            QbsTool tool;
            tool.runTool(toolName, QStringList(QLatin1String("--help")));
            if (tool.exitCode() != 0)
                continue;
            const QString shortDescription
                    = tool.stdOut().left(tool.stdOut().indexOf(QLatin1Char('\n')));
            help.append(whitespace).append(shortDescription).append(QLatin1Char('\n'));
        }
    }

    return help;
}

void CommandLineParser::CommandLineParserPrivate::setupProjectFile()
{
    projectFilePath = optionPool.fileOption()->projectFilePath();
    if (projectFilePath.isEmpty()) {
        qbsDebug() << "No project file given; looking in current directory.";
        projectFilePath = QDir::currentPath();
    }

    const QFileInfo projectFileInfo(projectFilePath);
    if (!projectFileInfo.exists())
        throw Error(Tr::tr("Project file '%1' cannot be found.").arg(projectFilePath));
    if (projectFileInfo.isRelative())
        projectFilePath = projectFileInfo.absoluteFilePath();
    if (projectFileInfo.isFile())
        return;
    if (!projectFileInfo.isDir())
        throw Error(Tr::tr("Project file '%1' has invalid type.").arg(projectFilePath));

    // TODO: Remove check for '.qbp' in 0.4.
    const QStringList namePatterns = QStringList()
            << QLatin1String("*.qbp") << QLatin1String("*.qbs");

    const QStringList &actualFileNames
            = QDir(projectFilePath).entryList(namePatterns, QDir::Files);
    if (actualFileNames.isEmpty()) {
        QString error;
        if (optionPool.fileOption()->projectFilePath().isEmpty()) {
            error = Tr::tr("No project file given and none found in current directory.\n");
            error += Tr::tr("Usage: %1").arg(command->longDescription());
        } else {
            error = Tr::tr("No project file found in directory '%1'.").arg(projectFilePath);
        }
        throw Error(error);
    }
    if (actualFileNames.count() > 1) {
        throw Error(Tr::tr("More than one project file found in directory '%1'.")
                .arg(projectFilePath));
    }
    projectFilePath.append(QLatin1Char('/')).append(actualFileNames.first());

    projectFilePath = QDir::current().filePath(projectFilePath);
    projectFilePath = QDir::cleanPath(projectFilePath);

    // TODO: Remove in 0.4
    if (projectFilePath.endsWith(QLatin1String(".qbp")))
        qbsInfo() << Tr::tr("Your main project file has the old suffix '.qbp'. This does not "
                        "hurt, but the convention is now to use '.qbs'.");

    qbsDebug() << "Using project file '" << QDir::toNativeSeparators(projectFilePath) << "'.";
}

void CommandLineParser::CommandLineParserPrivate::setupBuildOptions()
{
    buildOptions.dryRun = optionPool.dryRunOption()->enabled();
    buildOptions.changedFiles = optionPool.changedFilesOption()->arguments();
    QDir currentDir;
    for (int i = 0; i < buildOptions.changedFiles.count(); ++i) {
        QString &file = buildOptions.changedFiles[i];
        file = QDir::fromNativeSeparators(currentDir.absoluteFilePath(file));
    }
    buildOptions.keepGoing = optionPool.keepGoingOption()->enabled();
    const JobsOption * jobsOption = optionPool.jobsOption();
    buildOptions.maxJobCount = jobsOption->jobCount() > 0
            ? jobsOption->jobCount() : Preferences(settings).jobs();
}

void CommandLineParser::CommandLineParserPrivate::setupProgress()
{
    const ShowProgressOption * const option = optionPool.showProgressOption();
    showProgress = option->enabled();
#ifdef Q_OS_UNIX
    if (showProgress && !isatty(STDOUT_FILENO)) {
        showProgress = false;
        qbsWarning() << Tr::tr("Ignoring option '%1', because standard output is "
                               "not connected to a terminal.").arg(option->longRepresentation());
    }
#endif
}

void CommandLineParser::CommandLineParserPrivate::setupLogLevel()
{
    const LogLevelOption * const logLevelOption = optionPool.logLevelOption();
    const VerboseOption * const verboseOption = optionPool.verboseOption();
    const QuietOption * const quietOption = optionPool.quietOption();
    int logLevel = logLevelOption->logLevel();
    logLevel += verboseOption->count();
    logLevel -= quietOption->count();

    if (showProgress && logLevel != LoggerMinLevel) {
        const bool logLevelWasSetByUser
                = logLevelOption->logLevel() != defaultLogLevel()
                || verboseOption->count() > 0 || quietOption->count() > 0;
        if (logLevelWasSetByUser) {
            qbsInfo() << Tr::tr("Setting log level to '%1', because option '%2'"
                                " has been given.").arg(logLevelName(LoggerMinLevel),
                                optionPool.showProgressOption()->longRepresentation());
        }
        logLevel = LoggerMinLevel;
    }
    if (logLevel < LoggerMinLevel) {
        qbsWarning() << Tr::tr("Cannot decrease log level as much as specified; using '%1'.")
                .arg(logLevelName(LoggerMinLevel));
        logLevel = LoggerMinLevel;
    } else if (logLevel > LoggerMaxLevel) {
        qbsWarning() << Tr::tr("Cannot increase log level as much as specified; using '%1'.")
                .arg(logLevelName(LoggerMaxLevel));
        logLevel = LoggerMaxLevel;
    }
    ConsoleLogger::instance().logSink()->setLogLevel(static_cast<LoggerLevel>(logLevel));
}

QString CommandLineParser::CommandLineParserPrivate::propertyName(const QString &aCommandLineName) const
{
    // Make fully-qualified, ie "platform" -> "qbs.platform"
    if (aCommandLineName.contains("."))
        return aCommandLineName;
    else
        return "qbs." + aCommandLineName;
}

} // namespace qbs
