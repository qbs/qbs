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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <Qbs/globals.h>

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
    Q_DECLARE_TR_FUNCTIONS(CommandLineOptions)
public:
    CommandLineOptions();

    enum Command {
        BuildCommand,
        CleanCommand,
        StartShellCommand,
        RunCommand,
        StatusCommand,
        PropertiesCommand
    };

    void printHelp() const;
    Command command() const { return m_command;}
    bool parseCommandLine(const QStringList &args);
    const QString &runTargetName() const { return m_runTargetName; }
    const QString &projectFileName() const { return m_projectFileName; }
    const QStringList &searchPaths() const { return m_searchPaths; }
    const QStringList &pluginPaths() const { return m_pluginPaths; }
    const QStringList &positional() const { return m_positional; }
    const QStringList &runArgs() const { return m_runArgs; }
    const QStringList &changedFiles() const { return m_changedFiles; }
    const QStringList &selectedProductNames() const { return m_selectedProductNames; }
    bool isDumpGraphSet() const { return m_dumpGraph; }
    bool isDryRunSet() const { return m_dryRun; }
    bool isHelpSet() const { return m_help; }
    bool isCleanSet() const { return m_clean; }
    bool isKeepGoingSet() const { return m_keepGoing; }
    int jobs() const { return m_jobs; }
    Settings::Ptr settings() const { return m_settings; }
    QVariant configurationValue(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QList<QVariantMap> buildConfigurations() const;

private:
    void doParse();
    void parseLongOption(const QString &option);
    void parseShortOptions(const QString &options);
    QString getShortOptionArgument(const QString &options, int optionPos);
    QString getOptionArgument(const QString &option);
    QStringList getOptionArgumentAsList(const QString &option);
    void parseArgument(const QString &arg);

    QString guessProjectFileName();
    QString propertyName(const QString &aCommandLineName) const;
    void setRealProjectFile();

    Settings::Ptr m_settings;
    Command m_command;
    QString m_runTargetName;
    QString m_projectFileName;
    QStringList m_commandLine;
    QStringList m_searchPaths;
    QStringList m_pluginPaths;
    QStringList m_positional;
    QStringList m_runArgs;
    QStringList m_changedFiles;
    QStringList m_selectedProductNames;
    bool m_dumpGraph;
    bool m_dryRun;
    bool m_help;
    bool m_clean;
    bool m_keepGoing;
    int m_jobs;
    int m_logLevel;
};
}
#endif // OPTIONS_H
