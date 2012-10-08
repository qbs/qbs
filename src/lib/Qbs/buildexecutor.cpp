/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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


#include "buildexecutor.h"

#include <buildgraph/executor.h>
#include <Qbs/buildproduct.h>
#include <Qbs/buildproject.h>

namespace Qbs {

BuildExecutor::BuildExecutor()
    : m_maximumJobs(1),
      m_runOnceAndForgetModeEnabled(false),
      m_dryRun(false),
      m_keepGoing(false),
      m_state(ExecutorIdle),
      m_futureInterface(0)
{
}


void BuildExecutor::setMaximumJobs(int numberOfJobs)
{
    QWriteLocker locker(&m_lock);
    m_maximumJobs = numberOfJobs;
}

int BuildExecutor::maximumJobs() const
{
    QReadLocker locker(&m_lock);
    return m_maximumJobs;
}

void BuildExecutor::setRunOnceAndForgetModeEnabled(bool enable)
{
    QWriteLocker locker(&m_lock);
    m_runOnceAndForgetModeEnabled = enable;
}

bool BuildExecutor::runOnceAndForgetModeIsEnabled() const
{
    QReadLocker locker(&m_lock);
    return m_runOnceAndForgetModeEnabled;
}

void BuildExecutor::setDryRunEnabled(bool enable)
{
    QWriteLocker locker(&m_lock);
    m_dryRun = enable;
}

bool BuildExecutor::isDryRunEnabled() const
{
    QReadLocker locker(&m_lock);
    return m_dryRun;
}

void BuildExecutor::setKeepGoingEnabled(bool enable)
{
    QWriteLocker locker(&m_lock);
    m_keepGoing = enable;
}

bool BuildExecutor::isKeepGoingEnabled() const
{
    QReadLocker locker(&m_lock);
    return m_keepGoing;
}

void BuildExecutor::executeBuildProject(QFutureInterface<bool> &futureInterface, const BuildProject &buildProject)
{
    QVector<BuildProject> buildProjects;
    buildProjects.append(buildProject);
    QStringList changedFiles;   // ### populate
    QStringList selectedProductNames; // ### populate
    executeBuildProjects(futureInterface, buildProjects, changedFiles, selectedProductNames);
}

static QList<qbs::BuildProject::Ptr> toInternalBuildProjects(const QVector<Qbs::BuildProject > &buildProjects)
{
    QList<qbs::BuildProject::Ptr> internalBuildProjects;

    foreach (const Qbs::BuildProject &buildProject, buildProjects) {
        internalBuildProjects.append(buildProject.internalBuildProject());
    }

    return internalBuildProjects;
}

/*!
  Starts the build for the given list of projects.

  Args:
  changedFiles - absolute file names of changed files (optional)
  */
void BuildExecutor::executeBuildProjects(QFutureInterface<bool> &futureInterface,
                                         const QVector<Qbs::BuildProject> buildProjects,
                                         QStringList changedFiles,
                                         QStringList selectedProductNames)
{
    QEventLoop eventLoop;

    m_futureInterface = &futureInterface;
    m_state = ExecutorRunning;
    qbs::Executor executor(maximumJobs());
    executor.setRunOnceAndForgetModeEnabled(runOnceAndForgetModeIsEnabled());
    executor.setKeepGoing(isDryRunEnabled());
    executor.setDryRun(isDryRunEnabled());

    QObject::connect(&executor, SIGNAL(finished()), &eventLoop, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(&executor, SIGNAL(error()), &eventLoop, SLOT(quit()), Qt::QueuedConnection);

    executor.setProgressObserver(this);
    executor.build(toInternalBuildProjects(buildProjects), changedFiles, selectedProductNames);

    eventLoop.exec();

    setState(static_cast<ExecutorState>(executor.state()));
    m_buildResult = static_cast<BuildResult>(executor.buildResult());

    m_futureInterface = 0;
    if (futureInterface.resultCount() == 0)
        futureInterface.reportResult(true);
}

BuildExecutor::ExecutorState BuildExecutor::state() const
{
    QReadLocker locker(&m_lock);
    return m_state;
}

BuildExecutor::BuildResult BuildExecutor::buildResult() const
{
    return m_buildResult;
}

void BuildExecutor::setProgressRange(int minimum, int maximum)
{
    if (m_futureInterface)
        m_futureInterface->setProgressRange(minimum, maximum);
}

void BuildExecutor::setProgressValue(int value)
{
    if (m_futureInterface)
        m_futureInterface->setProgressValue(value);
}

int BuildExecutor::progressValue()
{
    return m_futureInterface ? m_futureInterface->progressValue() : 0;
}

void BuildExecutor::setState(const BuildExecutor::ExecutorState &state)
{
    QWriteLocker locker(&m_lock);
    m_state = state;
}

} // namespace Qbs
