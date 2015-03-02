/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#ifndef QBS_JOBS_H
#define QBS_JOBS_H

#include "project.h"
#include "../language/forward_decls.h"
#include "../tools/error.h"
#include "../tools/qbs_export.h"

#include <QObject>
#include <QProcess>
#include <QVariantMap>

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
    ~AbstractJob();

    enum State { StateRunning, StateCanceling, StateFinished };
    State state() const { return m_state; }

    ErrorInfo error() const;

public slots:
    void cancel();

protected:
    AbstractJob(Internal::InternalJob *internalJob, QObject *parent);
    Internal::InternalJob *internalJob() const { return m_internalJob; }

    bool lockProject(const Internal::TopLevelProjectPtr &project);

signals:
    void taskStarted(const QString &description, int maximumProgressValue, qbs::AbstractJob *job);
    void totalEffortChanged(int totalEffort, qbs::AbstractJob *job);
    void taskProgress(int newProgressValue, qbs::AbstractJob *job);
    void finished(bool success, qbs::AbstractJob *job);

private slots:
    void handleTaskStarted(const QString &description, int maximumProgressValue);
    void handleTotalEffortChanged(int totalEffort);
    void handleTaskProgress(int newProgressValue);
    void handleFinished();

private:
    void unlockProject();
    virtual void finish() { }

    Internal::InternalJob * const m_internalJob;
    Internal::TopLevelProjectPtr m_project;
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

    void finish();

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
               const QList<qbs::Internal::ResolvedProductPtr> &products,
               const BuildOptions &options);
};


class QBS_EXPORT CleanJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;

private:
    CleanJob(const Internal::Logger &logger, QObject *parent);

    void clean(const Internal::TopLevelProjectPtr &project,
               const QList<Internal::ResolvedProductPtr> &products, const CleanOptions &options);
};

class QBS_EXPORT InstallJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;
private:
    InstallJob(const Internal::Logger &logger, QObject *parent);

    void install(const Internal::TopLevelProjectPtr &project,
                 const QList<Internal::ResolvedProductPtr> &products, const InstallOptions &options);
};

} // namespace qbs

#endif // QBS_JOBS_H
