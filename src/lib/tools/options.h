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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <tools/settings.h>

#include <QPair>
#include <QStringList>
#include <QVariant>

namespace qbs {
/**
  * Command line options for qbs command line tools.
  * ### TODO: move to qbs application.
  */
class CommandLineOptions
{
public:
    CommandLineOptions();

    enum Command {
        BuildCommand,
        CleanCommand,
        ConfigCommand,
        StartShellCommand,
        RunCommand,
        StatusCommand,
        PropertiesCommand
    };

    static void printHelp();
    Command command() const { return m_command;}
    bool readCommandLineArguments(const QStringList &args);
    void configure();
    const QString &runTargetName() const { return m_runTargetName; }
    const QString &projectFileName() const { return m_projectFileName; }
    const QStringList &searchPaths() const { return m_searchPaths; }
    const QStringList &pluginPaths() const { return m_pluginPaths; }
    const QStringList &positional() const { return m_positional; }
    const QStringList &runArgs() const { return m_runArgs; }
    const QStringList &changedFiles() const { return m_changedFiles; }
    const QStringList &selectedProductNames() const { return m_selectedProductNames; }
    bool isDumpGraphSet() const { return m_dumpGraph; }
    bool isGephiSet() const { return m_gephi; }
    bool isDryRunSet() const { return m_dryRun; }
    bool isHelpSet() const { return m_help; }
    bool isCleanSet() const { return m_clean; }
    bool isKeepGoingSet() const { return m_keepGoing; }
    int jobs() const { return m_jobs; }
    Settings::Ptr settings() const { return m_settings; }
    QVariant configurationValue(const QString &key, const QVariant &defaultValue = QVariant());
    QList<QVariantMap> buildConfigurations() const;

private:
    void loadLocalProjectSettings(bool throwExceptionOnFailure);
    void printSettings(Settings::Scope scope);
    void exportGlobalSettings(const QString &filename);
    void importGlobalSettings(const QString &filename);
    void upgradeSettings(Settings::Scope scope);
    QString guessProjectFileName();
    QString propertyName(const QString &aCommandLineName) const;
    bool setRealProjectFile();

private:
    Settings::Ptr m_settings;
    Command m_command;
    QString m_runTargetName;
    QString m_projectFileName;
    QStringList m_searchPaths;
    QStringList m_pluginPaths;
    QStringList m_positional;
    QStringList m_runArgs;
    QStringList m_configureArgs;
    QStringList m_changedFiles;
    QStringList m_selectedProductNames;
    bool m_dumpGraph;
    bool m_gephi;
    bool m_dryRun;
    bool m_help;
    bool m_clean;
    bool m_keepGoing;
    int m_jobs;
};
}
#endif // OPTIONS_H
