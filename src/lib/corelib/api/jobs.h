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
#ifndef QBS_JOBS_H
#define QBS_JOBS_H

#include "project.h"
#include "../language/forward_decls.h"
#include "../tools/error.h"
#include "../tools/qbs_export.h"

#include <QtCore/qobject.h>
#include <QtCore/qprocess.h>
#include <QtCore/qvariant.h>

namespace qbs {
class BuildOptions;
class CleanOptions;
class InstallOptions;
class ProcessResult;
class SetupProjectParameters;
namespace Internal {
class InternalJob;
class Logger;
class ProjectPrivate;
} // namespace Internal

class Project;

class QBS_EXPORT AbstractJob : public QObject
{
    Q_OBJECT
public:
    ~AbstractJob() override;

    enum State { StateRunning, StateCanceling, StateFinished };
    State state() const { return m_state; }

    ErrorInfo error() const;

public slots:
    void cancel();

protected:
    AbstractJob(Internal::InternalJob *internalJob, QObject *parent);
    Internal::InternalJob *internalJob() const { return m_internalJob; }

    bool lockProject(const Internal::TopLevelProjectPtr &project);
    void setError(const ErrorInfo &error) { m_error = error; }

signals:
    void taskStarted(const QString &description, int maximumProgressValue, qbs::AbstractJob *job);
    void totalEffortChanged(int totalEffort, qbs::AbstractJob *job);
    void taskProgress(int newProgressValue, qbs::AbstractJob *job);
    void finished(bool success, qbs::AbstractJob *job);

private:
    void handleTaskStarted(const QString &description, int maximumProgressValue);
    void handleTotalEffortChanged(int totalEffort);
    void handleTaskProgress(int newProgressValue);
    void handleFinished();

    void unlockProject();
    virtual void finish() { }

    Internal::InternalJob * const m_internalJob;
    Internal::TopLevelProjectPtr m_project;
    ErrorInfo m_error;
    State m_state;
};


class QBS_EXPORT SetupProjectJob : public AbstractJob
{
    Q_OBJECT
    friend class Project;
public:
    Project project() const;

private:
    SetupProjectJob(const Internal::Logger &logger, QObject *parent);

    void resolve(const Project &existingProject, const SetupProjectParameters &parameters);
    void reportError(const ErrorInfo &error);

    void finish() override;

    Project m_existingProject;
};

class QBS_EXPORT BuildJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);

private:
    BuildJob(const Internal::Logger &logger, QObject *parent);

    void build(const Internal::TopLevelProjectPtr &project,
               const QVector<Internal::ResolvedProductPtr> &products,
               const BuildOptions &options);
    void handleLauncherError(const ErrorInfo &error);

    void finish() override;
};


class QBS_EXPORT CleanJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;

private:
    CleanJob(const Internal::Logger &logger, QObject *parent);

    void clean(const Internal::TopLevelProjectPtr &project,
               const QVector<Internal::ResolvedProductPtr> &products, const CleanOptions &options);
};

class QBS_EXPORT InstallJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;
private:
    InstallJob(const Internal::Logger &logger, QObject *parent);

    void install(const Internal::TopLevelProjectPtr &project,
                 const QVector<Internal::ResolvedProductPtr> &products,
                 const InstallOptions &options);
};

} // namespace qbs

#endif // QBS_JOBS_H
