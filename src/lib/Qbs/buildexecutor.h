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


#ifndef QBS_BUILDEXECUTOR_H
#define QBS_BUILDEXECUTOR_H

#include "buildproduct.h"
#include <tools/progressobserver.h>

#include <QFutureInterface>
#include <QReadWriteLock>

namespace qbs {
    class Executor;
    class BuildProject;
}

namespace Qbs {

class BuildExecutor : protected qbs::ProgressObserver
{
public:
    BuildExecutor();

    enum ExecutorState {
        ExecutorIdle,
        ExecutorRunning,
        ExecutorError
    };

    enum BuildResult {
        SuccessfulBuild,
        FailedBuild
    };

    void setMaximumJobs(int numberOfJobs);
    int maximumJobs() const;

    void setRunOnceAndForgetModeEnabled(bool enable);
    bool runOnceAndForgetModeIsEnabled() const;
    void setDryRunEnabled(bool enable);
    bool isDryRunEnabled() const;
    void setKeepGoingEnabled(bool enable);
    bool isKeepGoingEnabled() const;

    void executeBuildProject(QFutureInterface<bool> &futureInterface,
                             const BuildProject &buildProject);
    void executeBuildProjects(QFutureInterface<bool> &futureInterface,
                              const QVector<BuildProject> buildProjects,
                              QStringList changedFiles,
                              QStringList selectedProductNames);

    ExecutorState state() const;
    BuildResult buildResult() const;

protected:
    // Implementation of qbs::ProgressObserver
    virtual void setProgressRange(int minimum, int maximum);
    virtual void setProgressValue(int value);
    virtual int progressValue();

private: //functions
    void setState(const ExecutorState &state);

private:  // variables
    mutable QReadWriteLock m_lock;
    int m_maximumJobs;
    bool m_runOnceAndForgetModeEnabled;
    bool m_dryRun;
    bool m_keepGoing;
    ExecutorState m_state;
    BuildResult m_buildResult;
    QFutureInterface<bool> *m_futureInterface;
};

} // namespace Qbs

#endif // QBS_BUILDEXECUTOR_H
