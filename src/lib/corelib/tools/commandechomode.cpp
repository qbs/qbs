/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
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

#include "commandechomode.h"

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

/*!
 * \enum CommandEchoMode
 * This enum type specifies the kind of output to display when executing commands.
 * \value CommandEchoModeSilent Indicates that no output will be printed.
 * \value CommandEchoModeSummary Indicates that descriptions will be printed.
 * \value CommandEchoModeCommandLine Indidcates that full command line invocations will be printed.
 * \value CommandEchoModeCommandLineWithEnvironment Indidcates that full command line invocations,
 * including environment variables, will be printed.
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
        return QStringLiteral("silent");
    case CommandEchoModeSummary:
        return QStringLiteral("summary");
    case CommandEchoModeCommandLine:
        return QStringLiteral("command-line");
    case CommandEchoModeCommandLineWithEnvironment:
        return QStringLiteral("command-line-with-environment");
    default:
        break;
    }
    return {};
}

CommandEchoMode commandEchoModeFromName(const QString &name)
{
    CommandEchoMode mode = defaultCommandEchoMode();
    for (int i = 0; i < CommandEchoModeInvalid; ++i) {
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
    for (int i = 0; i < CommandEchoModeInvalid; ++i)
        result << commandEchoModeName(static_cast<CommandEchoMode>(i));
    return result;
}

} // namespace qbs
