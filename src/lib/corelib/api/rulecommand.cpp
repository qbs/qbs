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

#include "rulecommand.h"
#include "rulecommand_p.h"

#include <tools/qbsassert.h>

namespace qbs {

/*!
 * \class RuleCommand
 * \brief The \c RuleCommand class corresponds to a \c ProcessCommand or \c JavaScriptCommand
 *        in \QBS.
 */

/*!
 * \enum RuleCommand::Type
 * This enum type represents the different kinds of commands.
 * \value ProcessCommandType For the \QBS type \c ProcessCommand, which represents a command
 *        whose execution involves calling an executable.
 * \value JavaScriptCommandType For the \QBS type \c JavaScriptCommand, which represents a command
 *        whose execution involves running a piece of JavaScript code inside \QBS.
 * \value InvalidType Used to mark \c RuleCommand objects as invalid.
 */


RuleCommand::RuleCommand() : d(new Internal::RuleCommandPrivate)
{
}

RuleCommand::RuleCommand(const RuleCommand &other)  = default;

RuleCommand::~RuleCommand() = default;

RuleCommand& RuleCommand::operator=(const RuleCommand &other) = default;

/*!
 * Returns the type of this object. If the value is \c InvalidType, the object is invalid.
 */
RuleCommand::Type RuleCommand::type() const
{
    return d->type;
}

/*!
 * Returns the human-readable description of this command that \QBS will print when
 * the command executed.
 */
QString RuleCommand::description() const
{
    return d->description;
}

/*!
 * Returns the detailed description of this command that \QBS will print when
 * the command is executed.
 */
QString RuleCommand::extendedDescription() const
{
    return d->extendedDescription;
}

/*!
 * Returns the source of the command if \c type() is \c JavaScriptCommandType.
 * If \c type() is anything else, the behavior of this function is undefined.
 */
QString RuleCommand::sourceCode() const
{
    QBS_ASSERT(type() == JavaScriptCommandType, return {});
    return d->sourceCode;
}

/*!
 * Returns the executable that will be called when the corresponding \c ProcessCommand
 * is executed.
 * If \c type() is not \c ProcessCommandType, the behavior of this function is undefined.
 */
QString RuleCommand::executable() const
{
    QBS_ASSERT(type() == ProcessCommandType, return {});
    return d->executable;
}

/*!
 * Returns the command-line arguments of the executable that will be called when the
 * corresponding \c ProcessCommand is executed.
 * If \c type() is not \c ProcessCommandType, the behavior of this function is undefined.
 */
QStringList RuleCommand::arguments() const
{
    QBS_ASSERT(type() == ProcessCommandType, return {});
    return d->arguments;
}

/*!
 * Returns the working directory of the executable that will be called when the
 * corresponding \c ProcessCommand is executed.
 * If \c type() is not \c ProcessCommandType, the behavior of this function is undefined.
 */
QString RuleCommand::workingDirectory() const
{
    QBS_ASSERT(type() == ProcessCommandType, return {});
    return d->workingDir;
}

/*!
 * Returns the environment of the executable that will be called when the
 * corresponding \c ProcessCommand is executed.
 * If \c type() is not \c ProcessCommandType, the behavior of this function is undefined.
 */
QProcessEnvironment RuleCommand::environment() const
{
    QBS_ASSERT(type() == ProcessCommandType, return {});
    return d->environment;
}

} // namespace qbs
