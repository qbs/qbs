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
#include "executor.h"

#include "artifactvisitor.h"
#include "automoc.h"
#include "buildgraph.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "cycledetector.h"
#include "executorjob.h"
#include "inputartifactscanner.h"
#include "rulesevaluationcontext.h"

#include <buildgraph/transformer.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QSet>
#include <QTimer>

#include <algorithm>

namespace qbs {
namespace Internal {

class MocEffortCalculator : public ArtifactVisitor
{
public:
    MocEffortCalculator() : ArtifactVisitor(Artifact::SourceFile), m_effort(0) {}

    int effort() const { return m_effort; }

private:
    void doVisit(Artifact *) { m_effort += 10; }

    int m_effort;
};

class BuildEffortCalculator : public ArtifactVisitor
{
public:
    BuildEffortCalculator() : ArtifactVisitor(Artifact::Generated), m_effort(0) {}

    int effort() const { return m_effort; }

    static int multiplier(const Artifact *artifact) {
        return artifact->transformer->rule->multiplex ? 200 : 20;
    }

private:
    void doVisit(Artifact *artifact)
    {
        m_effort += multiplier(artifact);
    }

    int m_effort;
};


Executor::Executor(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_logger(logger)
    , m_progressObserver(0)
    , m_state(ExecutorIdle)
    , m_doTrace(logger.traceEnabled())
    , m_doDebug(logger.debugEnabled())
{
    m_inputArtifactScanContext = new InputArtifactScannerContext(&m_scanResultCache);
    m_autoMoc = new AutoMoc(logger);
    connect(m_autoMoc, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    m_autoMoc->setScanResultCache(&m_scanResultCache);
}

Executor::~Executor()
{
    // jobs must be destroyed before deleting the shared scan result cache
    foreach (ExecutorJob *job, m_availableJobs)
        delete job;
    foreach (ExecutorJob *job, m_processingJobs.keys())
        delete job;
    delete m_autoMoc; // delete before shared scan result cache
    delete m_inputArtifactScanContext;
}

FileTime Executor::recursiveFileTime(const QString &filePath) const
{
    FileTime newest;
    if (m_progressObserver) {
        qApp->processEvents();  // let the cancel event do its work
        if (m_progressObserver->canceled())
            return newest;
    }
    FileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        const QString nativeFilePath = QDir::toNativeSeparators(filePath);
        m_logger.qbsWarning() << Tr::tr("File '%1' not found.").arg(nativeFilePath);
        return newest;
    }
    newest = qMax(fileInfo.lastModified(), fileInfo.lastStatusChange());
    if (!fileInfo.isDir())
        return newest;
    const QStringList dirContents = QDir(filePath)
            .entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &curFileName, dirContents) {
        const FileTime ft = recursiveFileTime(filePath + QLatin1Char('/') + curFileName);
        if (ft > newest)
            newest = ft;
    }
    return newest;
}

void Executor::retrieveSourceFileTimestamp(Artifact *artifact) const
{
    QBS_CHECK(artifact->artifactType == Artifact::SourceFile);

    artifact->setTimestamp(recursiveFileTime(artifact->filePath()));
    artifact->timestampRetrieved = true;
}

void Executor::build()
{
    try {
        doBuild();
    } catch (const ErrorInfo &e) {
        m_error = e;
        QTimer::singleShot(0, this, SLOT(finish()));
    }
}

void Executor::setProject(const TopLevelProjectPtr &project)
{
    m_project = project;
}

void Executor::setProducts(const QList<ResolvedProductPtr> &productsToBuild)
{
    m_productsToBuild = productsToBuild;
}

void Executor::doBuild()
{
    if (m_buildOptions.maxJobCount() <= 0) {
        m_buildOptions.setMaxJobCount(BuildOptions::defaultMaxJobCount());
        m_logger.qbsDebug() << "max job count not explicitly set, using value of "
                            << m_buildOptions.maxJobCount();
    }
    QBS_CHECK(m_state == ExecutorIdle);
    m_leaves.clear();
    m_error.clear();
    m_explicitlyCanceled = false;
    m_activeFileTags = FileTags::fromStringList(m_buildOptions.activeFileTags());

    setState(ExecutorRunning);

    if (m_productsToBuild.isEmpty()) {
        m_logger.qbsTrace() << "No products to build, finishing.";
        QTimer::singleShot(0, this, SLOT(finish())); // Don't call back on the caller.
        return;
    }

    doSanityChecks();
    m_evalContext = m_project->buildData->evaluationContext;
    if (!m_evalContext) { // Is null before the first build.
        m_evalContext = RulesEvaluationContextPtr(new RulesEvaluationContext(m_logger));
        m_project->buildData->evaluationContext = m_evalContext;
    }

    m_logger.qbsDebug() << QString::fromLocal8Bit("[EXEC] preparing executor for %1 jobs "
                                                  "in parallel").arg(m_buildOptions.maxJobCount());
    addExecutorJobs(m_buildOptions.maxJobCount());
    foreach (ExecutorJob * const job, m_availableJobs)
        job->setDryRun(m_buildOptions.dryRun());

    bool sourceFilesChanged = false;
    prepareAllArtifacts(&sourceFilesChanged);
    Artifact::BuildState initialBuildState = m_buildOptions.changedFiles().isEmpty()
            ? Artifact::Buildable : Artifact::Built;

    QList<Artifact *> changedArtifacts;
    foreach (const QString &filePath, m_buildOptions.changedFiles()) {
        QList<FileResourceBase *> lookupResults;
        lookupResults.append(m_project->buildData->lookupFiles(filePath));
        if (lookupResults.isEmpty()) {
            m_logger.qbsWarning() << QString::fromLocal8Bit("Out of date file '%1' provided "
                    "but not found.").arg(QDir::toNativeSeparators(filePath));
            continue;
        }
        foreach (FileResourceBase *lookupResult, lookupResults)
            if (Artifact *artifact = dynamic_cast<Artifact *>(lookupResult))
                changedArtifacts += artifact;
    }
    qSort(changedArtifacts);
    changedArtifacts.erase(std::unique(changedArtifacts.begin(), changedArtifacts.end()),
                           changedArtifacts.end());

    // prepare products
    foreach (ResolvedProductPtr product, m_productsToBuild)
        product->setupBuildEnvironment(m_evalContext->engine(), m_project->environment);

    // find the root nodes
    m_roots.clear();
    foreach (const ResolvedProductPtr &product, m_productsToBuild) {
        foreach (Artifact *targetArtifact, product->buildData->targetArtifacts) {
            m_roots += targetArtifact;

            // The user expects that he can delete target artifacts and they get rebuilt.
            // To achieve this we must retrieve their timestamps.
            targetArtifact->setTimestamp(FileInfo(targetArtifact->filePath()).lastModified());
        }
    }

    prepareReachableArtifacts(initialBuildState);
    setupProgressObserver(sourceFilesChanged);
    if (sourceFilesChanged)
        runAutoMoc();
    initLeaves(changedArtifacts);
    if (!scheduleJobs()) {
        m_logger.qbsTrace() << "Nothing to do at all, finishing.";
        QTimer::singleShot(0, this, SLOT(finish())); // Don't call back on the caller.
    }
}

void Executor::setBuildOptions(const BuildOptions &buildOptions)
{
    m_buildOptions = buildOptions;
}

static void initArtifactsBottomUp(Artifact *artifact)
{
    if (artifact->buildState == Artifact::Untouched)
        return;
    artifact->buildState = Artifact::Buildable;
    foreach (Artifact *parent, artifact->parents)
        initArtifactsBottomUp(parent);
}

void Executor::initLeaves(const QList<Artifact *> &changedArtifacts)
{
    if (changedArtifacts.isEmpty()) {
        QSet<Artifact *> seenArtifacts;
        foreach (Artifact *root, m_roots)
            initLeavesTopDown(root, seenArtifacts);
    } else {
        foreach (Artifact *artifact, changedArtifacts) {
            m_leaves.append(artifact);
            initArtifactsBottomUp(artifact);
        }
    }
}

void Executor::initLeavesTopDown(Artifact *artifact, QSet<Artifact *> &seenArtifacts)
{
    if (seenArtifacts.contains(artifact))
        return;
    seenArtifacts += artifact;

    // Artifacts that appear in the build graph after
    // prepareBuildGraph() has been called, must be initialized.
    if (artifact->buildState == Artifact::Untouched) {
        artifact->buildState = Artifact::Buildable;
        if (artifact->artifactType == Artifact::SourceFile)
            retrieveSourceFileTimestamp(artifact);
    }

    if (artifact->children.isEmpty()) {
        m_leaves.append(artifact);
    } else {
        foreach (Artifact *child, artifact->children)
            initLeavesTopDown(child, seenArtifacts);
    }
}

// Returns true if some artifacts are still waiting to be built or currently building.
bool Executor::scheduleJobs()
{
    QBS_CHECK(m_state == ExecutorRunning);
    while (!m_leaves.isEmpty() && !m_availableJobs.isEmpty())
        buildArtifact(m_leaves.takeFirst());
    return !m_leaves.isEmpty() || !m_processingJobs.isEmpty();
}

bool Executor::isUpToDate(Artifact *artifact) const
{
    QBS_CHECK(artifact->artifactType == Artifact::Generated);

    const bool debug = false;
    if (debug) {
        m_logger.qbsDebug() << "[UTD] check " << artifact->filePath() << " "
                            << artifact->timestamp().toString();
    }

    if (m_buildOptions.forceTimestampCheck()) {
        artifact->setTimestamp(FileInfo(artifact->filePath()).lastModified());
        if (debug) {
            m_logger.qbsDebug() << "[UTD] timestamp retrieved from filesystem: "
                                << artifact->timestamp().toString();
        }
    }

    if (!artifact->timestamp().isValid()) {
        if (debug)
            m_logger.qbsDebug() << "[UTD] invalid timestamp. Out of date.";
        return false;
    }

    foreach (Artifact *child, artifact->children) {
        QBS_CHECK(child->timestamp().isValid());
        if (debug)
            m_logger.qbsDebug() << "[UTD] child timestamp " << child->timestamp().toString();
        if (artifact->timestamp() < child->timestamp())
            return false;
    }

    foreach (FileDependency *fileDependency, artifact->fileDependencies) {
        if (!fileDependency->timestamp().isValid()) {
            FileInfo fi(fileDependency->filePath());
            fileDependency->setTimestamp(fi.lastModified());
        }
        if (debug)
            m_logger.qbsDebug() << "[UTD] file dependency timestamp "
                                << fileDependency->timestamp().toString();
        if (artifact->timestamp() < fileDependency->timestamp())
            return false;
    }

    return true;
}

bool Executor::mustExecuteTransformer(const TransformerPtr &transformer) const
{
    foreach (Artifact *artifact, transformer->outputs)
        if (artifact->alwaysUpdated)
            return !isUpToDate(artifact);

    // All outputs of the transformer have alwaysUpdated == false.
    // We need at least on output that is always updated.
    QBS_CHECK(false);
    return true;
}

void Executor::buildArtifact(Artifact *artifact)
{
    QBS_CHECK(!m_availableJobs.isEmpty());

    if (m_doDebug)
        m_logger.qbsDebug() << "[EXEC] " << relativeArtifactFileName(artifact);

    // Skip artifacts that are already built.
    if (artifact->buildState == Artifact::Built) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] artifact already built. Skipping.";
        return;
    }

    // skip artifacts without transformer
    if (artifact->artifactType != Artifact::Generated) {
        // For source artifacts, that were not reachable when initializing the build, we must
        // retrieve timestamps. This can happen, if a dependency that's added during the build
        // makes the source artifact reachable.
        if (artifact->artifactType == Artifact::SourceFile && !artifact->timestampRetrieved)
            retrieveSourceFileTimestamp(artifact);

        if (m_doDebug)
            m_logger.qbsDebug() << QString::fromLocal8Bit("[EXEC] artifact type %1. Skipping.")
                                   .arg(toString(artifact->artifactType));
        finishArtifact(artifact);
        return;
    }

    // Every generated artifact must have a transformer.
    QBS_CHECK(artifact->transformer);

    // Skip if outputs of this transformer are already built.
    // That means we already ran the transformation.
    foreach (Artifact *sideBySideArtifact, artifact->transformer->outputs) {
        if (sideBySideArtifact == artifact)
            continue;
        switch (sideBySideArtifact->buildState)
        {
        case Artifact::Untouched:
        case Artifact::Buildable:
            break;
        case Artifact::Built:
            if (m_doDebug)
                m_logger.qbsDebug() << "[EXEC] Side by side artifact already finished. Skipping.";
            finishArtifact(artifact);
            return;
        case Artifact::Building:
            if (m_doDebug)
                m_logger.qbsDebug() << "[EXEC] Side by side artifact processing. Skipping.";
            artifact->buildState = Artifact::Building;
            return;
        }
    }

    // Skip if we're building just one file and the file tags do not match.
    if (!m_activeFileTags.isEmpty() && !m_activeFileTags.matches(artifact->fileTags)) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] file tags do not match. Skipping.";
        finishArtifact(artifact);
        return;
    }

    // Skip transformers that do not need to be built.
    if (!mustExecuteTransformer(artifact->transformer)) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] Up to date. Skipping.";
        finishArtifact(artifact);
        return;
    }

    // create the output directories
    if (!m_buildOptions.dryRun()) {
        ArtifactList::const_iterator it = artifact->transformer->outputs.begin();
        for (; it != artifact->transformer->outputs.end(); ++it) {
            Artifact *output = *it;
            QDir outDir = QFileInfo(output->filePath()).absoluteDir();
            if (!outDir.exists())
                outDir.mkpath(".");
        }
    }

    // scan all input artifacts
    InputArtifactScanner scanner(artifact, m_inputArtifactScanContext, m_logger);
    scanner.scan();

    // postpone the build of this artifact, if new dependencies found
    if (scanner.newDependencyAdded()) {
        bool buildingDependenciesFound = false;
        QVector<Artifact *> unbuiltDependencies;
        foreach (Artifact *dependency, artifact->children) {
            switch (dependency->buildState) {
            case Artifact::Untouched:
            case Artifact::Buildable:
                unbuiltDependencies += dependency;
                break;
            case Artifact::Building:
                buildingDependenciesFound = true;
                break;
            case Artifact::Built:
                // do nothing
                break;
            }
        }
        if (!unbuiltDependencies.isEmpty()) {
            artifact->inputsScanned = false;
            insertLeavesAfterAddingDependencies(unbuiltDependencies);
            return;
        }
        if (buildingDependenciesFound) {
            artifact->inputsScanned = false;
            return;
        }
    }

    ExecutorJob *job = m_availableJobs.takeFirst();
    artifact->buildState = Artifact::Building;
    m_processingJobs.insert(job, artifact);

    Q_ASSERT_X(artifact->product, Q_FUNC_INFO,
               qPrintable(QString("Generated artifact '%1' belongs to no product.")
               .arg(QDir::toNativeSeparators(artifact->filePath()))));

    job->run(artifact->transformer.data(), artifact->product);
}

void Executor::finishJob(ExecutorJob *job, bool success)
{
    QBS_CHECK(job);
    QBS_CHECK(m_state != ExecutorIdle);

    const QHash<ExecutorJob *, Artifact *>::Iterator it = m_processingJobs.find(job);
    QBS_CHECK(it != m_processingJobs.end());
    if (success)
        finishArtifact(it.value());
    m_processingJobs.erase(it);
    m_availableJobs.append(job);

    if (!success && !m_buildOptions.keepGoing())
        cancelJobs();

    if (m_state == ExecutorRunning && m_progressObserver && m_progressObserver->canceled()) {
        m_logger.qbsTrace() << "Received cancel request; canceling build.";
        m_explicitlyCanceled = true;
        cancelJobs();
    }

    if (m_state == ExecutorCanceling) {
        if (m_processingJobs.isEmpty()) {
            m_logger.qbsTrace() << "All pending jobs are done, finishing.";
            finish();
        }
        return;
    }

    if (!scheduleJobs()) {
        m_logger.qbsTrace() << "Nothing left to build; finishing.";
        finish();
    }
}

static bool allChildrenBuilt(Artifact *artifact)
{
    foreach (Artifact *child, artifact->children)
        if (child->buildState != Artifact::Built)
            return false;
    return true;
}

void Executor::finishArtifact(Artifact *leaf)
{
    QBS_CHECK(leaf);

    if (m_doTrace)
        m_logger.qbsTrace() << "[EXEC] finishArtifact " << relativeArtifactFileName(leaf);

    leaf->buildState = Artifact::Built;
    m_scanResultCache.remove(leaf->filePath());
    foreach (Artifact *parent, leaf->parents) {
        if (parent->buildState != Artifact::Buildable) {
            if (m_doTrace) {
                m_logger.qbsTrace() << "[EXEC] parent " << relativeArtifactFileName(parent)
                                    << " build state: " << toString(parent->buildState);
            }
            continue;
        }

        if (allChildrenBuilt(parent)) {
            m_leaves.append(parent);
            if (m_doTrace) {
                m_logger.qbsTrace() << "[EXEC] finishArtifact adds leaf "
                        << relativeArtifactFileName(parent) << " " << toString(parent->buildState);
            }
        } else {
            if (m_doTrace) {
                m_logger.qbsTrace() << "[EXEC] parent " << relativeArtifactFileName(parent)
                                    << " build state: " << toString(parent->buildState);
            }
        }
    }

    if (leaf->transformer)
        foreach (Artifact *sideBySideArtifact, leaf->transformer->outputs)
            if (leaf != sideBySideArtifact && sideBySideArtifact->buildState == Artifact::Building)
                finishArtifact(sideBySideArtifact);

    if (m_progressObserver && leaf->artifactType == Artifact::Generated)
        m_progressObserver->incrementProgressValue(BuildEffortCalculator::multiplier(leaf));
}

void Executor::insertLeavesAfterAddingDependencies_recurse(Artifact *const artifact,
        QSet<Artifact *> *seenArtifacts, QList<Artifact *> *leaves) const
{
    if (seenArtifacts->contains(artifact))
        return;
    seenArtifacts->insert(artifact);

    if (artifact->buildState == Artifact::Untouched)
        artifact->buildState = Artifact::Buildable;

    bool isLeaf = true;
    foreach (Artifact *child, artifact->children) {
        if (child->buildState != Artifact::Built) {
            isLeaf = false;
            insertLeavesAfterAddingDependencies_recurse(child, seenArtifacts, leaves);
        }
    }

    if (isLeaf) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] adding leaf " << relativeArtifactFileName(artifact);
        leaves->append(artifact);
    }
}

QString Executor::configString() const
{
    return tr(" for configuration %1").arg(m_project->id());
}

void Executor::insertLeavesAfterAddingDependencies(QVector<Artifact *> dependencies)
{
    QSet<Artifact *> seenArtifacts;
    foreach (Artifact *dependency, dependencies)
        insertLeavesAfterAddingDependencies_recurse(dependency, &seenArtifacts, &m_leaves);
}

void Executor::cancelJobs()
{
    m_logger.qbsTrace() << "Canceling all jobs.";
    setState(ExecutorCanceling);
    QList<ExecutorJob *> jobs = m_processingJobs.keys();
    foreach (ExecutorJob *job, jobs)
        job->cancel();
}

void Executor::setupProgressObserver(bool mocWillRun)
{
    if (!m_progressObserver)
        return;
    MocEffortCalculator mocEffortCalculator;
    BuildEffortCalculator buildEffortCalculator;
    foreach (const ResolvedProductConstPtr &product, m_productsToBuild)
        buildEffortCalculator.visitProduct(product);
    if (mocWillRun) {
        foreach (const ResolvedProductConstPtr &product, m_productsToBuild)
            mocEffortCalculator.visitProduct(product);
    }
    m_mocEffort = mocEffortCalculator.effort();
    const int totalEffort = m_mocEffort + buildEffortCalculator.effort();
    m_progressObserver->initialize(tr("Building%1").arg(configString()), totalEffort);
}

void Executor::doSanityChecks()
{
    QBS_CHECK(m_project);
    QBS_CHECK(!m_productsToBuild.isEmpty());
    foreach (const ResolvedProductConstPtr &product, m_productsToBuild) {
        QBS_CHECK(product->buildData);
        QBS_CHECK(product->topLevelProject() == m_project);
    }
}

void Executor::handleError(const ErrorInfo &error)
{
    m_error = error;
    if (m_processingJobs.isEmpty())
        finish();
    else
        cancelJobs();
}

void Executor::addExecutorJobs(int jobNumber)
{
    for (int i = 1; i <= jobNumber; i++) {
        ExecutorJob *job = new ExecutorJob(m_logger, this);
        job->setMainThreadScriptEngine(m_evalContext->engine());
        job->setObjectName(QString(QLatin1String("J%1")).arg(i));
        m_availableJobs.append(job);
        connect(job, SIGNAL(reportCommandDescription(QString,QString)),
                this, SIGNAL(reportCommandDescription(QString,QString)), Qt::QueuedConnection);
        connect(job, SIGNAL(reportProcessResult(qbs::ProcessResult)),
                this, SIGNAL(reportProcessResult(qbs::ProcessResult)), Qt::QueuedConnection);
        connect(job, SIGNAL(error(qbs::ErrorInfo)),
                this, SLOT(onProcessError(qbs::ErrorInfo)), Qt::QueuedConnection);
        connect(job, SIGNAL(success()), this, SLOT(onProcessSuccess()), Qt::QueuedConnection);
    }
}

void Executor::runAutoMoc()
{
    bool autoMocApplied = false;
    foreach (const ResolvedProductPtr &product, m_productsToBuild) {
        if (m_progressObserver && m_progressObserver->canceled())
            throw ErrorInfo(Tr::tr("Build canceled%1.").arg(configString()));
        // HACK call the automoc thingy here only if we have use Qt/core module
        foreach (const ResolvedModuleConstPtr &m, product->modules) {
            if (m->name == "Qt/core") {
                autoMocApplied = true;
                m_autoMoc->apply(product);
                break;
            }
        }
    }
    if (autoMocApplied) {
        foreach (const ResolvedProductConstPtr &product, m_productsToBuild)
            CycleDetector(m_logger).visitProduct(product);
    }
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue(m_mocEffort);
}

void Executor::onProcessError(const qbs::ErrorInfo &err)
{
    try {
        if (m_buildOptions.keepGoing()) {
            ErrorInfo fullWarning(err);
            fullWarning.prepend(Tr::tr("Ignoring the following errors on user request:"));
            m_logger.printWarning(fullWarning);
        } else {
            m_error = err;
        }
        ExecutorJob * const job = qobject_cast<ExecutorJob *>(sender());
        finishJob(job, false);
    } catch (const ErrorInfo &error) {
        handleError(error);
    }
}

void Executor::onProcessSuccess()
{
    try {
        ExecutorJob *job = qobject_cast<ExecutorJob *>(sender());
        QBS_CHECK(job);
        Artifact *processedArtifact = m_processingJobs.value(job);
        QBS_CHECK(processedArtifact);

        // Update the timestamps of the outputs of the transformer we just executed.
        processedArtifact->product->topLevelProject()->buildData->isDirty = true;
        foreach (Artifact *artifact, processedArtifact->transformer->outputs) {
            if (artifact->alwaysUpdated)
                artifact->setTimestamp(FileTime::currentTime());
            else
                artifact->setTimestamp(FileInfo(artifact->filePath()).lastModified());
        }

        finishJob(job, true);
    } catch (const ErrorInfo &error) {
        handleError(error);
    }
}

void Executor::finish()
{
    QBS_ASSERT(m_state != ExecutorIdle, /* ignore */);

    QStringList unbuiltProductNames;
    foreach (const ResolvedProductPtr &product, m_productsToBuild) {
        foreach (Artifact *artifact, product->buildData->targetArtifacts) {
            if (artifact->buildState != Artifact::Built) {
                unbuiltProductNames += product->name;
                break;
            }
        }
    }

    if (unbuiltProductNames.isEmpty()) {
        m_logger.qbsInfo() << Tr::tr("Build done%1.").arg(configString());
    } else {
        m_error.append(Tr::tr("The following products could not be built%1: %2.")
                 .arg(configString(), unbuiltProductNames.join(", ")));
    }

    if (m_explicitlyCanceled)
        m_error.append(Tr::tr("Build canceled%1.").arg(configString()));
    setState(ExecutorIdle);
    if (m_progressObserver)
        m_progressObserver->setFinished();
    emit finished();
}

/**
  * Sets the state of all artifacts in the graph to "untouched".
  * This must be done before doing a build.
  *
  * Retrieves the timestamps of source artifacts.
  *
  * This function sets *sourceFilesChanged to true, if the timestamp of a reachable source artifact
  * changed.
  */
void Executor::prepareAllArtifacts(bool *sourceFilesChanged)
{
    foreach (const ResolvedProductPtr &product, m_productsToBuild) {
        foreach (Artifact *artifact, product->buildData->artifacts) {
            artifact->buildState = Artifact::Untouched;
            artifact->inputsScanned = false;
            artifact->timestampRetrieved = false;

            if (artifact->artifactType == Artifact::SourceFile) {
                const FileTime oldTimestamp = artifact->timestamp();
                retrieveSourceFileTimestamp(artifact);
                if (oldTimestamp != artifact->timestamp())
                    *sourceFilesChanged = true;
            }

            // Timestamps of file dependencies must be invalid for every build.
            foreach (FileDependency *fileDependency, artifact->fileDependencies)
                fileDependency->clearTimestamp();
        }
    }
}

/**
 * Walk the build graph top-down from the roots and for each reachable node N
 *  - mark N as buildable.
 */
void Executor::prepareReachableArtifacts(const Artifact::BuildState buildState)
{
    foreach (Artifact *root, m_roots)
        prepareReachableArtifacts_impl(root, buildState);
}

void Executor::prepareReachableArtifacts_impl(Artifact *artifact,
        const Artifact::BuildState buildState)
{
    if (artifact->buildState != Artifact::Untouched)
        return;

    artifact->buildState = buildState;
    foreach (Artifact *child, artifact->children)
        prepareReachableArtifacts_impl(child, buildState);
}

void Executor::updateBuildGraph(Artifact::BuildState buildState)
{
    QSet<Artifact *> seenArtifacts;
    foreach (Artifact *root, m_roots)
        updateBuildGraph_impl(root, buildState, seenArtifacts);
}

void Executor::updateBuildGraph_impl(Artifact *artifact, Artifact::BuildState buildState, QSet<Artifact *> &seenArtifacts)
{
    if (seenArtifacts.contains(artifact))
        return;

    seenArtifacts += artifact;
    artifact->buildState = buildState;

    foreach (Artifact *child, artifact->children)
        updateBuildGraph_impl(child, buildState, seenArtifacts);
}

void Executor::setState(ExecutorState s)
{
    if (m_state == s)
        return;
    m_state = s;
}

} // namespace Internal
} // namespace qbs
