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
 * \brief Returns the error status of the process. If no error occurred, the value
 *        is \c Process::UnknownError.
 */
QProcess::ProcessError ProcessResult::error() const
{
    return d->error;
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
