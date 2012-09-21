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

#include "options.h"

#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/logger.h>
#include <tools/platformglobals.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <tools/platform.h>

namespace qbs {

CommandLineOptions::CommandLineOptions()
    : m_command(BuildCommand)
    , m_dumpGraph(false)
    , m_gephi (false)
    , m_help (false)
    , m_clean (false)
    , m_keepGoing(false)
{
    m_settings = Settings::create();
    m_jobs = configurationValue("preferences/jobs", 0).toInt();
    if (m_jobs <= 0)
        m_jobs = QThread::idealThreadCount();
}

void CommandLineOptions::printHelp()
{
    puts("qbs " QBS_VERSION "\n");
    fputs("usage: qbs [command] [options]\n"
         "\ncommands:\n"
         "  build  [variant] [property:value]\n"
         "                     build project (default command)\n"
         "  clean  ........... remove all generated files\n"
         "  properties [variant] [property:value]\n"
         "                     show all calculated properties for a build\n"
         "  shell  ........... open a shell\n"
         "  status [variant] [property:value]\n"
         "                     list files that are (un)tracked by the project\n"
         "  run target [variant] [property:value] -- <args>\n"
         "                     run the specified target\n"
         "  config\n"
         "                     set, get, or get all project/global options\n"
         "\ngeneral options:\n"
         "  -h -? --help  .... this help\n"
         "  -d  .............. dump graph\n"
         "  -f <file>  ....... specify .qbp project file\n"
         "  -v  .............. verbose (add up to 5 v to increase verbosity level)\n"
         "\nbuild options:\n"
         "  -j <n>  .......... use <n> jobs (default is nr of cores)\n"
         "  -k  .............. keep going (ignore errors)\n"
         "  -n  .............. dry run\n"
         "  --changed-files file [file 2] ... [file n]\n"
         "      .............. specify a list of out of date files\n"
         "  --products ProductName [product name 2] ... [product name n]\n"
         "      .............. specify a list of products to build\n"
         "\ngraph options:\n"
         "  -g  .............. show generation graph\n"
         , stdout);
}

/**
 * Finds automatically the project file (.qbs) in the search directory.
 * If there's more than one project file found then this function returns false.
 * A project file can explicitely be given by the -f option.
 */
bool CommandLineOptions::readCommandLineArguments(const QStringList &args)
{
    m_projectFileName.clear();
    m_dryRun = false;
    bool firstPositionalEaten = false;
    bool runArgMode = false;
    int verbosity = 0;

    const int argc = args.size();
    for (int i = 0; i < argc; ++i) {
        QString arg = args.at(i);
        if (runArgMode) {
            m_runArgs.append(arg);
            continue;
        }
        if (arg == "--") {
            runArgMode = true;
            continue;
        }

        // --- no dash
        if (arg.at(0) != '-' || arg.length() < 2) {
            if (arg == QLatin1String("build")) {
                m_command = BuildCommand;
            } else if (arg == QLatin1String("config")) {
                m_command = ConfigCommand;
                m_configureArgs = args.mid(i + 1);
                break;
            } else if (arg == QLatin1String("clean")) {
                m_command = CleanCommand;
            } else if (arg == QLatin1String("shell")) {
                m_command = StartShellCommand;
            } else if (arg == QLatin1String("run")) {
                m_command = RunCommand;
            } else if (m_command == RunCommand && !firstPositionalEaten) {
                m_runTargetName = arg;
                firstPositionalEaten = true;
            } else if (arg == QLatin1String("status")) {
                m_command = StatusCommand;
            } else if (arg == QLatin1String("properties")) {
                m_command = PropertiesCommand;
            } else {
                m_positional.append(arg);
            }

        // --- two dashes
        } else if (arg.at(1) == '-') {

            arg = arg.remove(0, 2).toLower();
            QString *targetString = 0;
            QStringList *targetStringList = 0;

            if (arg == QLatin1String("help")) {
                m_help = true;
            } else if (arg == "changed-files" && m_command == BuildCommand) {
                QStringList changedFiles;
                for (++i; i < argc && !args.at(i).startsWith('-'); ++i)
                    changedFiles += args.at(i);
                if (changedFiles.isEmpty()) {
                    qbsError("--changed-files expects one or more file names.");
                    return false;
                }
                m_changedFiles = changedFiles;
                --i;
                continue;
            } else if (arg == "products" && (m_command == BuildCommand || m_command == CleanCommand || m_command == PropertiesCommand)) {
                QStringList productNames;
                for (++i; i < argc && !args.at(i).startsWith('-'); ++i)
                    productNames += args.at(i);
                if (productNames.isEmpty()) {
                    qbsError("--products expects one or more product names.");
                    return false;
                }
                m_selectedProductNames = productNames;
                --i;
                continue;
            } else {
                QString err = QLatin1String("Parameter '%1' is unknown.");
                qbsError(qPrintable(err.arg(QLatin1String("--") + arg)));
                return false;
            }

            QString stringValue;
            if (targetString || targetStringList) {
                // string value expected
                if (++i >= argc) {
                    QString err = QLatin1String("String value expected after parameter '%1'.");
                    qbsError(qPrintable(err.arg(QLatin1String("--") + arg)));
                    return false;
                }
                stringValue = args.at(i);
            }

            if (targetString)
                *targetString = stringValue;
            else if (targetStringList)
                targetStringList->append(stringValue);

        // --- one dash
        } else {
            int k = 1;
            switch (arg.at(1).toLower().unicode()) {
            case '?':
            case 'h':
                m_help = true;
                break;
            case 'j':
                if (arg.length() > 2) {
                    const QByteArray str(arg.mid(2).toLatin1());
                    char *endStr = 0;
                    m_jobs = strtoul(str.data(), &endStr, 10);
                    k += endStr - str.data();
                } else if (++i < argc) {
                    m_jobs = args.at(i).toInt();
                }
                if (m_jobs < 1)
                    return false;
                break;
            case 'v':
                verbosity = 1;
                while (k < arg.length() - 1 && arg.at(k + 1).toLower().unicode() == 'v') {
                    verbosity++;
                    k++;
                }
                break;
            case 'd':
                m_dumpGraph = true;
                break;
            case 'f':
                if (arg.length() > 2) {
                    m_projectFileName = arg.mid(2);
                    k += m_projectFileName.length();
                } else if (++i < argc) {
                    m_projectFileName = args.at(i);
                }
                m_projectFileName = QDir::fromNativeSeparators(m_projectFileName);
                break;
            case 'g':
                m_gephi = true;
                break;
            case 'k':
                m_keepGoing = true;
                break;
            case 'n':
                m_dryRun = true;
                break;
            default:
                qbsError(qPrintable(QString("Unknown option '%1'.").arg(arg.at(1))));
                return false;
            }
            if (k < arg.length() - 1) { // Trailing characters?
                qbsError(qPrintable(QString("Invalid option '%1'.").arg(arg)));
                return false;
            }
        }
    }

    if (verbosity)
        Logger::instance().setLevel(verbosity);

    // automatically detect the project file name
    if (m_projectFileName.isEmpty())
        m_projectFileName = guessProjectFileName();

    // make the project file name absolute
    if (!m_projectFileName.isEmpty() && !FileInfo::isAbsolute(m_projectFileName)) {
        m_projectFileName = FileInfo::resolvePath(QDir::currentPath(), m_projectFileName);
    }

    // eventually load defaults from configs
    const QChar configPathSeparator = QLatin1Char(nativePathVariableSeparator);
    if (m_searchPaths.isEmpty())
        m_searchPaths = configurationValue("preferences/qbsPath", QVariant()).toString().split(configPathSeparator, QString::SkipEmptyParts);
    m_pluginPaths = configurationValue("preferences/pluginsPath", QVariant()).toString().split(configPathSeparator, QString::SkipEmptyParts);

    // fixup some defaults
    if (m_searchPaths.isEmpty())
        m_searchPaths.append(qbsRootPath() + "/share/qbs/");
    if (m_pluginPaths.isEmpty())
        m_pluginPaths.append(qbsRootPath() + "/plugins/");

    return true;
}

static void showConfigUsage()
{
    puts("usage: qbs config [options]\n"
         "\n"
         "Config file location:\n"
         "    --global    choose global configuration file\n"
         "    --local     choose local configuration file (default)\n"
         "\n"
         "Actions:\n"
         "    --list           list all variables\n"
         "    --unset          remove variable with given name\n"
         "    --import <file>  import global settings from given file\n"
         "    --export <file>  export global settings to given file\n");
}

void CommandLineOptions::loadLocalProjectSettings(bool throwExceptionOnFailure)
{
    if (m_settings->hasProjectSettings())
        return;
    if (throwExceptionOnFailure && m_projectFileName.isEmpty())
        throw Error("Can't find a project file in the current directory. Local configuration not available.");
    m_settings->loadProjectSettings(m_projectFileName);
}

void CommandLineOptions::configure()
{
    enum ConfigCommand { CfgSet, CfgUnset, CfgList, CfgExport, CfgImport, CfgUpgrade };
    ConfigCommand cmd = CfgSet;
    Settings::Scope scope = Settings::Local;
    bool scopeSet = false;

    QStringList args = m_configureArgs;
    m_configureArgs.clear();
    if (args.isEmpty()) {
        showConfigUsage();
        return;
    }

    while (!args.isEmpty() && args.first().startsWith("--")) {
        QString arg = args.takeFirst();
        arg.remove(0, 2);
        if (arg == "list") {
            cmd = CfgList;
        } else if (arg == "unset") {
            cmd = CfgUnset;
        } else if (arg == "global") {
            scope = Settings::Global;
            scopeSet = true;
        } else if (arg == "local") {
            scope = Settings::Local;
            scopeSet = true;
        } else if (arg == "export") {
            cmd = CfgExport;
        } else if (arg == "import") {
            cmd = CfgImport;
        } else if (arg == "upgrade") {
            cmd = CfgUpgrade;
        } else {
            throw Error("Unknown option for config command.");
        }
    }

    switch (cmd) {
    case CfgList:
        if (scopeSet) {
            if (scope == Settings::Local)
                loadLocalProjectSettings(true);
            printSettings(scope);
        } else {
            loadLocalProjectSettings(false);
            printSettings(Settings::Local);
            printSettings(Settings::Global);
        }
        break;
    case CfgSet:
        if (scope == Settings::Local)
            loadLocalProjectSettings(true);
        if (args.count() > 2)
            throw Error("Too many arguments for setting a configuration value.");
        if (args.count() == 0) {
            showConfigUsage();
        } else if (args.count() < 2) {
            puts(qPrintable(m_settings->value(scope, args.at(0)).toString()));
        } else {
            QString key(args.at(0));
            key.replace(QChar('.'), QChar('/'));
            m_settings->setValue(scope, key, args.at(1));
        }
        break;
    case CfgUnset:
        if (scope == Settings::Local)
            loadLocalProjectSettings(true);
        if (args.isEmpty())
            throw Error("unset what?");
        foreach (const QString &arg, args) {
            QString key(arg);
            key.replace(QChar('.'), QChar('/'));
            m_settings->remove(scope, key);
        }
        break;
    case CfgExport:
        if (args.count() != 1)
            throw Error("Syntax: qbs config --export <filename>");
        exportGlobalSettings(args[0]);
        break;
    case CfgImport:
        if (args.count() != 1)
            throw Error("Syntax: qbs config --import <filename>");
        // Display old and new settings, in case import fails or user accidentally nukes everything
        printf("old "); // Will end up as "old global settings:"
        printSettings(Settings::Global);
        importGlobalSettings(args[0]);
        printf("\nnew ");
        printSettings(Settings::Global);
        break;
    case CfgUpgrade:
        if (scopeSet) {
            if (scope == Settings::Local)
                loadLocalProjectSettings(true);
            upgradeSettings(scope);
        } else {
            loadLocalProjectSettings(false);
            upgradeSettings(Settings::Local);
            upgradeSettings(Settings::Global);
        }
        break;
    }
}

QVariant CommandLineOptions::configurationValue(const QString &key, const QVariant &defaultValue)
{
    return m_settings->value(key, defaultValue);
}

template <typename S, typename T>
QMap<S,T> &inhaleValues(QMap<S,T> &dst, const QMap<S,T> &src)
{
    for (typename QMap<S,T>::const_iterator it=src.constBegin();
         it != src.constEnd(); ++it)
    {
        dst.insert(it.key(), it.value());
    }
    return dst;
}

QString CommandLineOptions::propertyName(const QString &aCommandLineName) const
{
    // Make fully-qualified, ie "platform" -> "qbs.platform"
    // "profile" is the only top-level thing that doesn't have a dot or belong in the qbs module
    if (aCommandLineName.contains(".") || aCommandLineName == "profile")
        return aCommandLineName;
    else
        return "qbs." + aCommandLineName;
}

QList<QVariantMap> CommandLineOptions::buildConfigurations() const
{
    QList<QVariantMap> ret;
    QVariantMap globalSet;
    QVariantMap currentSet;
    QString currentName;
    foreach (const QString &arg, positional()) {
        int idx = arg.indexOf(':');
        if (idx < 0) {
            if (currentName.isEmpty()) {
                globalSet = currentSet;
            } else {
                QVariantMap map = globalSet;
                inhaleValues(map, currentSet);
                if (!map.contains("qbs.buildVariant"))
                    map["qbs.buildVariant"] = currentName;
                ret.append(map);
            }
            currentSet.clear();
            currentName = arg;
        } else {
            currentSet.insert(propertyName(arg.left(idx)), arg.mid(idx + 1));
        }
    }

    if (currentName.isEmpty()) {
        // If a build variant is not specified the default build variant can be given by
        // modules.qbs.buildVariant or by any of the current profiles
        QStringList currentProfiles = currentSet.value("profile").toString().split(QChar(','), QString::SkipEmptyParts);
        currentName = m_settings->moduleValue("qbs/buildVariant", currentProfiles, "").toString();
    }
    if (currentName.isEmpty())
        currentName = m_settings->value("modules/qbs/buildvariant", "debug").toString();
    QVariantMap map = globalSet;
    inhaleValues(map, currentSet);
    if (!map.contains("qbs.buildVariant"))
        map["qbs.buildVariant"] = currentName;
    ret.append(map);
    return ret;
}

void CommandLineOptions::printSettings(Settings::Scope scope)
{
    if (scope == Settings::Global)
        printf("global variables:\n");
    else
        printf("local variables:\n");
    foreach (const QString &key, m_settings->allKeys(scope)) {
        QString prettyKey(key);
        prettyKey.replace(QChar('/'), QChar('.'));
        printf("%s: %s\n", qPrintable(prettyKey),
               qPrintable(m_settings->value(scope, key).toString()));
    }
}

QString CommandLineOptions::guessProjectFileName()
{
    QDir searchDir = QDir::current();
    for (;;) {
        QStringList projectFiles = searchDir.entryList(QStringList() << "*.qbp", QDir::Files);
        if (projectFiles.count() == 1) {
            QDir::setCurrent(searchDir.path());
            return searchDir.absoluteFilePath(projectFiles.first());
        } else if (projectFiles.count() > 1) {
            QString str = QLatin1String("Multiple project files found in '%1'.\n"
                    "Please specify the correct project file using the -f option.");
            qbsError(qPrintable(str.arg(QDir::toNativeSeparators(searchDir.absolutePath()))));
            return QString();
        }
        if (!searchDir.cdUp())
            break;
    }
    return QString();
}

void CommandLineOptions::exportGlobalSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::Truncate | QFile::WriteOnly | QFile::Text)) {
        qbsFatal("Couldn't open file %s for writing: %s", qPrintable(filename), qPrintable(file.errorString()));
        return;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    foreach (const QString &key, m_settings->allKeys(Settings::Global)) {
        QString prettyKey(key);
        prettyKey.replace(QChar('/'), QChar('.'));
        stream << prettyKey << ": " << m_settings->value(Settings::Global, key).toString() << endl;
    }
}

void CommandLineOptions::importGlobalSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qbsFatal("Couldn't open file %s for reading: %s", qPrintable(filename), qPrintable(file.errorString()));
        return;
    }
    // Remove all current settings
    foreach (const QString &key, m_settings->allKeys(Settings::Global)) {
        m_settings->remove(Settings::Global, key);
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        int colon = line.indexOf(':');
        if (colon >= 0 && !line.startsWith("#")) {
            QString key = line.left(colon).trimmed();
            key.replace(QChar('.'), QChar('/'));
            QString value = line.mid(colon + 1).trimmed();
            m_settings->setValue(Settings::Global, key, value);
        }
    }
}

void CommandLineOptions::upgradeSettings(Settings::Scope scope)
{
    // This upgrades config settings from v0.1 format to v0.2 format
    // ### REMOVE this after v0.2!
    QStringList allKeys = m_settings->allKeys(scope);
    bool seenQtDefaultKey = false;
    foreach (const QString &key, allKeys) {
        QString newKey;
        QVariant newValue = m_settings->value(scope, key);
        if (key.startsWith("qt/")) {
            // Replace with qt/name/* with profiles/name/qt/core/*
            QStringList components = key.split("/");
            components[0] = "profiles";
            // The examples used to show Qt names with dots in, we can't allow that any more
            components[1].replace(".", "_");
            components.insert(2, "qt");
            components.insert(3, "core");
            newKey = components.join("/");
            if (key.startsWith("qt/default"))
                seenQtDefaultKey = true;
        } else if (key.compare("defaults/buildvariant", Qt::CaseInsensitive) == 0) {
            newKey = "modules/qbs/buildVariant";
        } else if (key == "defaults/platform") {
            newKey = "modules/qbs/platform";
        } else if (key == "defaults/qtVersionName") {
            newKey = "profile";
            QString val = newValue.toString();
            val.replace(".", "_");
            newValue = val;
        } else if (key == "defaults/jobs") {
            newKey = "preferences/jobs";
        } else if (key == "defaults/useColoredOutput") {
            newKey = "preferences/useColoredOutput";
        } else if (key == "paths/cubes") {
            newKey = "preferences/qbsPath";
        } else if (key == "paths/plugins") {
            newKey = "preferences/pluginsPath";
        }

        if (newKey.length()) {
            printf("Replacing %s with %s\n", qPrintable(QString(key).replace('/', '.')), qPrintable(QString(newKey).replace('/', '.')));
            m_settings->remove(scope, key);
            m_settings->setValue(scope, newKey, newValue);
        }
    }
    if (seenQtDefaultKey && !m_settings->value(scope, "profile").isValid()) {
        printf("Setting profile key to use 'profiles.default'\n");
        m_settings->setValue(scope, "profile", "default");
    }
}

} // namespace qbs
