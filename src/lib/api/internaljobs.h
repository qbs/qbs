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
#ifndef QBS_INTERNALJOBS_H
#define QBS_INTERNALJOBS_H

#include "projectdata.h"
#include <buildgraph/forward_decls.h>
#include <tools/buildoptions.h>
#include <tools/error.h>

#include <QMutex>
#include <QObject>
#include <QProcessEnvironment>
#include <QWaitCondition>

namespace qbs {
class ProcessResult;

namespace Internal {
class Executor;
class JobObserver;

class InternalJob : public QObject
{
    Q_OBJECT
    friend class JobObserver;
public:
    void cancel();
    Error error() const { return m_error; }
    bool hasError() const { return !error().entries().isEmpty(); }

protected:
    explicit InternalJob(QObject *parent = 0);

    JobObserver *observer() const { return m_observer; }
    void setError(const Error &error) { m_error = error; }
    void storeBuildGraph(const BuildProject *buildProject);

signals:
    void finished(Internal::InternalJob *job);
    void newTaskStarted(const QString &description, int totalEffort, Internal::InternalJob *job);
    void taskProgress(int value, Internal::InternalJob *job);

private:
    Error m_error;
    JobObserver * const m_observer;
};


class InternalSetupProjectJob : public InternalJob
{
    Q_OBJECT
public:
    InternalSetupProjectJob(QObject *parent = 0);
    ~InternalSetupProjectJob();

    void resolve(const QString &projectFilePath, const QString &buildRoot,
                 const QVariantMap &buildConfig);

    BuildProjectPtr buildProject() const;

private slots:
    void start();
    void handleFinished();

private:
    void doResolve();
    void execute();

    bool m_running;
    QMutex m_runMutex;
    QWaitCondition m_runWaitCondition;

    QString m_projectFilePath;
    QString m_buildRoot;
    QVariantMap m_buildConfig;
    BuildProjectPtr m_buildProject;
};


class BuildGraphTouchingJob : public InternalJob
{
    Q_OBJECT
public:
    const QList<BuildProductPtr> &products() const { return m_products; }

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);
    void reportWarning(const qbs::CodeLocation &location, const QString &message);

protected:
    BuildGraphTouchingJob(QObject *parent = 0);
    ~BuildGraphTouchingJob();

    void setup(const QList<BuildProductPtr> &products, const BuildOptions &buildOptions);
    const BuildOptions &buildOptions() const { return m_buildOptions; }
    void storeBuildGraph();

private:
    QList<BuildProductPtr> m_products;
    BuildOptions m_buildOptions;
};


class InternalBuildJob : public BuildGraphTouchingJob
{
    Q_OBJECT
public:
    InternalBuildJob(QObject *parent = 0);

    void build(const QList<BuildProductPtr> &products, const BuildOptions &buildOptions,
               const QProcessEnvironment &env);

private slots:
    void start();
    void handleFinished();

private:
    Executor *m_executor;
    QProcessEnvironment m_environment;
};


class InternalCleanJob : public BuildGraphTouchingJob
{
    Q_OBJECT
public:
    InternalCleanJob(QObject *parent = 0);

    void clean(const QList<BuildProductPtr> &products, const BuildOptions &buildOptions,
               bool cleanAll);

private slots:
    void start();
    void handleFinished();

private:
    void doClean();

    bool m_cleanAll;
};


class ErrorJob : public InternalJob
{
    Q_OBJECT
public:
    ErrorJob(QObject *parent);

    void reportError(const Error &error);

private slots:
    void handleFinished();
};

} // namespace Internal
} // namespace qbs

#endif // QBS_INTERNALJOBS_H
