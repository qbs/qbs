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
#include "commandpool.h"

#include "command.h"

namespace qbs {

CommandPool::CommandPool(CommandLineOptionPool &optionPool) : m_optionPool(optionPool)
{
}

CommandPool::~CommandPool()
{
    qDeleteAll(m_commands);
}

qbs::Command *CommandPool::getCommand(CommandType type) const
{
    Command *& command = m_commands[type];
    if (!command) {
        switch (type) {
        case ResolveCommandType:
            command = new ResolveCommand(m_optionPool);
            break;
        case BuildCommandType:
            command = new BuildCommand(m_optionPool);
            break;
        case CleanCommandType:
            command = new CleanCommand(m_optionPool);
            break;
        case RunCommandType:
            command = new RunCommand(m_optionPool);
            break;
        case ShellCommandType:
            command = new ShellCommand(m_optionPool);
            break;
        case PropertiesCommandType:
            command = new PropertiesCommand(m_optionPool);
            break;
        case StatusCommandType:
            command = new StatusCommand(m_optionPool);
            break;
        case UpdateTimestampsCommandType:
            command = new UpdateTimestampsCommand(m_optionPool);
            break;
        case InstallCommandType:
            command = new InstallCommand(m_optionPool);
            break;
        case HelpCommandType:
            command = new HelpCommand(m_optionPool);
            break;
        }
    }
    return command;
}

} // namespace qbs
