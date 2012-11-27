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

#include "commandlineparser.h"

#include <logging/logger.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/platform.h>
#include <tools/settings.h>

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qbs {

static LoggerLevel logLevelFromString(const QString &logLevelString)
{
    const QList<LoggerLevel> levels = QList<LoggerLevel>() << LoggerError << LoggerWarning
            << LoggerInfo << LoggerDebug << LoggerTrace;
    foreach (LoggerLevel l, levels) {
        if (Logger::logLevelName(l) == logLevelString)
            return l;
    }
    throw Error(CommandLineParser::tr("Invalid log level '%1'.").arg(logLevelString));
}

static QStringList allLogLevelStrings()
{
    QStringList result;
    for (int i = static_cast<int>(LoggerMinLevel); i <= static_cast<int>(LoggerMaxLevel); ++i)
        result << Logger::logLevelName(static_cast<LoggerLevel>(i));
    return result;
}

CommandLineParser::CommandLineParser()
{
}

// TODO: Symbolic constants for all option and command names.
void CommandLineParser::printHelp() const
{
    QTextStream stream(m_help ? stdout : stderr);

    stream << "qbs " QBS_VERSION "\n";
    stream << tr("Usage: qbs [command] [options]\n"
         "\nCommands:\n"
         "  build  [variant] [property:value]\n"
         "                     Build project (default command).\n"
         "  clean  ........... Remove all generated files.\n"
         "  properties [variant] [property:value]\n"
         "                     Show all calculated properties for a build.\n"
         "  shell  ........... Open a shell.\n"
         "  status [variant] [property:value]\n"
         "                     List files that are (un)tracked by the project.\n"
         "  run target [variant] [property:value] -- <args>\n"
         "                     Run the specified target.\n"
         "  config\n"
         "                     Set or get project/global option(s).\n"
         "\nGeneral options:\n"
         "  -h -? --help  .... Show this help.\n"
         "  -f <file>  ....... Use <file> as the main project file.\n"
         "  -v  .............. Be more verbose. Increases the log level by one.\n"
         "  -q  .............. Be more quiet. Decreases the log level by one.\n"
         "\nBuild options:\n"
         "  -j <n>  .......... Use <n> jobs (default is the number of cores).\n"
         "  -k  .............. Keep going (ignore errors).\n"
         "  -n  .............. Dry run.\n"
         "  --changed-files file[,file...]\n"
         "      .............. Assume these and only these files have changed.\n"
         "  --products name[,name...]\n"
         "      .............. Build only the specified products.\n"
#ifdef Q_OS_UNIX
         "  --show-progress  . Show a progress bar. Implies --log-level=%3.\n"
#endif
         "  --log-level level\n"
         "      .............. Use the specified log level. Possible values are \"%1\".\n"
         "                     The default is \"%2\".\n")
             .arg(allLogLevelStrings().join(QLatin1String("\", \"")),
                  Logger::logLevelName(Logger::defaultLevel())
#ifdef Q_OS_UNIX
                  , Logger::logLevelName(LoggerMinLevel)
#endif
                  );
}

/**
 * Finds automatically the project file (.qbs) in the search directory.
 * If there's more than one project file found then this function returns false.
 * A project file can explicitely be given by the -f option.
 */
bool CommandLineParser::parseCommandLine(const QStringList &args)
{
    m_commandLine = args;
    try {
        doParse();
    } catch (const Error &error) {
        qbsError(qPrintable(tr("Invalid command line: %1").arg(error.toString())));
        return false;
    }
    return true;
}

void CommandLineParser::doParse()
{
    m_command = BuildCommand;
    m_projectFileName.clear();
    m_products.clear();
    m_buildOptions = BuildOptions();
    m_help = false;
    m_showProgress = false;
    m_logLevel = Logger::defaultLevel();

    while (!m_commandLine.isEmpty()) {
        const QString arg = m_commandLine.takeFirst();
        if (arg.isEmpty())
            throw Error(tr("Empty arguments not allowed."));
        if (arg.startsWith(QLatin1String("--")))
            parseLongOption(arg);
        else if (arg.at(0) == QLatin1Char('-'))
            parseShortOptions(arg);
        else
            parseArgument(arg);
    }

    if (m_showProgress && m_logLevel != LoggerMinLevel) {
        qbsInfo() << tr("Setting log level to \"%1\", because option \"--show-progress\""
                           " has been given.").arg(Logger::logLevelName(LoggerMinLevel));
        m_logLevel = LoggerMinLevel;
    }
    if (m_logLevel < LoggerMinLevel) {
        qbsWarning() << tr("Cannot decrease log level as much as specified; using \"%1\".")
                .arg(Logger::logLevelName(LoggerMinLevel));
        m_logLevel = LoggerMinLevel;
    } else if (m_logLevel > LoggerMaxLevel) {
        qbsWarning() << tr("Cannot increase log level as much as specified; using \"%1\".")
                .arg(Logger::logLevelName(LoggerMaxLevel));
        m_logLevel = LoggerMaxLevel;
    }
    Logger::instance().setLevel(m_logLevel);

    if (isHelpSet())
        return;

    if (m_projectFileName.isEmpty()) {
        qbsDebug() << "No project file given; looking in current directory.";
        m_projectFileName = QDir::currentPath();
    }
    setRealProjectFile();
    m_projectFileName = FileInfo::resolvePath(QDir::currentPath(), m_projectFileName);
    m_projectFileName = QDir::cleanPath(m_projectFileName);

    // TODO: Remove in 0.4
    if (m_projectFileName.endsWith(QLatin1String(".qbp")))
        qbsInfo() << tr("Your main project file has the old suffix '.qbp'. This does not "
                        "hurt, but the convention is now to use '.qbs'.");

    qbsDebug() << DontPrintLogLevel << "Using project file '"
               << QDir::toNativeSeparators(projectFileName()) << "'.";
}

void CommandLineParser::parseLongOption(const QString &option)
{
    const QString optionName = option.mid(2);
    if (optionName.isEmpty()) {
        if (m_command != RunCommand)
            throw Error(tr("Argument '--' only allowed in run mode."));
        m_runArgs = m_commandLine;
        m_commandLine.clear();
    }
    else if (optionName == QLatin1String("help")) {
        m_help = true;
        m_commandLine.clear();
    } else if (optionName == QLatin1String("changed-files") && m_command == BuildCommand) {
        m_buildOptions.changedFiles = getOptionArgumentAsList(option);
        QDir currentDir;
        for (int i = 0; i < m_buildOptions.changedFiles.count(); ++i) {
            QString &file = m_buildOptions.changedFiles[i];
            file = QDir::fromNativeSeparators(currentDir.absoluteFilePath(file));
        }
    } else if (optionName == QLatin1String("products") && (m_command == BuildCommand
            || m_command == CleanCommand || m_command == PropertiesCommand)) {
        m_products = getOptionArgumentAsList(option);
#ifdef Q_OS_UNIX
    } else if (optionName == QLatin1String("show-progress")) {
        if (isatty(STDOUT_FILENO))
            m_showProgress = true;
        else
            qbsWarning() << tr("Ignoring option \"--show-progress\", because standard output is "
                               "not connected to a terminal.");
#endif
    } else if (optionName == QLatin1String("log-level")) {
        m_logLevel = logLevelFromString(getOptionArgument(option));
    } else {
        throw Error(tr("Unknown option '%1'.").arg(option));
    }
}

void CommandLineParser::parseShortOptions(const QString &options)
{
    for (int i = 1; i < options.count(); ++i) {
        const char option = options.at(i).toLower().toLatin1();
        switch (option) {
        case '?':
        case 'h':
            m_help = true;
            m_commandLine.clear();
            return;
        case 'j': {
            const QString jobCountString = getShortOptionArgument(options, i);
            bool stringOk;
            m_buildOptions.maxJobCount = jobCountString.toInt(&stringOk);
            if (!stringOk || m_buildOptions.maxJobCount <= 0)
                throw Error(tr("Invalid job count '%1'.").arg(jobCountString));
            break;
        }
        case 'v':
            ++m_logLevel;
            break;
        case 'q':
            --m_logLevel;
            break;
        case 'f':
            m_projectFileName = QDir::fromNativeSeparators(getShortOptionArgument(options, i));
            break;
        case 'k':
            m_buildOptions.keepGoing = true;
            break;
        case 'n':
            m_buildOptions.dryRun = true;
            break;
        default:
            throw Error(tr("Unknown option '-%1'.").arg(option));
        }
    }
}

QString CommandLineParser::getShortOptionArgument(const QString &options, int optionPos)
{
    const QString option = QLatin1Char('-') + options.at(optionPos);
    if (optionPos < options.count() - 1)
        throw Error(tr("Option '%1' needs an argument.").arg(option));
    return getOptionArgument(option);
}

QString CommandLineParser::getOptionArgument(const QString &option)
{
    if (m_commandLine.isEmpty())
        throw Error(tr("Option '%1' needs an argument.").arg(option));
    return m_commandLine.takeFirst();
}

QStringList CommandLineParser::getOptionArgumentAsList(const QString &option)
{
    if (m_commandLine.isEmpty())
        throw Error(tr("Option '%1' expects an argument.").arg(option));
    const QStringList list = m_commandLine.takeFirst().split(QLatin1Char(','));
    if (list.isEmpty())
        throw Error(tr("Argument list for option '%1' must not be empty.").arg(option));
    foreach (const QString &element, list) {
        if (element.isEmpty()) {
            throw Error(tr("Argument list for option '%1' must not contain empty elements.")
                    .arg(option));
        }
    }
    return list;
}

void CommandLineParser::parseArgument(const QString &arg)
{
    if (arg == QLatin1String("build")) {
        m_command = BuildCommand;
    } else if (arg == QLatin1String("clean")) {
        m_command = CleanCommand;
    } else if (arg == QLatin1String("shell")) {
        m_command = StartShellCommand;
    } else if (arg == QLatin1String("run")) {
        m_command = RunCommand;
    } else if (m_command == RunCommand && m_runTargetName.isEmpty()) {
        m_runTargetName = arg;
    } else if (arg == QLatin1String("status")) {
        m_command = StatusCommand;
    } else if (arg == QLatin1String("properties")) {
        m_command = PropertiesCommand;
    } else {
        m_positional.append(arg);
    }
}

QString CommandLineParser::propertyName(const QString &aCommandLineName) const
{
    // Make fully-qualified, ie "platform" -> "qbs.platform"
    if (aCommandLineName.contains("."))
        return aCommandLineName;
    else
        return "qbs." + aCommandLineName;
}

void CommandLineParser::setRealProjectFile()
{
    const QFileInfo projectFileInfo(m_projectFileName);
    if (!projectFileInfo.exists())
        throw Error(tr("Project file '%1' cannot be found.").arg(m_projectFileName));
    if (projectFileInfo.isFile())
        return;
    if (!projectFileInfo.isDir())
        throw Error(tr("Project file '%1' has invalid type.").arg(m_projectFileName));

    // TODO: Remove check for '.qbp' in 0.4.
    const QStringList namePatterns = QStringList()
            << QLatin1String("*.qbp") << QLatin1String("*.qbs");

    const QStringList &actualFileNames
            = QDir(m_projectFileName).entryList(namePatterns, QDir::Files);
    if (actualFileNames.isEmpty())
        throw Error(tr("No project file found in directory '%1'.").arg(m_projectFileName));
    if (actualFileNames.count() > 1) {
        throw Error(tr("More than one project file found in directory '%1'.")
                .arg(m_projectFileName));
    }
    m_projectFileName.append(QLatin1Char('/')).append(actualFileNames.first());
}

QList<QVariantMap> CommandLineParser::buildConfigurations() const
{
    // Key: build variant, value: properties. Empty key used for global properties.
    typedef QMap<QString, QVariantMap> PropertyMaps;
    PropertyMaps properties;

    const QString buildVariantKey = QLatin1String("qbs.buildVariant");
    QString currentKey = QString();
    QVariantMap currentProperties;
    foreach (const QString &arg, m_positional) {
        const int sepPos = arg.indexOf(QLatin1Char(':'));
        if (sepPos == -1) { // New build variant found.
            properties.insert(currentKey, currentProperties);
            currentKey = arg;
            currentProperties.clear();
            continue;
        }
        const QString property = propertyName(arg.left(sepPos));
        if (property.isEmpty())
            qbsWarning() << tr("Ignoring empty property.");
        else if (property == buildVariantKey)
            qbsWarning() << tr("Refusing to overwrite special property '%1'.").arg(buildVariantKey);
        else
            currentProperties.insert(property, arg.mid(sepPos + 1));
    }
    properties.insert(currentKey, currentProperties);

    if (properties.count() == 1) // No build variant specified on command line.
        properties.insert(Settings().buildVariant(), QVariantMap());

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

} // namespace qbs
