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
#include "progressobserver.h"

namespace qbs {
namespace Internal {

/*!
 * \class ProgressObserver
 * The \c ProgressObserver class is used in long running qbs operations. It serves two purposes:
 * Firstly, it allows operations to indicate progress to a client. Secondly, a client can
 * signal to an operation that is should exit prematurely.
 * Clients of the qbs library are supposed to subclass this class and implement the virtual
 * functions in a way that lets users know about the current operation and its progress.
 */

/*!
 * \fn virtual void initialize(const QString &task, int maximum) = 0
 * \brief Indicates that a new operation is starting.
 * Library code calls this function to indicate that it is starting a new task.
 * The \a task parameter is a textual description of that task suitable for presentation to a user.
 * The \a maximum parameter is an estimate of the maximum effort the operation is going to take.
 * This is helpful if the client wants to set up some sort of progress bar showing the
 * percentage of the work already done.
 */

/*!
 * \fn virtual void setProgressValue(int value) = 0
 * \brief Sets the new progress value.
 * Library code calls this function to indicate that the current operation has progressed.
 * It will try hard to ensure that \a value will not exceed \c maximum().
 * \sa ProgressObserver::maximum().
 */

/*!
 * \fn virtual int progressValue() = 0
 * \brief The current progress value.
 * Will typically reflect the \a value from the last call to \c setProgressValue() and should not
 * exceed \c maximum().
 * \sa setProgressvalue()
 * \sa maximum()
 */

void ProgressObserver::incrementProgressValue(int increment)
{
    setProgressValue(progressValue() + increment);
}

/*!
 * \fn virtual bool canceled() const = 0
 * \brief Indicates whether the current operation should be canceled.
 * Library code will periodically call this function and abort the current operation
 * if it returns true.
 */

/*!
 * \fn virtual int maximum() const = 0
 * \brief The expected maximum progress value.
 * This will typically be the value of \c maximum passed to \c initialize().
 * \sa ProgressObserver::initialize()
 */

void ProgressObserver::setFinished()
{
    setProgressValue(maximum());
}

} // namespace Internal
} // namespace qbs
