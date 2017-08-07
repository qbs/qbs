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

#include "commandlineparser.h"

#include "commandlineoption.h"
#include "commandlineoptionpool.h"
#include "commandpool.h"
#include "parsercommand.h"
#include "../qbstool.h"
#include "../../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/buildoptions.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/generateoptions.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/settings.h>
#include <tools/settingsmodel.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qmap.h>
#include <QtCore/qtextstream.h>

#include <utility>

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
    void setupBuildDirectory();
    void setupProgress();
    void setupLogLevel();
    void setupBuildOptions();
    void setupBuildConfigurations();
    bool checkForExistingBuildConfiguration(const QList<QVariantMap> &buildConfigs,
                                            const QString &configurationName);
    bool withNonDefaultProducts() const;
    bool dryRun() const;
    QString settingsDir() const { return  optionPool.settingsDirOption()->settingsDir(); }

    CommandEchoMode echoMode() const;

    QString propertyName(const QString &aCommandLineName) const;

    QStringList commandLine;
    Command *command;
    QString projectFilePath;
    QString projectBuildDirectory;
    BuildOptions buildOptions;
    QList<QVariantMap> buildConfigurations;
    CommandLineOptionPool optionPool;
    CommandPool commandPool;
    bool showProgress;
    bool logTime;
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
            throw ErrorInfo(Tr::tr("No such command '%1'.\n%2")
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

QString CommandLineParser::projectBuildDirectory() const
{
    return d->projectBuildDirectory;
}

BuildOptions CommandLineParser::buildOptions(const QString &profile) const
{
    Settings settings(settingsDir());
    Preferences preferences(&settings, profile);

    if (d->buildOptions.maxJobCount() <= 0) {
        d->buildOptions.setMaxJobCount(preferences.jobs());
    }

    if (d->buildOptions.echoMode() < 0) {
        d->buildOptions.setEchoMode(preferences.defaultEchoMode());
    }

    return d->buildOptions;
}

CleanOptions CommandLineParser::cleanOptions(const QString &profile) const
{
    CleanOptions options;
    options.setDryRun(buildOptions(profile).dryRun());
    options.setKeepGoing(buildOptions(profile).keepGoing());
    options.setLogElapsedTime(logTime());
    return options;
}

GenerateOptions CommandLineParser::generateOptions() const
{
    GenerateOptions options;
    options.setGeneratorName(d->optionPool.generatorOption()->generatorName());
    return options;
}

InstallOptions CommandLineParser::installOptions(const QString &profile) const
{
    InstallOptions options;
    options.setRemoveExistingInstallation(d->optionPool.removeFirstoption()->enabled());
    options.setInstallRoot(d->optionPool.installRootOption()->installRoot());
    options.setInstallIntoSysroot(d->optionPool.installRootOption()->useSysroot());
    if (!options.installRoot().isEmpty()) {
        QFileInfo fi(options.installRoot());
        if (!fi.isAbsolute())
            options.setInstallRoot(fi.absoluteFilePath());
    }
    options.setDryRun(buildOptions(profile).dryRun());
    options.setKeepGoing(buildOptions(profile).keepGoing());
    options.setLogElapsedTime(logTime());
    return options;
}

bool CommandLineParser::force() const
{
    return d->optionPool.forceOption()->enabled();
}

bool CommandLineParser::forceTimestampCheck() const
{
    return d->optionPool.forceTimestampCheckOption()->enabled();
}

bool CommandLineParser::forceOutputCheck() const
{
    return d->optionPool.forceOutputCheckOption()->enabled();
}

bool CommandLineParser::dryRun() const
{
    return d->dryRun();
}

bool CommandLineParser::forceProbesExecution() const
{
    return d->optionPool.forceProbesOption()->enabled();
}

bool CommandLineParser::waitLockBuildGraph() const
{
    return d->optionPool.waitLockOption()->enabled();
}

bool CommandLineParser::logTime() const
{
    return d->logTime;
}

bool CommandLineParser::withNonDefaultProducts() const
{
    return d->withNonDefaultProducts();
}

bool CommandLineParser::buildBeforeInstalling() const
{
    return !d->optionPool.noBuildOption()->enabled();
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

bool CommandLineParser::showVersion() const
{
    return d->optionPool.versionOption()->enabled();
}

QString CommandLineParser::settingsDir() const
{
    return d->settingsDir();
}

QString CommandLineParser::commandName() const
{
    return d->command->representation();
}

QString CommandLineParser::commandDescription() const
{
    return d->command->longDescription();
}

static QString getBuildConfigurationName(const QVariantMap &buildConfig)
{
    return buildConfig.value(QLatin1String("qbs.configurationName")).toString();
}

QList<QVariantMap> CommandLineParser::buildConfigurations() const
{
    return d->buildConfigurations;
}

bool CommandLineParser::parseCommandLine(const QStringList &args)
{
    delete d;
    d = new CommandLineParserPrivate;
    d->commandLine = args;
    try {
        d->doParse();
        return true;
    } catch (const ErrorInfo &error) {
        qbsError() << error.toString();
        return false;
    }
}


CommandLineParser::CommandLineParserPrivate::CommandLineParserPrivate()
    : command(0), commandPool(optionPool), showProgress(false), logTime(false)
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

    if (command->type() == BuildCommandType && optionPool.versionOption()->enabled())
        return;

    setupBuildDirectory();
    setupBuildConfigurations();
    setupProjectFile();
    setupProgress();
    setupLogLevel();
    setupBuildOptions();
}

Command *CommandLineParser::CommandLineParserPrivate::commandFromString(const QString &commandString) const
{
    foreach (Command * const command, allCommands()) {
        if (command->representation() == commandString)
            return command;
    }
    return 0;
}

QList<Command *> CommandLineParser::CommandLineParserPrivate::allCommands() const
{
    return QList<Command *>()
            << commandPool.getCommand(GenerateCommandType)
            << commandPool.getCommand(ResolveCommandType)
            << commandPool.getCommand(BuildCommandType)
            << commandPool.getCommand(CleanCommandType)
            << commandPool.getCommand(RunCommandType)
            << commandPool.getCommand(ShellCommandType)
            << commandPool.getCommand(StatusCommandType)
            << commandPool.getCommand(UpdateTimestampsCommandType)
            << commandPool.getCommand(InstallCommandType)
            << commandPool.getCommand(DumpNodesTreeCommandType)
            << commandPool.getCommand(HelpCommandType);
}

static QString extractToolDescription(const QString &tool, const QString &output)
{
    if (tool == QLatin1String("create-project")) {
        // This command uses QCommandLineParser, where the description is not in the first line.
        const int eol1Pos = output.indexOf(QLatin1Char('\n'));
        const int eol2Pos = output.indexOf(QLatin1Char('\n'), eol1Pos + 1);
        return output.mid(eol1Pos + 1, eol2Pos - eol1Pos - 1);
    }
    return output.left(output.indexOf(QLatin1Char('\n')));
}

QString CommandLineParser::CommandLineParserPrivate::generalHelp() const
{
    QString help = Tr::tr("Usage: qbs [command] [command parameters]\n");
    help += Tr::tr("Built-in commands:\n");
    const int rhsIndentation = 30;

    // Sorting the commands by name is nicer for the user.
    QMap<QString, const Command *> commandMap;
    foreach (const Command * command, allCommands())
        commandMap.insert(command->representation(), command);

    foreach (const Command * command, commandMap) {
        help.append(QLatin1String("  ")).append(command->representation());
        const QString whitespace
                = QString(rhsIndentation - 2 - command->representation().count(), QLatin1Char(' '));
        help.append(whitespace).append(command->shortDescription()).append(QLatin1Char('\n'));
    }

    QStringList toolNames = QbsTool::allToolNames();
    toolNames.sort();
    if (!toolNames.isEmpty()) {
        help.append(QLatin1Char('\n')).append(Tr::tr("Auxiliary commands:\n"));
        foreach (const QString &toolName, toolNames) {
            help.append(QLatin1String("  ")).append(toolName);
            const QString whitespace = QString(rhsIndentation - 2 - toolName.count(),
                                               QLatin1Char(' '));
            QbsTool tool;
            tool.runTool(toolName, QStringList(QLatin1String("--help")));
            if (tool.exitCode() != 0)
                continue;
            const QString shortDescription = extractToolDescription(toolName, tool.stdOut());
            help.append(whitespace).append(shortDescription).append(QLatin1Char('\n'));
        }
    }

    return help;
}

void CommandLineParser::CommandLineParserPrivate::setupProjectFile()
{
    projectFilePath = optionPool.fileOption()->projectFilePath();
}

void CommandLineParser::CommandLineParserPrivate::setupBuildDirectory()
{
    projectBuildDirectory = optionPool.buildDirectoryOption()->projectBuildDirectory();
}

void CommandLineParser::CommandLineParserPrivate::setupBuildOptions()
{
    buildOptions.setDryRun(dryRun());
    QStringList changedFiles = optionPool.changedFilesOption()->arguments();
    QDir currentDir;
    for (int i = 0; i < changedFiles.count(); ++i) {
        QString &file = changedFiles[i];
        file = QDir::fromNativeSeparators(currentDir.absoluteFilePath(file));
    }
    buildOptions.setChangedFiles(changedFiles);
    buildOptions.setKeepGoing(optionPool.keepGoingOption()->enabled());
    buildOptions.setForceTimestampCheck(optionPool.forceTimestampCheckOption()->enabled());
    buildOptions.setForceOutputCheck(optionPool.forceOutputCheckOption()->enabled());
    const JobsOption * jobsOption = optionPool.jobsOption();
    buildOptions.setMaxJobCount(jobsOption->jobCount());
    buildOptions.setLogElapsedTime(logTime);
    buildOptions.setEchoMode(echoMode());
    buildOptions.setInstall(!optionPool.noInstallOption()->enabled());
    buildOptions.setRemoveExistingInstallation(optionPool.removeFirstoption()->enabled());
}

void CommandLineParser::CommandLineParserPrivate::setupBuildConfigurations()
{
    // first: configuration name, second: properties.
    // Empty configuration name used for global properties.
    typedef std::pair<QString, QVariantMap> PropertyListItem;
    QList<PropertyListItem> propertiesPerConfiguration;

    const QString configurationNameKey = QLatin1String("qbs.configurationName");
    QString currentConfigurationName;
    QVariantMap currentProperties;
    foreach (const QString &arg, command->additionalArguments()) {
        const int sepPos = arg.indexOf(QLatin1Char(':'));
        if (sepPos == -1) { // New build configuration found.
            propertiesPerConfiguration << std::make_pair(currentConfigurationName,
                                                         currentProperties);
            currentConfigurationName = arg;
            currentProperties.clear();
            continue;
        }
        const QString property = propertyName(arg.left(sepPos));
        if (property.isEmpty()) {
            qbsWarning() << Tr::tr("Ignoring empty property.");
        } else if (property == configurationNameKey) {
            qbsWarning() << Tr::tr("Refusing to overwrite special property '%1'.")
                            .arg(configurationNameKey);
        } else {
            const QString rawString = arg.mid(sepPos + 1);
            currentProperties.insert(property, representationToSettingsValue(rawString));
        }
    }
    propertiesPerConfiguration << std::make_pair(currentConfigurationName, currentProperties);

    if (propertiesPerConfiguration.count() == 1) // No configuration name specified on command line.
        propertiesPerConfiguration << PropertyListItem(QStringLiteral("default"), QVariantMap());

    const QVariantMap globalProperties = propertiesPerConfiguration.takeFirst().second;
    QList<QVariantMap> buildConfigs;
    foreach (const PropertyListItem &item, propertiesPerConfiguration) {
        QVariantMap properties = item.second;
        for (QVariantMap::ConstIterator globalPropIt = globalProperties.constBegin();
                 globalPropIt != globalProperties.constEnd(); ++globalPropIt) {
            if (!properties.contains(globalPropIt.key()))
                properties.insert(globalPropIt.key(), globalPropIt.value());
        }

        const QString configurationName = item.first;
        if (checkForExistingBuildConfiguration(buildConfigs, configurationName)) {
            qbsWarning() << Tr::tr("Ignoring redundant request to build for configuration '%1'.")
                            .arg(configurationName);
            continue;
        }

        properties.insert(configurationNameKey, configurationName);
        buildConfigs << properties;
    }

    buildConfigurations = buildConfigs;
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

    logTime = optionPool.logTimeOption()->enabled();
    if (showProgress && logTime) {
        qbsWarning() << Tr::tr("Options '%1' and '%2' are incompatible. Ignoring '%2'.")
                .arg(optionPool.showProgressOption()->longRepresentation(),
                     optionPool.logTimeOption()->longRepresentation());
        logTime = false;
    }

    ConsoleLogger::instance().logSink()->setLogLevel(static_cast<LoggerLevel>(logLevel));
}

QString CommandLineParser::CommandLineParserPrivate::propertyName(const QString &aCommandLineName) const
{
    // Make fully-qualified, ie "platform" -> "qbs.platform"
    if (aCommandLineName.contains(QLatin1Char('.')))
        return aCommandLineName;
    else
        return QLatin1String("qbs.") + aCommandLineName;
}

bool CommandLineParser::CommandLineParserPrivate::checkForExistingBuildConfiguration(
        const QList<QVariantMap> &buildConfigs, const QString &configurationName)
{
    foreach (const QVariantMap &buildConfig, buildConfigs) {
        if (configurationName == getBuildConfigurationName(buildConfig))
            return true;
    }
    return false;
}

bool CommandLineParser::CommandLineParserPrivate::withNonDefaultProducts() const
{
    if (command->type() == GenerateCommandType)
        return true;
    return optionPool.buildNonDefaultOption()->enabled();
}

bool CommandLineParser::CommandLineParserPrivate::dryRun() const
{
     if (command->type() == GenerateCommandType)
         return true;
     return optionPool.dryRunOption()->enabled();
}

CommandEchoMode CommandLineParser::CommandLineParserPrivate::echoMode() const
{
    if (command->type() == GenerateCommandType)
        return CommandEchoModeSilent;

    if (optionPool.commandEchoModeOption()->commandEchoMode() < CommandEchoModeInvalid)
        return optionPool.commandEchoModeOption()->commandEchoMode();

    return defaultCommandEchoMode();
}

} // namespace qbs
