/****************************************************************************
**
** Copyright (C) 2015 Jake Petroules.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "commandechomode.h"

/*!
 * \enum CommandEchoMode
 * This enum type specifies the kind of output to display when executing commands.
 * \value CommandEchoModeSilent Indicates that no output will be printed.
 * \value CommandEchoModeSummary Indicates that descriptions will be printed.
 * \value CommandEchoModeCommandLine Indidcates that full command line invocations will be printed.
 */

namespace qbs {

CommandEchoMode defaultCommandEchoMode()
{
    return CommandEchoModeSummary;
}

QString commandEchoModeName(CommandEchoMode mode)
{
    switch (mode) {
    case CommandEchoModeSilent:
        return QLatin1String("silent");
    case CommandEchoModeSummary:
        return QLatin1String("summary");
    case CommandEchoModeCommandLine:
        return QLatin1String("command-line");
    default:
        break;
    }
    return QString();
}

CommandEchoMode commandEchoModeFromName(const QString &name)
{
    CommandEchoMode mode = defaultCommandEchoMode();
    for (int i = 0; i <= static_cast<int>(CommandEchoModeLast); ++i) {
        if (commandEchoModeName(static_cast<CommandEchoMode>(i)) == name) {
            mode = static_cast<CommandEchoMode>(i);
            break;
        }
    }

    return mode;
}

QStringList allCommandEchoModeStrings()
{
    QStringList result;
    for (int i = 0; i <= static_cast<int>(CommandEchoModeLast); ++i)
        result << commandEchoModeName(static_cast<CommandEchoMode>(i));
    return result;
}

} // namespace qbs
