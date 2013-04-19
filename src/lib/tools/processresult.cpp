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
#include "processresult.h"
#include "processresult_p.h"

/*!
 * \class SetupProjectParameters
 * \brief The \c ProcessResult class describes a finished qbs process command.
 */

namespace qbs {

ProcessResult::ProcessResult() : d(new Internal::ProcessResultPrivate)
{
}

ProcessResult::ProcessResult(const ProcessResult &other) : d(other.d)
{
}

ProcessResult &ProcessResult::operator=(const ProcessResult &other)
{
    d = other.d;
    return *this;
}

ProcessResult::~ProcessResult()
{
}

/*!
 * \brief Returns true iff the command finished successfully.
 */
bool ProcessResult::success() const
{
    return d->success;
}

/*!
 * \brief Returns the file path of the executable that was run.
 */
QString ProcessResult::executableFilePath() const
{
    return d->executableFilePath;
}

/*!
 * \brief Returns the command-line arguments with which the command was invoked.
 */
QStringList ProcessResult::arguments() const
{
    return d->arguments;
}

/*!
 * \brief Returns the working directory of the invoked command.
 */
QString ProcessResult::workingDirectory() const
{
    return d->workingDirectory;
}

/*!
 * \brief Returns the exit status of the command.
 */
QProcess::ExitStatus ProcessResult::exitStatus() const
{
    return d->exitStatus;
}

/*!
 * \brief Returns the exit code of the command.
 */
int ProcessResult::exitCode() const
{
    return d->exitCode;
}

/*!
 * \brief Returns the data the command wrote to the standard output channel.
 */
QStringList ProcessResult::stdOut() const
{
    return d->stdOut;
}

/*!
 * \brief Returns the data the command wrote to the standard error channel.
 */
QStringList ProcessResult::stdErr() const
{
    return d->stdErr;
}

} // namespace qbs
