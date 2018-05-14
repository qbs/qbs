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
#include "configcommandlineparser.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <cstdio>

using namespace qbs;
using namespace Internal;

void ConfigCommandLineParser::parse(const QStringList &commandLine)
{
    m_command = ConfigCommand();
    m_helpRequested = false;
    m_settingsDir.clear();

    m_commandLine = commandLine;
    if (m_commandLine.empty())
        throw Error(Tr::tr("No parameters supplied."));
    if (m_commandLine.size() == 1 && (m_commandLine.front() == QLatin1String("--help")
                                      || m_commandLine.front() == QLatin1String("-h"))) {
        m_helpRequested = true;
        return;
    }

    while (!m_commandLine.empty() && m_commandLine.front().startsWith(QLatin1String("--"))) {
        const QString arg = m_commandLine.takeFirst().mid(2);
        if (arg == QLatin1String("list"))
            setCommand(ConfigCommand::CfgList);
        else if (arg == QLatin1String("unset"))
            setCommand(ConfigCommand::CfgUnset);
        else if (arg == QLatin1String("export"))
            setCommand(ConfigCommand::CfgExport);
        else if (arg == QLatin1String("import"))
            setCommand(ConfigCommand::CfgImport);
        else if (arg == QLatin1String("settings-dir"))
            assignOptionArgument(arg, m_settingsDir);
        else if (arg == QLatin1String("user"))
            setScope(qbs::Settings::UserScope);
        else if (arg == QLatin1String("system"))
            setScope(qbs::Settings::SystemScope);
        else
            throw Error(Tr::tr("Unknown option for config command."));
    }

    switch (command().command) {
    case ConfigCommand::CfgNone:
        if (m_commandLine.empty())
            throw Error(Tr::tr("No parameters supplied."));
        if (m_commandLine.size() > 2)
            throw Error(Tr::tr("Too many arguments."));
        m_command.varNames << m_commandLine.front();
        if (m_commandLine.size() == 1) {
            setCommand(ConfigCommand::CfgList);
        } else {
            m_command.varValue = m_commandLine.at(1);
            setCommand(ConfigCommand::CfgSet);
        }
        break;
    case ConfigCommand::CfgUnset:
        if (m_commandLine.empty())
            throw Error(Tr::tr("Need name of variable to unset."));
        m_command.varNames = m_commandLine;
        break;
    case ConfigCommand::CfgExport:
        if (m_commandLine.size() != 1)
            throw Error(Tr::tr("Need name of file to which to export."));
        m_command.fileName = m_commandLine.front();
        break;
    case ConfigCommand::CfgImport:
        if (m_commandLine.size() != 1)
            throw Error(Tr::tr("Need name of file from which to import."));
        m_command.fileName = m_commandLine.front();
        break;
    case ConfigCommand::CfgList:
        m_command.varNames = m_commandLine;
        break;
    default:
        break;
    }
}

void ConfigCommandLineParser::setCommand(ConfigCommand::Command command)
{
    if (m_command.command != ConfigCommand::CfgNone)
        throw Error(Tr::tr("You cannot specify more than one command."));
    m_command.command = command;
}

void ConfigCommandLineParser::setScope(Settings::Scope scope)
{
    if (m_scope != qbs::Settings::allScopes())
        throw Error(Tr::tr("The --user and --system options can only appear once."));
    m_scope = scope;
}

void ConfigCommandLineParser::printUsage() const
{
    puts("Usage:\n"
        "    qbs config [--settings-dir <settings directory] <options>\n"
        "    qbs config [--settings-dir <settings directory] <key>\n"
        "    qbs config [--settings-dir <settings directory] <key> <value>"
        "\n"
         "Options:\n"
         "    --list [<root> ...] list keys under key <root> or all keys\n"
         "    --user              consider only user-level settings\n"
         "    --system            consider only system-level settings\n"
         "    --unset <name>      remove key with given name\n"
         "    --import <file>     import settings from given file\n"
         "    --export <file>     export settings to given file\n");
}

void ConfigCommandLineParser::assignOptionArgument(const QString &option, QString &argument)
{
    if (m_commandLine.empty())
        throw Error(Tr::tr("Option '%1' needs an argument.").arg(option));
    argument = m_commandLine.takeFirst();
    if (argument.isEmpty())
        throw Error(Tr::tr("Argument for option '%1' must not be empty.").arg(option));
}
