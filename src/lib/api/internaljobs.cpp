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
#include "internaljobs.h"

#include "jobs.h"

#include <buildgraph/artifactcleaner.h>
#include <buildgraph/buildgraphloader.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/projectbuilddata.h>
#include <buildgraph/executor.h>
#include <buildgraph/productinstaller.h>
#include <buildgraph/rulesevaluationcontext.h>
#include <language/language.h>
#include <language/loader.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/progressobserver.h>
#include <tools/preferences.h>
#include <tools/qbsassert.h>

#include <QEventLoop>
#include <QScopedPointer>
#include <QTimer>

namespace qbs {
namespace Internal {

class JobObserver : public ProgressObserver
{
public:
    JobObserver(InternalJob *job) : m_canceled(false), m_job(job), m_timedLogger(0) { }
    ~JobObserver() { delete m_timedLogger; }

    void cancel() { m_canceled = true; }

private:
    void initialize(const QString &task, int maximum)
    {
        QBS_ASSERT(!m_timedLogger, delete m_timedLogger);
        m_timedLogger = new TimedActivityLogger(m_job->logger(), task, QString(),
                m_job->timed() ? LoggerInfo : LoggerDebug, m_job->timed());
        m_value = 0;
        m_maximum = maximum;
        m_canceled = false;
        emit m_job->newTaskStarted(task, maximum, m_job);
    }

    void setMaximum(int maximum)
    {
        m_maximum = maximum;
        emit m_job->totalEffortChanged(maximum, m_job);
    }

    void setProgressValue(int value)
    {
        //QBS_ASSERT(value >= m_value, qDebug("old value = %d, new value = %d", m_value, value));
        //QBS_ASSERT(value <= m_maximum, qDebug("value = %d, maximum = %d", value, m_maximum));
        m_value = value;
        if (value == m_maximum) {
            delete m_timedLogger;
            m_timedLogger = 0;
        }
        emit m_job->taskProgress(value, m_job);
    }

    int progressValue() { return m_value; }
    int maximum() const { return m_maximum; }
    bool canceled() const { return m_canceled; }

    int m_value;
    int m_maximum;
    bool m_canceled;
    InternalJob * const m_job;
    TimedActivityLogger *m_timedLogger;
};


InternalJob::InternalJob(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_observer(new JobObserver(this))
    , m_ownsObserver(true)
    , m_logger(logger)
    , m_timed(false)
{
}

InternalJob::~InternalJob()
{
    if (m_ownsObserver)
        delete m_observer;
}

void InternalJob::cancel()
{
    m_observer->cancel();
}

void InternalJob::shareObserverWith(InternalJob *otherJob)
{
    if (m_ownsObserver) {
        delete m_observer;
        m_ownsObserver = false;
    }
    m_observer = otherJob->m_observer;
}

void InternalJob::storeBuildGraph(const TopLevelProjectConstPtr &project)
{
    try {
        project->store(logger());
    } catch (const ErrorInfo &error) {
        logger().printWarning(error);
    }
}


/**
 * Construct a new thread wrapper for a synchronous job.
 * This object takes over ownership of the synchronous job.
 */
InternalJobThreadWrapper::InternalJobThreadWrapper(InternalJob *synchronousJob, QObject *parent)
    : InternalJob(synchronousJob->logger(), parent)
    , m_job(synchronousJob)
    , m_running(false)
{
    synchronousJob->shareObserverWith(this);
    m_job->moveToThread(&m_thread);
    connect(m_job, SIGNAL(finished(Internal::InternalJob*)), SLOT(handleFinished()));
    connect(m_job, SIGNAL(newTaskStarted(QString,int,Internal::InternalJob*)),
            SIGNAL(newTaskStarted(QString,int,Internal::InternalJob*)));
    connect(m_job, SIGNAL(taskProgress(int,Internal::InternalJob*)),
            SIGNAL(taskProgress(int,Internal::InternalJob*)));
    connect(m_job, SIGNAL(totalEffortChanged(int,Internal::InternalJob*)),
            SIGNAL(totalEffortChanged(int,Internal::InternalJob*)));
    m_job->connect(this, SIGNAL(startRequested()), SLOT(start()));
}

InternalJobThreadWrapper::~InternalJobThreadWrapper()
{
    if (m_running) {
        QEventLoop loop;
        loop.connect(m_job, SIGNAL(finished(Internal::InternalJob*)), SLOT(quit()));
        cancel();
        loop.exec();
    }
    m_thread.quit();
    m_thread.wait();
    delete m_job;
}

void InternalJobThreadWrapper::start()
{
    setTimed(m_job->timed());
    m_thread.start();
    m_running = true;
    emit startRequested();
}

void InternalJobThreadWrapper::handleFinished()
{
    m_running = false;
    setError(m_job->error());
    emit finished(this);
}


InternalSetupProjectJob::InternalSetupProjectJob(const Logger &logger)
    : InternalJob(logger)
{
}

InternalSetupProjectJob::~InternalSetupProjectJob()
{
}

void InternalSetupProjectJob::init(const SetupProjectParameters &parameters)
{
    m_parameters = parameters;
    setTimed(parameters.logElapsedTime());
}

void InternalSetupProjectJob::reportError(const ErrorInfo &error)
{
    setError(error);
    emit finished(this);
}

TopLevelProjectPtr InternalSetupProjectJob::project() const
{
    return m_project;
}

void InternalSetupProjectJob::start()
{
    try {
        execute();
    } catch (const ErrorInfo &error) {
        setError(error);
    }
    emit finished(this);
}

void InternalSetupProjectJob::execute()
{
    RulesEvaluationContextPtr evalContext(new RulesEvaluationContext(logger()));
    evalContext->setObserver(observer());

    switch (m_parameters.restoreBehavior()) {
    case SetupProjectParameters::ResolveOnly:
        resolveProjectFromScratch(evalContext->engine());
        resolveBuildDataFromScratch(evalContext);
        setupPlatformEnvironment();
        break;
    case SetupProjectParameters::RestoreOnly:
        m_project = restoreProject(evalContext).loadedProject;
        break;
    case SetupProjectParameters::RestoreAndTrackChanges: {
        const BuildGraphLoadResult loadResult = restoreProject(evalContext);
        m_project = loadResult.newlyResolvedProject;
        if (!m_project && !loadResult.discardLoadedProject)
            m_project = loadResult.loadedProject;
        if (!m_project)
            resolveProjectFromScratch(evalContext->engine());
        if (!m_project->buildData) {
            resolveBuildDataFromScratch(evalContext);
            if (loadResult.loadedProject)
                BuildDataResolver::rescueBuildData(loadResult.loadedProject, m_project, logger());
        }
        setupPlatformEnvironment();
        break;
    }
    }

    if (!m_parameters.dryRun())
        storeBuildGraph(m_project);

    // The evalutation context cannot be re-used for building, which runs in a different thread.
    m_project->buildData->evaluationContext.clear();
}

void InternalSetupProjectJob::resolveProjectFromScratch(ScriptEngine *engine)
{
    Loader loader(engine, logger());
    loader.setSearchPaths(m_parameters.searchPaths());
    loader.setProgressObserver(observer());
    m_project = loader.loadProject(m_parameters);
}

void InternalSetupProjectJob::resolveBuildDataFromScratch(const RulesEvaluationContextPtr &evalContext)
{
    TimedActivityLogger resolveLogger(logger(), QLatin1String("Resolving build project"));
    BuildDataResolver(logger()).resolveBuildData(m_project, evalContext);
}

void InternalSetupProjectJob::setupPlatformEnvironment()
{
    const QVariantMap platformEnvironment
            = m_parameters.buildConfiguration().value(QLatin1String("environment")).toMap();
    m_project->platformEnvironment = platformEnvironment;
}

BuildGraphLoadResult InternalSetupProjectJob::restoreProject(const RulesEvaluationContextPtr &evalContext)
{
    BuildGraphLoader bgLoader(m_parameters.environment(), logger());
    const BuildGraphLoadResult loadResult = bgLoader.load(m_parameters, evalContext);
    return loadResult;
}

BuildGraphTouchingJob::BuildGraphTouchingJob(const Logger &logger, QObject *parent)
    : InternalJob(logger, parent), m_dryRun(false)
{
}

BuildGraphTouchingJob::~BuildGraphTouchingJob()
{
}

void BuildGraphTouchingJob::setup(const TopLevelProjectPtr &project,
                                  const QList<ResolvedProductPtr> &products, bool dryRun)
{
    m_project = project;
    m_products = products;
    m_dryRun = dryRun;
}

void BuildGraphTouchingJob::storeBuildGraph()
{
    if (!m_dryRun)
        InternalJob::storeBuildGraph(m_project);
}

InternalBuildJob::InternalBuildJob(const Logger &logger, QObject *parent)
    : BuildGraphTouchingJob(logger, parent), m_executor(0)
{
}

void InternalBuildJob::build(const TopLevelProjectPtr &project,
        const QList<ResolvedProductPtr> &products, const BuildOptions &buildOptions)
{
    setup(project, products, buildOptions.dryRun());
    setTimed(buildOptions.logElapsedTime());

    m_executor = new Executor(logger());
    m_executor->setProject(project);
    m_executor->setProducts(products);
    m_executor->setBuildOptions(buildOptions);
    m_executor->setProgressObserver(observer());

    QThread * const executorThread = new QThread(this);
    m_executor->moveToThread(executorThread);
    connect(m_executor, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    connect(m_executor, SIGNAL(reportProcessResult(qbs::ProcessResult)),
            this, SIGNAL(reportProcessResult(qbs::ProcessResult)));

    connect(executorThread, SIGNAL(started()), m_executor, SLOT(build()));
    connect(m_executor, SIGNAL(finished()), SLOT(handleFinished()));
    connect(m_executor, SIGNAL(destroyed()), executorThread, SLOT(quit()));
    connect(executorThread, SIGNAL(finished()), this, SLOT(emitFinished()));
    executorThread->start();
}

void InternalBuildJob::handleFinished()
{
    setError(m_executor->error());
    project()->buildData->evaluationContext.clear();
    storeBuildGraph();
    m_executor->deleteLater();
}

void InternalBuildJob::emitFinished()
{
    emit finished(this);
}

InternalCleanJob::InternalCleanJob(const Logger &logger, QObject *parent)
    : BuildGraphTouchingJob(logger, parent)
{
}

void InternalCleanJob::init(const TopLevelProjectPtr &project,
                             const QList<ResolvedProductPtr> &products, const CleanOptions &options)
{
    setup(project, products, options.dryRun());
    setTimed(options.logElapsedTime());
    m_options = options;
}

void InternalCleanJob::start()
{
    try {
        ArtifactCleaner cleaner(logger(), observer());
        cleaner.cleanup(project(), products(), m_options);
    } catch (const ErrorInfo &error) {
        setError(error);
    }
    storeBuildGraph();
    emit finished(this);
}


InternalInstallJob::InternalInstallJob(const Logger &logger)
    : InternalJob(logger)
{
}

InternalInstallJob::~InternalInstallJob()
{
}

void InternalInstallJob::init(const TopLevelProjectPtr &project,
        const QList<ResolvedProductPtr> &products, const InstallOptions &options)
{
    m_project = project;
    m_products = products;
    m_options = options;
    setTimed(options.logElapsedTime());
}

void InternalInstallJob::start()
{
    try {
        ProductInstaller(m_project, m_products, m_options, observer(), logger()).install();
    } catch (const ErrorInfo &error) {
        setError(error);
    }
    emit finished(this);
}

} // namespace Internal
} // namespace qbs
