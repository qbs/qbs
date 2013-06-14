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

    QStringList args = commandLine;
    if (args.isEmpty())
        throw ErrorInfo(Tr::tr("No parameters supplied."));
    if (args.count() == 1 && (args.first() == QLatin1String("--help")
                              || args.first() == QLatin1String("-h"))) {
        m_helpRequested = true;
        return;
    }

    while (!args.isEmpty() && args.first().startsWith("--")) {
        const QString arg = args.takeFirst().mid(2);
        if (arg == "list") {
            setCommand(ConfigCommand::CfgList);
        } else if (arg == "unset") {
            setCommand(ConfigCommand::CfgUnset);
        } else if (arg == "export") {
            setCommand(ConfigCommand::CfgExport);
        } else if (arg == "import") {
            setCommand(ConfigCommand::CfgImport);
        } else {
            throw ErrorInfo("Unknown option for config command.");
        }
    }

    switch (command().command) {
    case ConfigCommand::CfgNone:
        if (args.isEmpty())
            throw ErrorInfo(Tr::tr("No parameters supplied."));
        if (args.count() > 2)
            throw ErrorInfo("Too many arguments.");
        m_command.varNames << args.first();
        if (args.count() == 1) {
            setCommand(ConfigCommand::CfgGet);
        } else {
            m_command.varValue = args.at(1);
            setCommand(ConfigCommand::CfgSet);
        }
        break;
    case ConfigCommand::CfgUnset:
        if (args.isEmpty())
            throw ErrorInfo("Need name of variable to unset.");
        m_command.varNames = args;
        break;
    case ConfigCommand::CfgExport:
        if (args.count() != 1)
            throw ErrorInfo("Need name of file to which to export.");
        m_command.fileName = args.first();
        break;
    case ConfigCommand::CfgImport:
        if (args.count() != 1)
            throw ErrorInfo("Need name of file from which to import.");
        m_command.fileName = args.first();
        break;
    case ConfigCommand::CfgList:
        m_command.varNames = args;
        break;
    default:
        break;
    }
}

void ConfigCommandLineParser::setCommand(ConfigCommand::Command command)
{
    if (m_command.command != ConfigCommand::CfgNone)
        throw ErrorInfo("You cannot specify more than one command.");
    m_command.command = command;
}

void ConfigCommandLineParser::printUsage() const
{
    puts("Usage:\n"
        "    qbs config <options>\n"
        "    qbs config <key>\n"
        "    qbs config <key> <value>"
        "\n"
         "Options:\n"
         "    --list [<root> ...] list keys under key <root> or all keys\n"
         "    --unset <name>      remove key with given name\n"
         "    --import <file>     import settings from given file\n"
         "    --export <file>     export settings to given file\n");
}
