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
#include <buildgraph/buildproduct.h>
#include <buildgraph/buildproject.h>
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

#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QScopedPointer>
#include <QThread>
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


InternalJob::InternalJob(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_observer(new JobObserver(this))
    , m_logger(logger)
{
}

void InternalJob::cancel()
{
    m_observer->cancel();
}

void InternalJob::storeBuildGraph(const BuildProject *buildProject)
{
    try {
        buildProject->store();
    } catch (const Error &error) {
        logger().qbsWarning() << error.toString();
    }
}

InternalSetupProjectJob::InternalSetupProjectJob(const Logger &logger, QObject *parent)
    : InternalJob(logger, parent), m_running(false)
{
}

InternalSetupProjectJob::~InternalSetupProjectJob()
{
    QMutexLocker locker(&m_runMutex);
    while (m_running)
        m_runWaitCondition.wait(&m_runMutex);
 }

void InternalSetupProjectJob::resolve(const SetupProjectParameters &parameters)
{
    m_parameters = parameters;
    QTimer::singleShot(0, this, SLOT(start()));
}

void InternalSetupProjectJob::reportError(const Error &error)
{
    setError(error);
    QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection,
                              Q_ARG(Internal::InternalJob *, this));
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
    RulesEvaluationContextPtr evalContext(new RulesEvaluationContext(logger()));
    evalContext->setObserver(observer());
    BuildProjectLoader bpLoader(logger());
    const BuildProjectLoader::LoadResult loadResult = bpLoader.load(m_parameters, evalContext);
    ResolvedProjectPtr rProject;
    if (!loadResult.discardLoadedProject)
        m_buildProject = loadResult.loadedProject;
    if (m_buildProject) {
        rProject = m_buildProject->resolvedProject();
    } else {
        if (loadResult.changedResolvedProject) {
            rProject = loadResult.changedResolvedProject;
        } else {
            Loader loader(evalContext->engine(), logger());
            loader.setSearchPaths(m_parameters.searchPaths);
            loader.setProgressObserver(observer());
            rProject = loader.loadProject(m_parameters);
        }
        if (rProject->products.isEmpty()) {
            throw Error(Tr::tr("Project '%1' does not contain products.")
                        .arg(m_parameters.projectFilePath));
        }
    }

    // copy the environment from the platform config into the project's config
    const QVariantMap platformEnvironment
            = m_parameters.buildConfiguration.value(QLatin1String("environment")).toMap();
    rProject->platformEnvironment = platformEnvironment;

    logger().qbsDebug() << QString::fromLocal8Bit("for %1:").arg(rProject->id());
    foreach (const ResolvedProductConstPtr &p, rProject->products) {
        logger().qbsDebug() << QString::fromLocal8Bit("  - [%1] %2 as %3")
                               .arg(p->fileTags.toStringList().join(QLatin1String(", ")))
                               .arg(p->name).arg(p->project->id());
    }
    logger().qbsDebug() << '\n';

    if (!m_buildProject) {
        TimedActivityLogger resolveLogger(logger(), QLatin1String("Resolving build project"));
        m_buildProject = BuildProjectResolver(logger()).resolveProject(rProject, evalContext);
        if (loadResult.loadedProject)
            m_buildProject->rescueDependencies(loadResult.loadedProject);
    }

    if (!m_parameters.dryRun)
        storeBuildGraph(m_buildProject.data());

    // The evalutation context cannot be re-used for building, which runs in a different thread.
    m_buildProject->setEvaluationContext(RulesEvaluationContextPtr());
}


BuildGraphTouchingJob::BuildGraphTouchingJob(const Logger &logger, QObject *parent)
    : InternalJob(logger, parent), m_dryRun(false)
{
}

BuildGraphTouchingJob::~BuildGraphTouchingJob()
{
}

void BuildGraphTouchingJob::setup(const QList<BuildProductPtr> &products, bool dryRun)
{
    m_products = products;
    m_dryRun = dryRun;
}

void BuildGraphTouchingJob::storeBuildGraph()
{
    if (!m_dryRun && !m_products.isEmpty())
        InternalJob::storeBuildGraph(m_products.first()->project);
}

InternalBuildJob::InternalBuildJob(const Logger &logger, QObject *parent)
    : BuildGraphTouchingJob(logger, parent), m_executor(0)
{
}

void InternalBuildJob::build(const QList<BuildProductPtr> &products,
                             const BuildOptions &buildOptions, const QProcessEnvironment &env)
{
    setup(products, buildOptions.dryRun);

    m_executor = new Executor(logger());
    m_executor->setProducts(products);
    m_executor->setBuildOptions(buildOptions);
    m_executor->setProgressObserver(observer());
    m_executor->setBaseEnvironment(env);

    QThread * const executorThread = new QThread(this);
    m_executor->moveToThread(executorThread);
    connect(m_executor, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    connect(m_executor, SIGNAL(reportProcessResult(qbs::ProcessResult)),
            this, SIGNAL(reportProcessResult(qbs::ProcessResult)));
    connect(m_executor, SIGNAL(reportWarning(qbs::Error)), this, SIGNAL(reportWarning(qbs::Error)));

    connect(executorThread, SIGNAL(started()), m_executor, SLOT(build()));
    connect(m_executor, SIGNAL(finished()), SLOT(handleFinished()));
    connect(m_executor, SIGNAL(destroyed()), executorThread, SLOT(quit()));
    connect(executorThread, SIGNAL(finished()), this, SLOT(emitFinished()));
    executorThread->start();
}

void InternalBuildJob::handleFinished()
{
    if (m_executor->hasError())
        setError(m_executor->error());
    if (!products().isEmpty())
        products().first()->project->setEvaluationContext(RulesEvaluationContextPtr());
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

void InternalCleanJob::clean(const QList<BuildProductPtr> &products, const CleanOptions &options)
{
    setup(products, options.dryRun);
    m_options = options;
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
        ArtifactCleaner cleaner(logger());
        cleaner.cleanup(products(), m_options);
    } catch (const Error &error) {
        setError(error);
    }
    storeBuildGraph();
}


InternalInstallJob::InternalInstallJob(const Logger &logger, QObject *parent)
    : InternalJob(logger, parent)
{
}

InternalInstallJob::~InternalInstallJob()
{
}

void InternalInstallJob::install(const QList<BuildProductPtr> &products,
                                 const InstallOptions &options)
{
    m_products = products;
    m_options = options;
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

void InternalInstallJob::handleFinished()
{
    emit finished(this);
}

void InternalInstallJob::start()
{
    QFutureWatcher<void> * const watcher = new QFutureWatcher<void>(this);
    connect(watcher, SIGNAL(finished()), SLOT(handleFinished()));
    watcher->setFuture(QtConcurrent::run(this, &InternalInstallJob::doInstall));
}

void InternalInstallJob::doInstall()
{
    try {
        ProductInstaller(m_products, m_options, observer(), logger()).install();
    } catch (const Error &error) {
        setError(error);
    }
}

} // namespace Internal
} // namespace qbs
