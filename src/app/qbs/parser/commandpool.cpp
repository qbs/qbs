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
#include "commandpool.h"

#include "parsercommand.h"

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
        case GenerateCommandType:
            command = new GenerateCommand(m_optionPool);
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
        case StatusCommandType:
            command = new StatusCommand(m_optionPool);
            break;
        case UpdateTimestampsCommandType:
            command = new UpdateTimestampsCommand(m_optionPool);
            break;
        case InstallCommandType:
            command = new InstallCommand(m_optionPool);
            break;
        case DumpNodesTreeCommandType:
            command = new DumpNodesTreeCommand(m_optionPool);
            break;
        case ListProductsCommandType:
            command = new ListProductsCommand(m_optionPool);
            break;
        case HelpCommandType:
            command = new HelpCommand(m_optionPool);
            break;
        case VersionCommandType:
            command = new VersionCommand(m_optionPool);
            break;
        case SessionCommandType:
            command = new SessionCommand(m_optionPool);
            break;
        }
    }
    return command;
}

} // namespace qbs
