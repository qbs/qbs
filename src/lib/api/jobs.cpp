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
#include "jobs.h"

#include "internaljobs.h"
#include "project.h"
#include <language/language.h>
#include <tools/qbsassert.h>

namespace qbs {
using namespace Internal;

/*!
 * \class AbstractJob
 * \brief The \c AbstractJob class represents an operation relating to a \c Project.
 * Concrete child classes of \c AbstractJob are created by factory functions in the \c Project
 * class. The respective objects represent an operation that is started automatically
 * and is considered "running" until the \c finished() signal has been emitted. Afterwards,
 * callers can find out whether the operation was successful by calling \c hasError(). While
 * the operation is going on, progress information is being provided via \c taskStarted() and
 * \c taskProgress.
 * Note that though a job is being started automatically by its factory function, you are guaranteed
 * to recevieve all signals it emits if you connect to it right after getting the object from the
 * creating function.
 * \sa Project
 */

/*!
 * \enum AbstractJob::State
 * This enum type specifies which states a job can be in.
 * \value StateRunning The respective operation is ongoing.
 * \value StateCanceling The job has been requested to cancel via \c AbstractJob::cancel(),
 *        but the \c AbstractJob::finished() signal has not been emitted yet.
 * \value StateFinished The operation has finished and the \c AbstractJob::finished() signal
 *                      has been emitted.
 */

 /*!
  * \fn AbstractJob::State AbstractJob::state() const
  * \brief Returns the current state of the operation.
  */

 /*!
  * \fn bool AbstractJob::hasError() const
  * \brief Returns true if the operation has finished with an error, otherwise returns false.
  * This function should not be called before the \c finished() signal has been emitted.
  */

/*!
 * \fn void AbstractJob::taskStarted(const QString &description, int maximumProgressValue, qbs::AbstractJob *job)
 * \brief Indicates that a new task has been started.
 * The \a description parameter is a string intended for presentation to a user.
 * The \a maximumProgressValue parameter indicates the maximum value to which subsequent values of
 * \c taskProgress() will go.
 * This signal is typically emitted exactly once for a job that finishes successfully. However,
 * operations might emit it several times if they are made up of subtasks whose overall effort
 * cannot be determined in advance.
 * \sa AbstractJob::taskProgress()
 */

/*!
 * \fn void taskProgress(int newProgressValue, qbs::AbstractJob *job)
 * \brief Indicates progress in executing the operation.
 * The \a newProgressValue parameter represents the current progress. It is always greater than
 * zero, strictly increasing and goes up to the \c maximumProgressValue argument of the last
 * call to \c taskStarted().
 * \sa AbstractJob::taskStarted()
 */

 /*!
  * \fn void finished(bool success, qbs::AbstractJob *job)
  * \brief Indicates that the operation has finished.
  * Check the \a success parameter to find out whether everything went fine or an error occurred.
  */

AbstractJob::AbstractJob(InternalJob *internalJob, QObject *parent)
    : QObject(parent), m_internalJob(internalJob)
{
    m_internalJob->setParent(this);
    connect(m_internalJob, SIGNAL(newTaskStarted(QString,int,Internal::InternalJob*)),
            SLOT(handleTaskStarted(QString,int)), Qt::QueuedConnection);
    connect(m_internalJob, SIGNAL(totalEffortChanged(int,Internal::InternalJob*)),
            SLOT(handleTotalEffortChanged(int)));
    connect(m_internalJob, SIGNAL(taskProgress(int,Internal::InternalJob*)),
            SLOT(handleTaskProgress(int)), Qt::QueuedConnection);
    connect(m_internalJob, SIGNAL(finished(Internal::InternalJob *)), SLOT(handleFinished()));
    m_state = StateRunning;
}

/*!
 * \brief Destroys the object, canceling the operation if necessary.
 */
AbstractJob::~AbstractJob()
{
    m_internalJob->disconnect(this);
    cancel();
}

/*!
 * \brief Returns the error which caused this operation to fail, if it did fail.
 */
ErrorInfo AbstractJob::error() const
{
    return internalJob()->error();
}

/*!
 * \brief Cancels this job.
 * Note that the job might not finish immediately. If you need to make sure it has actually
 * finished, wait for the \c finished() signal.
 * \sa AbstractJob::finished(AbstractJob *);
 */
void AbstractJob::cancel()
{
    if (m_state != StateRunning)
        return;
    m_state = StateCanceling;
    internalJob()->cancel();
}

void AbstractJob::handleTaskStarted(const QString &description, int maximumProgressValue)
{
    emit taskStarted(description, maximumProgressValue, this);
}

void AbstractJob::handleTotalEffortChanged(int totalEffort)
{
    emit totalEffortChanged(totalEffort, this);
}

void AbstractJob::handleTaskProgress(int newProgressValue)
{
    emit taskProgress(newProgressValue, this);
}

void AbstractJob::handleFinished()
{
    QBS_ASSERT(m_state != StateFinished, return);
    m_state = StateFinished;
    emit finished(!error().hasError(), this);
}


/*!
 * \class SetupProjectJob
 * \brief The \c SetupProjectJob class represents an operation that reads a qbs project file and
 * creates a \c Project object from it.
 * Note that this job can emit the \c taskStarted() signal more than once.
 * \sa AbstractJob::taskStarted()
 */

SetupProjectJob::SetupProjectJob(const Logger &logger, QObject *parent)
    : AbstractJob(new InternalJobThreadWrapper(new InternalSetupProjectJob(logger)), parent)
{
}

/*!
 * \brief Returns the project resulting from this operation.
 * Note that the result is undefined if the job did not finish successfully.
 * \sa AbstractJob::hasError()
 */
Project SetupProjectJob::project() const
{
    const InternalJobThreadWrapper * const wrapper
            = qobject_cast<InternalJobThreadWrapper *>(internalJob());
    const InternalSetupProjectJob * const job
            = qobject_cast<InternalSetupProjectJob *>(wrapper->synchronousJob());
    return Project(job->project(), job->logger());
}

void SetupProjectJob::resolve(const SetupProjectParameters &parameters)
{
    InternalJobThreadWrapper * const wrapper
            = qobject_cast<InternalJobThreadWrapper *>(internalJob());
    InternalSetupProjectJob * const job
            = qobject_cast<InternalSetupProjectJob *>(wrapper->synchronousJob());
    job->init(parameters);
    wrapper->start();
}

void SetupProjectJob::reportError(const ErrorInfo &error)
{
    InternalJobThreadWrapper * const wrapper
            = qobject_cast<InternalJobThreadWrapper *>(internalJob());
    InternalSetupProjectJob * const job
            = qobject_cast<InternalSetupProjectJob *>(wrapper->synchronousJob());
    job->reportError(error);
}

/*!
 * \class ProcessResult
 * \brief The \c ProcessResult class represents the result of one external program run by Qbs.
 *
 * The \c ProcessResult class represents all the information on one external program that was
 * run by Qbs. It includes the command line used to start the program, the working directory
 * as well as output and exit codes.
 */

/*!
 * \class BuildJob
 * \brief The \c BuildJob class represents a build operation.
 */

/*!
 * \fn void BuildJob::reportCommandDescription(const QString &highlight, const QString &message)
 * \brief Signals that a new command is being worked on.
 * The \a highlight parameter is used to decide on the colors and font styles to be used to
 * print the message.
 * The \a message parameter is the localized message to print.
 */

/*!
 * \fn void BuildJob::reportProcessResult(const qbs::ProcessResult &result)
 * \brief Signals that an external command has finished.
 * The \a result parameter contains all details on the process that was run by Qbs.
 */

BuildJob::BuildJob(const Logger &logger, QObject *parent)
    : AbstractJob(new InternalBuildJob(logger), parent)
{
    InternalBuildJob *job = static_cast<InternalBuildJob *>(internalJob());
    connect(job, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    connect(job, SIGNAL(reportProcessResult(qbs::ProcessResult)),
            this, SIGNAL(reportProcessResult(qbs::ProcessResult)));
}

void BuildJob::build(const TopLevelProjectPtr &project, const QList<ResolvedProductPtr> &products,
                     const BuildOptions &options)
{
    qobject_cast<InternalBuildJob *>(internalJob())->build(project, products, options);
}


/*!
 * \class CleanJob
 * \brief The \c CleanJob class represents an operation removing build artifacts.
 */

CleanJob::CleanJob(const Logger &logger, QObject *parent)
    : AbstractJob(new InternalJobThreadWrapper(new InternalCleanJob(logger)), parent)
{
}

void CleanJob::clean(const TopLevelProjectPtr &project, const QList<ResolvedProductPtr> &products,
                     const qbs::CleanOptions &options)
{
    InternalJobThreadWrapper * wrapper = qobject_cast<InternalJobThreadWrapper *>(internalJob());
    qobject_cast<InternalCleanJob *>(wrapper->synchronousJob())->init(project, products, options);
    wrapper->start();
}

/*!
 * \class InstallJob
 * \brief The \c InstallJob class represents an operation installing files.
 */

InstallJob::InstallJob(const Logger &logger, QObject *parent)
    : AbstractJob(new InternalJobThreadWrapper(new InternalInstallJob(logger)), parent)
{
}

void InstallJob::install(const TopLevelProjectPtr &project,
                         const QList<ResolvedProductPtr> &products, const InstallOptions &options)
{
    InternalJobThreadWrapper *wrapper = qobject_cast<InternalJobThreadWrapper *>(internalJob());
    InternalInstallJob *installJob = qobject_cast<InternalInstallJob *>(wrapper->synchronousJob());
    installJob->init(project, products, options);
    wrapper->start();
}

} // namespace qbs
