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
#ifndef QBS_JOBS_H
#define QBS_JOBS_H

#include <buildgraph/forward_decls.h>
#include <tools/error.h>
#include <tools/processresult.h>

#include <QObject>
#include <QProcess>
#include <QVariantMap>

namespace qbs {
class BuildOptions;
class InstallOptions;
class Settings;
class SetupProjectParameters;
namespace Internal {
class InternalJob;
class ProjectPrivate;
} // namespace Internal

class Project;

class AbstractJob : public QObject
{
    Q_OBJECT
public:
    ~AbstractJob();

    enum State { StateRunning, StateCanceling, StateFinished };
    State state() const { return m_state; }

    Error error() const;
    bool hasError() const { return !error().entries().isEmpty(); }

    void cancel();

protected:
    AbstractJob(Internal::InternalJob *internalJob, QObject *parent);
    Internal::InternalJob *internalJob() const { return m_internalJob; }

signals:
    void taskStarted(const QString &description, int maximumProgressValue, qbs::AbstractJob *job);
    void taskProgress(int newProgressValue, qbs::AbstractJob *job);
    void finished(bool success, qbs::AbstractJob *job);

private slots:
    void handleTaskStarted(const QString &description, int maximumProgressValue);
    void handleTaskProgress(int newProgressValue);
    void handleFinished();

private:
    Internal::InternalJob * const m_internalJob;
    State m_state;
};


class SetupProjectJob : public AbstractJob
{
    Q_OBJECT
    friend class Project;
public:
    Project project() const;

private:
    SetupProjectJob(Settings *settings, QObject *parent);

    void resolve(const SetupProjectParameters &parameters);
};

class BuildJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);
    void reportWarning(const qbs::Error &warning);

private:
    BuildJob(QObject *parent);

    void build(const QList<Internal::BuildProductPtr> &products, const BuildOptions &options,
               const QProcessEnvironment &env);
};


class CleanJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;

private:
    CleanJob(QObject *parent);

    void clean(const QList<Internal::BuildProductPtr> &products, const BuildOptions &options,
               bool cleanAll);
};

class InstallJob : public AbstractJob
{
    Q_OBJECT
    friend class Internal::ProjectPrivate;
private:
    InstallJob(QObject *parent);

    void install(const QList<Internal::BuildProductPtr> &products, const InstallOptions &options);
};

} // namespace qbs

#endif // QBS_JOBS_H
