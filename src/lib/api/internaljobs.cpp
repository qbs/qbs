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
#include "internaljobs.h"

#include <buildgraph/artifactcleaner.h>
#include <buildgraph/buildproduct.h>
#include <buildgraph/buildproject.h>
#include <buildgraph/executor.h>
#include <buildgraph/rulesevaluationcontext.h>
#include <language/language.h>
#include <language/loader.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/progressobserver.h>
#include <tools/settings.h>

#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QScopedPointer>
#include <QTimer>

namespace qbs {
namespace Internal {

class JobObserver : public ProgressObserver
{
public:
    JobObserver(InternalJob *job) : m_canceled(false), m_job(job) { }

    void cancel() { m_canceled = true; }

private:
    void initialize(const QString &task, int maximum) {
        m_value = 0;
        m_maximum = maximum;
        m_canceled = false;
        emit m_job->newTaskStarted(task, maximum, m_job);
    }
    void setProgressValue(int value) {
        m_value = value;
        emit m_job->taskProgress(value, m_job);
    }
    int progressValue() { return m_value; }
    int maximum() const { return m_maximum; }
    bool canceled() const { return m_canceled; }

    int m_value;
    int m_maximum;
    bool m_canceled;
    InternalJob * const m_job;
};


InternalJob::InternalJob(QObject *parent)
    : QObject(parent)
    , m_observer(new JobObserver(this))
{
}

void InternalJob::cancel()
{
    m_observer->cancel();
}


InternalSetupProjectJob::InternalSetupProjectJob(QObject *parent)
    : InternalJob(parent), m_running(false)
{
}

InternalSetupProjectJob::~InternalSetupProjectJob()
{
    QMutexLocker locker(&m_runMutex);
    while (m_running)
        m_runWaitCondition.wait(&m_runMutex);
 }

void InternalSetupProjectJob::resolve(const QString &projectFilePath, const QString &buildRoot,
                         const QVariantMap &buildConfig)
{
    m_projectFilePath = projectFilePath;
    m_buildRoot = buildRoot;
    m_buildConfig = buildConfig;
    QTimer::singleShot(0, this, SLOT(start()));
}

BuildProjectPtr InternalSetupProjectJob::buildProject() const
{
    return m_buildProject;
}

void InternalSetupProjectJob::start()
{
    m_running = true;
    QFutureWatcher<void> * const watcher = new QFutureWatcher<void>(this);
    connect(watcher, SIGNAL(finished()), SLOT(handleFinished()));
    watcher->setFuture(QtConcurrent::run(this, &InternalSetupProjectJob::doResolve));
}

void InternalSetupProjectJob::handleFinished()
{
    emit finished(this);
}

void InternalSetupProjectJob::doResolve()
{
    try {
        execute();
    } catch (const Error &error) {
        setError(error);
    }
    QMutexLocker locker(&m_runMutex);
    m_running = false;
    m_runWaitCondition.wakeOne();
}

void InternalSetupProjectJob::execute()
{
    QScopedPointer<RulesEvaluationContext> evalContext(new RulesEvaluationContext);
    evalContext->setObserver(observer());
    const QStringList searchPaths = Settings().searchPaths();
    const BuildProjectLoader::LoadResult loadResult = BuildProjectLoader().load(m_projectFilePath,
            evalContext.data(), m_buildRoot, m_buildConfig, searchPaths);

    ResolvedProjectPtr rProject;
    if (!loadResult.discardLoadedProject)
        m_buildProject = loadResult.loadedProject;
    if (m_buildProject) {
        evalContext.take();
        rProject = m_buildProject->resolvedProject();
    } else {
        if (loadResult.changedResolvedProject) {
            rProject = loadResult.changedResolvedProject;
        } else {
            Loader loader(evalContext->engine());
            loader.setSearchPaths(searchPaths);
            loader.setProgressObserver(observer());
            rProject = loader.loadProject(m_projectFilePath, m_buildRoot, m_buildConfig);
        }
        if (rProject->products.isEmpty())
            throw Error(Tr::tr("Project '%1' does not contain products.").arg(m_projectFilePath));
    }

    // copy the environment from the platform config into the project's config
    const QVariantMap platformEnvironment = m_buildConfig.value("environment").toMap();
    rProject->platformEnvironment = platformEnvironment;

    qbsDebug("for %s:", qPrintable(rProject->id()));
    foreach (const ResolvedProductConstPtr &p, rProject->products) {
        qbsDebug("  - [%s] %s as %s"
                 ,qPrintable(p->fileTags.join(", "))
                 ,qPrintable(p->name)
                 ,qPrintable(p->project->id())
                 );
    }
    qbsDebug("");

    if (m_buildProject)
        return;

    TimedActivityLogger resolveLogger(QLatin1String("Resolving build project"));
    m_buildProject = BuildProjectResolver().resolveProject(rProject, evalContext.data());
    if (loadResult.loadedProject)
        m_buildProject->rescueDependencies(loadResult.loadedProject);
    evalContext.take();
}


BuildGraphTouchingJob::BuildGraphTouchingJob(QObject *parent) : InternalJob(parent)
{
}

BuildGraphTouchingJob::~BuildGraphTouchingJob()
{
}

void BuildGraphTouchingJob::setup(const QList<BuildProductPtr> &products,
                                  const BuildOptions &buildOptions)
{
    m_products = products;
    m_buildOptions = buildOptions;
}

void BuildGraphTouchingJob::storeBuildGraph()
{
    try {
        if (m_buildOptions.dryRun)
            return;
        m_products.first()->project->store();
    } catch (const Error &error) {
        qbsWarning() << error.toString();
    }
}


InternalBuildJob::InternalBuildJob(QObject *parent) : BuildGraphTouchingJob(parent)
{
}

void InternalBuildJob::build(const QList<BuildProductPtr> &products,
                     const BuildOptions &buildOptions)
{
    setup(products, buildOptions);
    QTimer::singleShot(0, this, SLOT(start()));
}

void InternalBuildJob::start()
{
    m_executor = new Executor(this);
    connect(m_executor, SIGNAL(finished()), SLOT(handleFinished()));
    m_executor->setBuildOptions(buildOptions());
    m_executor->setProgressObserver(observer());
    m_executor->build(products());
}

void InternalBuildJob::handleFinished()
{
    storeBuildGraph();
    if (m_executor->hasError())
        setError(m_executor->error());
    emit finished(this);
}


InternalCleanJob::InternalCleanJob(QObject *parent) : BuildGraphTouchingJob(parent)
{
}

void InternalCleanJob::clean(const QList<BuildProductPtr> &products, const BuildOptions &buildOptions,
                     bool cleanAll)
{
    setup(products, buildOptions);
    m_cleanAll = cleanAll;
    QTimer::singleShot(0, this, SLOT(start()));
}

void InternalCleanJob::start()
{
    QFutureWatcher<void> * const watcher = new QFutureWatcher<void>(this);
    connect(watcher, SIGNAL(finished()), SLOT(handleFinished()));
    watcher->setFuture(QtConcurrent::run(this, &InternalCleanJob::doClean));
}

void InternalCleanJob::handleFinished()
{
    emit finished(this);
}

void InternalCleanJob::doClean()
{
    try {
        ArtifactCleaner cleaner;
        cleaner.cleanup(products(), m_cleanAll, buildOptions());
    } catch (const Error &error) {
        setError(error);
    }
    storeBuildGraph();
}


ErrorJob::ErrorJob(QObject *parent) : InternalJob(parent)
{
}

void ErrorJob::reportError(const Error &error)
{
    setError(error);
    QTimer::singleShot(0, this, SLOT(handleFinished()));
}

void ErrorJob::handleFinished()
{
    emit finished(this);
}

} // namespace Internal
} // namespace qbs
