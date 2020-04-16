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
#ifndef QBS_INTERNALJOBS_H
#define QBS_INTERNALJOBS_H

#include <buildgraph/forward_decls.h>
#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/buildoptions.h>
#include <tools/cleanoptions.h>
#include <tools/installoptions.h>
#include <tools/error.h>
#include <tools/setupprojectparameters.h>

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qthread.h>

namespace qbs {
class ProcessResult;
class Settings;

namespace Internal {
class BuildGraphLoadResult;
class BuildGraphLocker;
class Executor;
class JobObserver;
class ScriptEngine;

class InternalJob : public QObject
{
    Q_OBJECT
    friend class JobObserver;
public:
    ~InternalJob() override;

    void cancel();
    virtual void start() {}
    ErrorInfo error() const { return m_error; }
    void setError(const ErrorInfo &error) { m_error = error; }

    Logger logger() const { return m_logger; }
    bool timed() const { return m_timed; }
    void shareObserverWith(InternalJob *otherJob);

protected:
    explicit InternalJob(Logger logger, QObject *parent = nullptr);

    JobObserver *observer() const { return m_observer; }
    void setTimed(bool timed) { m_timed = timed; }
    void storeBuildGraph(const TopLevelProjectPtr &project);

signals:
    void finished(Internal::InternalJob *job);
    void newTaskStarted(const QString &description, int totalEffort, Internal::InternalJob *job);
    void totalEffortChanged(int totalEffort, Internal::InternalJob *job);
    void taskProgress(int value, Internal::InternalJob *job);

private:
    ErrorInfo m_error;
    JobObserver *m_observer;
    bool m_ownsObserver;
    Logger m_logger;
    bool m_timed;
};


class InternalJobThreadWrapper : public InternalJob
{
    Q_OBJECT
public:
    InternalJobThreadWrapper(InternalJob *synchronousJob, QObject *parent = nullptr);
    ~InternalJobThreadWrapper() override;

    void start() override;
    InternalJob *synchronousJob() const { return m_job; }

signals:
    void startRequested();

private:
    void handleFinished();

    QThread m_thread;
    InternalJob *m_job;
    bool m_running;
};

class InternalSetupProjectJob : public InternalJob
{
    Q_OBJECT
public:
    InternalSetupProjectJob(const Logger &logger);
    ~InternalSetupProjectJob() override;

    void init(const TopLevelProjectPtr &existingProject, const SetupProjectParameters &parameters);
    void reportError(const ErrorInfo &error);

    TopLevelProjectPtr project() const;

private:
    void start() override;
    void resolveProjectFromScratch(Internal::ScriptEngine *engine);
    void resolveBuildDataFromScratch(const RulesEvaluationContextPtr &evalContext);
    BuildGraphLoadResult restoreProject(const RulesEvaluationContextPtr &evalContext);
    void execute();

    TopLevelProjectPtr m_existingProject;
    TopLevelProjectPtr m_newProject;
    SetupProjectParameters m_parameters;
};


class BuildGraphTouchingJob : public InternalJob
{
    Q_OBJECT
public:
    const QVector<ResolvedProductPtr> &products() const { return m_products; }
    const TopLevelProjectPtr &project() const { return m_project; }

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);

protected:
    BuildGraphTouchingJob(const Logger &logger, QObject *parent = nullptr);
    ~BuildGraphTouchingJob() override;

    void setup(const TopLevelProjectPtr &project, const QVector<ResolvedProductPtr> &products,
               bool dryRun);
    void storeBuildGraph();

private:
    TopLevelProjectPtr m_project;
    QVector<ResolvedProductPtr> m_products;
    bool m_dryRun;
};


class InternalBuildJob : public BuildGraphTouchingJob
{
    Q_OBJECT
public:
    InternalBuildJob(const Logger &logger, QObject *parent = nullptr);

    void build(const TopLevelProjectPtr &project, const QVector<ResolvedProductPtr> &products,
               const BuildOptions &buildOptions);

private:
    void handleFinished();
    void emitFinished();

    Executor *m_executor;
};


class InternalCleanJob : public BuildGraphTouchingJob
{
    Q_OBJECT
public:
    InternalCleanJob(const Logger &logger, QObject *parent = nullptr);

    void init(const TopLevelProjectPtr &project, const QVector<ResolvedProductPtr> &products,
              const CleanOptions &options);

private:
    void start() override;

    CleanOptions m_options;
};


class InternalInstallJob : public InternalJob
{
    Q_OBJECT
public:
    InternalInstallJob(const Logger &logger);
    ~InternalInstallJob() override;

    void init(const TopLevelProjectPtr &project, const QVector<ResolvedProductPtr> &products,
            const InstallOptions &options);

private:
    void start() override;

    TopLevelProjectPtr m_project;
    QVector<ResolvedProductPtr> m_products;
    InstallOptions m_options;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_INTERNALJOBS_H
