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
#include "buildproduct.h"
#include "buildproject.h"
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
    Q_ASSERT(artifact->artifactType == Artifact::SourceFile);

    artifact->timestamp = recursiveFileTime(artifact->filePath());
    artifact->timestampRetrieved = true;
}

void Executor::build()
{
    try {
        doBuild();
    } catch (const Error &e) {
        m_error = e;
        QTimer::singleShot(0, this, SLOT(finish()));
    }
}

void Executor::setProducts(const QList<BuildProductPtr> &productsToBuild)
{
    m_productsToBuild = productsToBuild;
}

void Executor::doBuild()
{
    if (m_buildOptions.maxJobCount <= 0) {
        m_buildOptions.maxJobCount = BuildOptions::defaultMaxJobCount();
        m_logger.qbsDebug() << "max job count not explicitly set, using value of "
                            << m_buildOptions.maxJobCount;
    }
    Q_ASSERT(m_state == ExecutorIdle);
    m_leaves.clear();
    m_error.clear();
    m_explicitlyCanceled = false;

    setState(ExecutorRunning);

    if (m_productsToBuild.isEmpty()) {
        m_logger.qbsTrace() << "No products to build, finishing.";
        QTimer::singleShot(0, this, SLOT(finish())); // Don't call back on the caller.
        return;
    }

    doSanityChecks();
    BuildProject * const project = m_productsToBuild.first()->project;
    m_evalContext = project->evaluationContext();
    if (!m_evalContext) { // Is null before the first build.
        m_evalContext = RulesEvaluationContextPtr(new RulesEvaluationContext(m_logger));
        project->setEvaluationContext(m_evalContext);
    }

    m_logger.qbsDebug() << QString::fromLocal8Bit("[EXEC] preparing executor for %1 jobs "
                                                  "in parallel").arg(m_buildOptions.maxJobCount);
    addExecutorJobs(m_buildOptions.maxJobCount);
    foreach (ExecutorJob * const job, m_availableJobs)
        job->setDryRun(m_buildOptions.dryRun);

    initializeArtifactsState();
    Artifact::BuildState initialBuildState = m_buildOptions.changedFiles.isEmpty()
            ? Artifact::Buildable : Artifact::Built;

    QList<Artifact *> changedArtifacts;
    foreach (const QString &filePath, m_buildOptions.changedFiles) {
        QList<Artifact *> artifacts;
        artifacts.append(project->lookupArtifacts(filePath));
        if (artifacts.isEmpty()) {
            m_logger.qbsWarning() << QString::fromLocal8Bit("Out of date file '%1' provided "
                    "but not found.").arg(QDir::toNativeSeparators(filePath));
            continue;
        }
        changedArtifacts += artifacts;
    }
    qSort(changedArtifacts);
    std::unique(changedArtifacts.begin(), changedArtifacts.end());

    // prepare products
    foreach (BuildProductPtr product, m_productsToBuild)
        product->rProduct->setupBuildEnvironment(m_evalContext->engine(), m_baseEnvironment);

    // find the root nodes
    m_roots.clear();
    foreach (BuildProductPtr product, m_productsToBuild) {
        foreach (Artifact *targetArtifact, product->targetArtifacts) {
            m_roots += targetArtifact;

            // The user expects that he can delete target artifacts and they get rebuilt.
            // To achieve this we must retrieve their timestamps.
            targetArtifact->timestamp = FileInfo(targetArtifact->filePath()).lastModified();
        }
    }

    bool sourceFilesChanged;
    prepareBuildGraph(initialBuildState, &sourceFilesChanged);
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
    Q_ASSERT(m_state == ExecutorRunning);
    while (!m_leaves.isEmpty() && !m_availableJobs.isEmpty())
        buildArtifact(m_leaves.takeFirst());
    return !m_leaves.isEmpty() || !m_processingJobs.isEmpty();
}

bool Executor::isUpToDate(Artifact *artifact) const
{
    Q_ASSERT(artifact->artifactType == Artifact::Generated);

    const bool debug = false;
    if (debug) {
        m_logger.qbsDebug() << "[UTD] check " << artifact->filePath() << " "
                            << artifact->timestamp.toString();
    }

    if (!artifact->timestamp.isValid()) {
        if (debug)
            m_logger.qbsDebug() << "[UTD] invalid timestamp. Out of date.";
        return false;
    }

    foreach (Artifact *child, artifact->children) {
        Q_ASSERT(child->timestamp.isValid());
        if (debug)
            m_logger.qbsDebug() << "[UTD] child timestamp " << child->timestamp.toString();
        if (artifact->timestamp < child->timestamp)
            return false;
    }

    foreach (Artifact *fileDependency, artifact->fileDependencies) {
        if (!fileDependency->timestamp.isValid()) {
            FileInfo fi(fileDependency->filePath());
            fileDependency->timestamp = fi.lastModified();
        }
        if (debug)
            m_logger.qbsDebug() << "[UTD] file dependency timestamp "
                                << fileDependency->timestamp.toString();
        if (artifact->timestamp < fileDependency->timestamp)
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
    Q_ASSERT(false);
    return true;
}

void Executor::buildArtifact(Artifact *artifact)
{
    Q_ASSERT(!m_availableJobs.isEmpty());

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
    Q_ASSERT(artifact->transformer);

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

    // Skip transformers that do not need to be built.
    if (!mustExecuteTransformer(artifact->transformer)) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] Up to date. Skipping.";
        finishArtifact(artifact);
        return;
    }

    // create the output directories
    if (!m_buildOptions.dryRun) {
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
               qPrintable(QString("Generated artifact '%1'' belongs to no product.")
               .arg(QDir::toNativeSeparators(artifact->filePath()))));

    job->run(artifact->transformer.data(), artifact->product);
}

void Executor::finishJob(ExecutorJob *job, bool success)
{
    Q_ASSERT(job);
    Q_ASSERT(m_state != ExecutorIdle);

    const QHash<ExecutorJob *, Artifact *>::Iterator it = m_processingJobs.find(job);
    Q_ASSERT(it != m_processingJobs.end());
    if (success)
        finishArtifact(it.value());
    m_processingJobs.erase(it);
    m_availableJobs.append(job);

    if (!success && !m_buildOptions.keepGoing)
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
    Q_ASSERT(leaf);

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
    foreach (const BuildProductConstPtr &product, m_productsToBuild)
        buildEffortCalculator.visitProduct(product);
    if (mocWillRun) {
        foreach (const BuildProductConstPtr &product, m_productsToBuild)
            mocEffortCalculator.visitProduct(product);
    }
    m_mocEffort = mocEffortCalculator.effort();
    const int totalEffort = m_mocEffort + buildEffortCalculator.effort();
    m_progressObserver->initialize(tr("Building"), totalEffort);
}

void Executor::doSanityChecks()
{
    Q_ASSERT(!m_productsToBuild.isEmpty());
    for (int i = 1; i < m_productsToBuild.count(); ++i)
        Q_ASSERT(m_productsToBuild.at(i)->project == m_productsToBuild.first()->project);
}

void Executor::addExecutorJobs(int jobNumber)
{
    for (int i = 1; i <= jobNumber; i++) {
        ExecutorJob *job = new ExecutorJob(m_logger, this);
        job->setMainThreadScriptEngine(m_evalContext->engine());
        job->setObjectName(QString(QLatin1String("J%1")).arg(i));
        m_availableJobs.append(job);
        connect(job, SIGNAL(reportCommandDescription(QString,QString)),
                this, SIGNAL(reportCommandDescription(QString,QString)));
        connect(job, SIGNAL(reportProcessResult(qbs::ProcessResult)),
                this, SIGNAL(reportProcessResult(qbs::ProcessResult)));
        connect(job, SIGNAL(error(qbs::Error)),
                this, SLOT(onProcessError(qbs::Error)));
        connect(job, SIGNAL(success()), this, SLOT(onProcessSuccess()));
    }
}

void Executor::runAutoMoc()
{
    bool autoMocApplied = false;
    foreach (const BuildProductPtr &product, m_productsToBuild) {
        if (m_progressObserver && m_progressObserver->canceled())
            throw Error(Tr::tr("Build canceled due to user request."));
        // HACK call the automoc thingy here only if we have use qt/core module
        foreach (const ResolvedModuleConstPtr &m, product->rProduct->modules) {
            if (m->name == "qt/core") {
                autoMocApplied = true;
                m_autoMoc->apply(product);
                break;
            }
        }
    }
    if (autoMocApplied) {
        foreach (const BuildProductConstPtr &product, m_productsToBuild)
            CycleDetector(m_logger).visitProduct(product);
    }
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue(m_mocEffort);
}

void Executor::onProcessError(const qbs::Error &err)
{
    if (m_buildOptions.keepGoing) {
        Error fullWarning(err);
        fullWarning.prepend(Tr::tr("Ignoring the following errors on user request:"));
        emit reportWarning(fullWarning);
    } else {
        m_error = err;
    }
    ExecutorJob * const job = qobject_cast<ExecutorJob *>(sender());
    finishJob(job, false);
}

void Executor::onProcessSuccess()
{
    ExecutorJob *job = qobject_cast<ExecutorJob *>(sender());
    Q_ASSERT(job);
    Artifact *processedArtifact = m_processingJobs.value(job);
    Q_ASSERT(processedArtifact);

    // Update the timestamps of the outputs of the transformer we just executed.
    processedArtifact->project->markDirty();
    foreach (Artifact *artifact, processedArtifact->transformer->outputs) {
        if (artifact->alwaysUpdated)
            artifact->timestamp = FileTime::currentTime();
        else
            artifact->timestamp = FileInfo(artifact->filePath()).lastModified();
    }

    finishJob(job, true);
}

void Executor::finish()
{
    Q_ASSERT(m_state != ExecutorIdle);

    QStringList unbuiltProductNames;
    foreach (BuildProductPtr buildProduct, m_productsToBuild) {
        foreach (Artifact *artifact, buildProduct->targetArtifacts) {
            if (artifact->buildState != Artifact::Built) {
                unbuiltProductNames += buildProduct->rProduct->name;
                break;
            }
        }
    }
    if (unbuiltProductNames.isEmpty()) {
        m_logger.qbsInfo() << Tr::tr("Build done.");
    } else {
        m_error.append(Tr::tr("The following products could not be built: %1.")
                 .arg(unbuiltProductNames.join(", ")));
    }

    if (m_explicitlyCanceled)
        m_error.append(Tr::tr("Build was canceled due to user request."));
    setState(ExecutorIdle);
    if (m_progressObserver)
        m_progressObserver->setFinished();
    emit finished();
}

/**
  * Sets the state of all artifacts in the graph to "untouched".
  * This must be done before doing a build.
  */
void Executor::initializeArtifactsState()
{
    foreach (const BuildProductPtr &product, m_productsToBuild) {
        foreach (Artifact *artifact, product->artifacts) {
            artifact->buildState = Artifact::Untouched;
            artifact->inputsScanned = false;
            artifact->timestampRetrieved = false;

            // Timestamps of file dependencies must be invalid for every build.
            foreach (Artifact *fileDependency, artifact->fileDependencies)
                fileDependency->timestamp.clear();
        }
    }
}

/**
 * Walk the build graph top-down from the roots and for each reachable node N
 *  - mark N as buildable,
 *  - retrieve the timestamps of N, if N is a source file.
 *
 * This function sets *sourceFilesChanged to true, if the timestamp of a reachable source artifact
 * changed. Otherwise *sourceFilesChanged will be false.
 */
void Executor::prepareBuildGraph(const Artifact::BuildState buildState, bool *sourceFilesChanged)
{
    *sourceFilesChanged = false;
    foreach (Artifact *root, m_roots)
        prepareBuildGraph_impl(root, buildState, sourceFilesChanged);
}

void Executor::prepareBuildGraph_impl(Artifact *artifact, const Artifact::BuildState buildState,
                                      bool *sourceFilesChanged)
{
    if (artifact->buildState != Artifact::Untouched)
        return;

    artifact->buildState = buildState;
    if (artifact->artifactType == Artifact::SourceFile) {
        const FileTime oldTimestamp = artifact->timestamp;
        retrieveSourceFileTimestamp(artifact);
        if (oldTimestamp != artifact->timestamp)
            *sourceFilesChanged = true;
    }

    foreach (Artifact *child, artifact->children)
        prepareBuildGraph_impl(child, buildState, sourceFilesChanged);
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
