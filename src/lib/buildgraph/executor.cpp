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

#include "artifactvisitor.h"
#include "automoc.h"
#include "executor.h"
#include "executorjob.h"
#include "inputartifactscanner.h"

#include <buildgraph/transformer.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>

#include <QSet>

#include <algorithm>

namespace qbs {
namespace Internal {

static QHashDummyValue hashDummy;

class MocEffortCalculator : public ArtifactVisitor
{
public:
    MocEffortCalculator() : ArtifactVisitor(Artifact::SourceFile), m_effort(0) {}

    int effort() const { return m_effort; }

private:
    void doVisit(const Artifact *) { m_effort += 10; }

    int m_effort;
};

class ScanEffortCalculator : public ArtifactVisitor
{
public:
    ScanEffortCalculator()
        : ArtifactVisitor(Artifact::SourceFile | Artifact::FileDependency), m_effort(0) {}

    int effort() const { return m_effort; }

private:
    void doVisit(const Artifact *) { ++m_effort; }

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
    void doVisit(const Artifact *artifact)
    {
        m_effort += multiplier(artifact);
    }

    int m_effort;
};


Executor::Executor()
    : m_engine(0)
    , m_progressObserver(0)
    , m_state(ExecutorIdle)
    , m_buildResult(SuccessfulBuild)
{
    m_inputArtifactScanContext = new InputArtifactScannerContext(&m_scanResultCache);
    m_autoMoc = new AutoMoc;
    m_autoMoc->setScanResultCache(&m_scanResultCache);
}

Executor::~Executor()
{
    // jobs must be destroyed before deleting the shared scan result cache
    foreach (ExecutorJob *job, m_availableJobs)
        delete job;
    foreach (ExecutorJob *job, m_processingJobs.keys())
        delete job;
    delete m_autoMoc;
    delete m_inputArtifactScanContext;
}

void Executor::build(const QList<BuildProduct::Ptr> &productsToBuild)
{
    Q_ASSERT(m_buildOptions.maxJobCount > 0);
    Q_ASSERT(m_engine);
    Q_ASSERT(m_state != ExecutorRunning);
    m_leaves.clear();
    bool success = true;
    m_buildResult = SuccessfulBuild;
    m_productsToBuild = productsToBuild;

    setState(ExecutorRunning);
    Artifact::BuildState initialBuildState = m_buildOptions.changedFiles.isEmpty()
            ? Artifact::Buildable : Artifact::Built;

    QList<Artifact *> changedArtifacts;
    foreach (const QString &filePath, m_buildOptions.changedFiles) {
        QList<Artifact *> artifacts;
        QSet<const BuildProject *> projects;
        foreach (const BuildProduct::ConstPtr &buildProduct, productsToBuild)
            projects << buildProduct->project;
        foreach (const BuildProject * const project, projects)
            artifacts.append(project->lookupArtifacts(filePath));
        if (artifacts.isEmpty()) {
            qbsWarning() << QString("Out of date file '%1' provided but not found.").arg(QDir::toNativeSeparators(filePath));
            continue;
        }
        changedArtifacts += artifacts;
    }
    qSort(changedArtifacts);
    std::unique(changedArtifacts.begin(), changedArtifacts.end());

    // prepare products
    const QProcessEnvironment systemEnvironment = QProcessEnvironment::systemEnvironment();
    foreach (BuildProduct::Ptr product, m_productsToBuild) {
        try {
            product->rProduct->setupBuildEnvironment(m_engine, systemEnvironment);
        } catch (const Error &e) {
            setError(e.toString());
            return;
        }
        foreach (Artifact *artifact, product->artifacts)
            if (artifact->artifactType == Artifact::SourceFile)
                artifact->buildState = initialBuildState;
    }

    // find the root nodes
    m_roots.clear();
    foreach (BuildProduct::Ptr product, m_productsToBuild)
        foreach (Artifact *rootArtifact, product->targetArtifacts)
            m_roots += rootArtifact;

    // mark the artifacts we want to build
    prepareBuildGraph(initialBuildState);

    int scanEffort;
    int mocEffort;
    if (m_progressObserver) {
        if (m_progressObserver->canceled()) {
            setError(tr("Build canceled."));
            return;
        }
        ScanEffortCalculator scanEffortCalculator;
        MocEffortCalculator mocEffortCalculator;
        BuildEffortCalculator buildEffortCalculator;
        foreach (const BuildProduct::ConstPtr &product, m_productsToBuild) {
            scanEffortCalculator.visit(product);
            mocEffortCalculator.visit(product);
            buildEffortCalculator.visit(product);
        }
        scanEffort = scanEffortCalculator.effort();
        mocEffort = mocEffortCalculator.effort();
        const int totalEffort = scanEffort + mocEffort + buildEffortCalculator.effort();
        m_progressObserver->initialize(tr("Building"), totalEffort);
    }

    // determine which artifacts are out of date
    const bool changedFilesProvided = !m_buildOptions.changedFiles.isEmpty();
    if (changedFilesProvided) {
        m_printScanningMessage = true;
        foreach (Artifact *artifact, changedArtifacts) {
            if (!artifact->inputsScanned && artifact->artifactType == Artifact::Generated) {
                printScanningMessageOnce();
                InputArtifactScanner scanner(artifact, m_inputArtifactScanContext);
                scanner.scan();
            }
        }
    } else {
        doOutOfDateCheck();
    }
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue(scanEffort);

    if (success) {
        success = runAutoMoc();
        if (m_progressObserver)
            m_progressObserver->incrementProgressValue(mocEffort);
    }
    if (success)
        initLeaves(changedArtifacts);

    if (success) {
        bool stillArtifactsToExecute = run();

        if (!stillArtifactsToExecute)
            finish();
    }
}

void Executor::cancelBuild()
{
    if (m_state != ExecutorRunning)
        return;
    qbsInfo() << "Build canceled.";
    setState(ExecutorCanceled);
    cancelJobs();
}

void Executor::setEngine(ScriptEngine *engine)
{
    if (m_engine == engine)
        return;

    m_engine = engine;
    foreach (ExecutorJob *job, findChildren<ExecutorJob *>())
        job->setMainThreadScriptEngine(engine);
}

void Executor::setBuildOptions(const BuildOptions &buildOptions)
{
    m_buildOptions = buildOptions;

    qbsDebug("[EXEC] preparing executor for %d jobs in parallel", m_buildOptions.maxJobCount);
    const int actualJobNumber = m_availableJobs.count() + m_processingJobs.count();
    if (actualJobNumber > m_buildOptions.maxJobCount) {
        removeExecutorJobs(actualJobNumber - m_buildOptions.maxJobCount);
    } else {
        addExecutorJobs(m_buildOptions.maxJobCount - actualJobNumber);
    }

    foreach (ExecutorJob * const job, m_availableJobs)
        job->setDryRun(m_buildOptions.dryRun);
}

bool Executor::isLeaf(Artifact *artifact)
{
    foreach (Artifact *child, artifact->children)
        if (child->buildState != Artifact::Built)
            return false;
    return true;
}

static void markAsOutOfDateBottomUp(Artifact *artifact)
{
    if (artifact->buildState == Artifact::Untouched)
        return;
    artifact->buildState = Artifact::Buildable;
    artifact->outOfDateCheckPerformed = true;
    artifact->isOutOfDate = true;
    artifact->isExistingFile = FileInfo(artifact->filePath()).exists();
    foreach (Artifact *parent, artifact->parents)
        markAsOutOfDateBottomUp(parent);
}

void Executor::initLeaves(const QList<Artifact *> &changedArtifacts)
{
    if (changedArtifacts.isEmpty()) {
        QSet<Artifact *> seenArtifacts;
        foreach (Artifact *root, m_roots)
            initLeavesTopDown(root, seenArtifacts);
    } else {
        foreach (Artifact *artifact, changedArtifacts) {
            m_leaves.insert(artifact, hashDummy);
            markAsOutOfDateBottomUp(artifact);
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
    if (artifact->buildState == Artifact::Untouched)
        artifact->buildState = Artifact::Buildable;

    if (artifact->children.isEmpty()) {
        m_leaves.insert(artifact, hashDummy);
    } else {
        foreach (Artifact *child, artifact->children)
            initLeavesTopDown(child, seenArtifacts);
    }
}

/**
  * Returns true if there are still artifacts to traverse.
  */
bool Executor::run()
{
    while (m_state == ExecutorRunning) {
        if (m_leaves.isEmpty())
            return !m_processingJobs.isEmpty();

        if (m_availableJobs.isEmpty()) {
            if (qbsLogLevel(LoggerDebug))
                qbsDebug("[EXEC] No jobs available. Trying later.");
            return true;
        }

        Artifact *leaf = m_leaves.begin().key();
        execute(leaf);
    }
    return false;
}

FileTime Executor::timeStamp(Artifact *artifact)
{
    FileTime result = m_timeStampCache.value(artifact);
    if (result.isValid())
        return result;

    FileInfo fi(artifact->filePath());
    if (!fi.exists())
        return FileTime::currentTime();

    result = fi.lastModified();
    foreach (Artifact *child, artifact->children) {
        const FileTime childTime = timeStamp(child);
        if (result < childTime)
            result = childTime;
    }
    foreach (Artifact *fileDependency, artifact->fileDependencies) {
        const FileTime ft = timeStamp(fileDependency);
        if (result < ft)
            result = ft;
    }

    m_timeStampCache.insert(artifact, result);
    return result;
}

void Executor::execute(Artifact *artifact)
{
    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[EXEC] " << fileName(artifact);

//    artifact->project->buildGraph()->dump(*artifact->products.begin());

    if (!artifact->outOfDateCheckPerformed)
        doOutOfDateCheck(artifact);
    bool fileExists = artifact->isExistingFile;
    bool isDirty = artifact->isOutOfDate;
    m_leaves.remove(artifact);

    // skip source artifacts
    if (!fileExists && artifact->artifactType == Artifact::SourceFile) {
        QString msg = QLatin1String("Can't find source file '%1', referenced in '%2'.");
        setError(msg.arg(artifact->filePath(), artifact->product->rProduct->qbsFile));
        return;
    }

    // skip non-generated artifacts
    if (artifact->artifactType != Artifact::Generated) {
        if (!fileExists)
            qbsWarning() << tr("No transformer builds '%1'").arg(QDir::toNativeSeparators(artifact->filePath()));
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] artifact type %s. Skipping.",
                     qPrintable(toString(artifact->artifactType)));
        finishArtifact(artifact);
        return;
    }

    // Every generated artifact must have a transformer.
    Q_ASSERT(artifact->transformer);

    // skip artifacts that are up-to-date
    if (!isDirty) {
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] Up to date. Skipping.");
        finishArtifact(artifact);
        return;
    }

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
            if (qbsLogLevel(LoggerDebug))
                qbsDebug("[EXEC] Side by side artifact already finished. Skipping.");
            finishArtifact(artifact);
            return;
        case Artifact::Building:
            if (qbsLogLevel(LoggerDebug))
                qbsDebug("[EXEC] Side by side artifact processing. Skipping.");
            artifact->buildState = Artifact::Building;
            return;
        }
    }

    // create the output directories
    QSet<Artifact*>::const_iterator it = artifact->transformer->outputs.begin();
    for (; it != artifact->transformer->outputs.end(); ++it) {
        Artifact *output = *it;
        QDir outDir = QFileInfo(output->filePath()).absoluteDir();
        if (!outDir.exists())
            outDir.mkpath(".");
    }

    // scan all input artifacts
    InputArtifactScanner scanner(artifact, m_inputArtifactScanContext);
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
            insertLeavesAfterAddingDependencies(unbuiltDependencies);
            return;
        }
        if (buildingDependenciesFound)
            return;
    }

    ExecutorJob *job = m_availableJobs.takeFirst();
    artifact->buildState = Artifact::Building;
    m_processingJobs.insert(job, artifact);

    Q_ASSERT_X(artifact->product, Q_FUNC_INFO,
               qPrintable(QString("Generated artifact '%1'' belongs to no product.")
               .arg(QDir::toNativeSeparators(artifact->filePath()))));

    job->run(artifact->transformer.data(), artifact->product);
}

void Executor::finishArtifact(Artifact *leaf)
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[EXEC] finishArtifact " << fileName(leaf);

    leaf->buildState = Artifact::Built;
    foreach (Artifact *parent, leaf->parents) {
        if (parent->buildState != Artifact::Buildable) {
            if (qbsLogLevel(LoggerTrace))
                qbsTrace() << "[EXEC] parent " << fileName(parent) << " build state: " << toString(parent->buildState);
            continue;
        }

        if (isLeaf(parent)) {
            m_leaves.insert(parent, hashDummy);
            if (qbsLogLevel(LoggerTrace))
                qbsTrace() << "[EXEC] finishArtifact adds leaf " << fileName(parent) << " " << toString(parent->buildState);
        } else {
            if (qbsLogLevel(LoggerTrace))
                qbsTrace() << "[EXEC] parent " << fileName(parent) << " build state: " << toString(parent->buildState);
        }
    }

    if (leaf->transformer)
        foreach (Artifact *sideBySideArtifact, leaf->transformer->outputs)
            if (leaf != sideBySideArtifact && sideBySideArtifact->buildState == Artifact::Building)
                finishArtifact(sideBySideArtifact);

    if (m_progressObserver && leaf->artifactType == Artifact::Generated)
        m_progressObserver->incrementProgressValue(BuildEffortCalculator::multiplier(leaf));
}

static void insertLeavesAfterAddingDependencies_recurse(Artifact *artifact, QSet<Artifact *> &seenArtifacts, QMap<Artifact *, QHashDummyValue> &leaves)
{
    if (seenArtifacts.contains(artifact))
        return;
    seenArtifacts += artifact;

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
        if (qbsLogLevel(LoggerDebug))
            qbsDebug() << "[EXEC] adding leaf " << fileName(artifact);
        leaves.insert(artifact, hashDummy);
    }
}

void Executor::insertLeavesAfterAddingDependencies(QVector<Artifact *> dependencies)
{
    QSet<Artifact *> seenArtifacts;
    foreach (Artifact *dependency, dependencies)
        insertLeavesAfterAddingDependencies_recurse(dependency, seenArtifacts, m_leaves);
}

void Executor::cancelJobs()
{
    QList<ExecutorJob *> jobs = m_processingJobs.keys();
    foreach (ExecutorJob *job, jobs)
        job->cancel();
    foreach (ExecutorJob *job, jobs)
        job->waitForFinished();
}

void Executor::addExecutorJobs(int jobNumber)
{
    for (int i = 1; i <= jobNumber; i++) {
        ExecutorJob *job = new ExecutorJob(this);
        if (m_engine)
            job->setMainThreadScriptEngine(m_engine);
        job->setObjectName(QString(QLatin1String("J%1")).arg(i));
        m_availableJobs.append(job);
        connect(job, SIGNAL(error(QString)),
                this, SLOT(onProcessError(QString)));
        connect(job, SIGNAL(success()),
                this, SLOT(onProcessSuccess()));
    }
}

void Executor::removeExecutorJobs(int jobNumber)
{
    if (jobNumber >= m_availableJobs.count()) {
        qDeleteAll(m_availableJobs);
        m_availableJobs.clear();
    } else {
        for (int i = 1; i <= jobNumber; i++) {
            delete m_availableJobs.takeLast();
        }
    }
}

bool Executor::runAutoMoc()
{
    bool autoMocApplied = false;
    foreach (const BuildProduct::Ptr &product, m_productsToBuild) {
        // HACK call the automoc thingy here only if we have use qt/core module
        foreach (const ResolvedModule::ConstPtr &m, product->rProduct->modules) {
            if (m->name == "qt/core") {
                try {
                    autoMocApplied = true;
                    m_autoMoc->apply(product);
                } catch (const Error &e) {
                    setError(e.toString());
                    return false;
                }
                break;
            }
        }
    }
    if (autoMocApplied)
        foreach (const BuildProduct::ConstPtr &product, m_productsToBuild)
            BuildGraph::detectCycle(product);

    return true;
}

void Executor::printScanningMessageOnce()
{
    if (m_printScanningMessage) {
        m_printScanningMessage = false;
        qbsInfo() << DontPrintLogLevel
                  << tr("Scanning source files...");
    }
}

void Executor::onProcessError(QString errorString)
{
    m_buildResult = FailedBuild;
    if (m_buildOptions.keepGoing) {
        qbsWarning() << tr("ignoring error: %1").arg(errorString);
        onProcessSuccess();
    } else {
        setError(errorString);
        finish();
    }
}

void Executor::onProcessSuccess()
{
    ExecutorJob *job = qobject_cast<ExecutorJob *>(sender());
    Q_ASSERT(job);
    Artifact *processedArtifact = m_processingJobs.value(job);
    Q_ASSERT(processedArtifact);
    m_processingJobs.remove(job);
    m_availableJobs.append(job);
    finishArtifact(processedArtifact);

    if (m_state == ExecutorRunning && !run())
        finish();
}

void Executor::finish()
{
    if (m_state == ExecutorIdle)
        return;

    QStringList unbuiltProductNames;
    foreach (BuildProduct::Ptr buildProduct, m_productsToBuild) {
        foreach (Artifact *artifact, buildProduct->targetArtifacts) {
            if (artifact->buildState != Artifact::Built) {
                unbuiltProductNames += buildProduct->rProduct->name;
                break;
            }
        }
    }
    if (unbuiltProductNames.isEmpty()) {
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut << TextColorGreen
                  << "Build done.";
    } else {
        qbsError() << tr("The following products could not be built: %1.").arg(unbuiltProductNames.join(", "));
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut << TextColorRed
                  << "Build failed.";
    }

    resetArtifactsToUntouched();
    setState(ExecutorIdle);
    if (m_progressObserver)
        m_progressObserver->setFinished();
    emit finished();
}

/**
  * Resets the state of all artifacts in the graph to "untouched".
  * This must be done before doing another build.
  */
void Executor::resetArtifactsToUntouched()
{
    foreach (const BuildProduct::Ptr &product, m_productsToBuild) {
        foreach (Artifact *artifact, product->artifacts) {
            artifact->buildState = Artifact::Untouched;
            artifact->outOfDateCheckPerformed = false;
            artifact->isExistingFile = false;
            artifact->isOutOfDate = false;
        }
    }
}

void Executor::prepareBuildGraph(Artifact::BuildState buildState)
{
    foreach (Artifact *root, m_roots)
        prepareBuildGraph_impl(root, buildState);
}

void Executor::prepareBuildGraph_impl(Artifact *artifact, Artifact::BuildState buildState)
{
    if (artifact->buildState != Artifact::Untouched)
        return;

    artifact->buildState = buildState;

    foreach (Artifact *child, artifact->children)
        prepareBuildGraph_impl(child, buildState);
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

void Executor::doOutOfDateCheck()
{
    m_printScanningMessage = true;
    foreach (Artifact *root, m_roots)
        doOutOfDateCheck(root);
}

void Executor::doOutOfDateCheck(Artifact *artifact)
{
    if (artifact->outOfDateCheckPerformed)
        return;

    FileInfo fi(artifact->filePath());
    artifact->isExistingFile = fi.exists();
    if (!artifact->isExistingFile) {
        artifact->isOutOfDate = true;
    } else {
        if (!artifact->inputsScanned && artifact->artifactType == Artifact::Generated) {
            // The file exists but no dependency scan has been performed.
            // This happens if the build graph is removed manually.
            printScanningMessageOnce();
            InputArtifactScanner scanner(artifact, m_inputArtifactScanContext);
            scanner.scan();
        }

        FileTime artifactTimeStamp = fi.lastModified();
        foreach (Artifact *child, artifact->children) {
            if (artifactTimeStamp < timeStamp(child)) {
                artifact->isOutOfDate = true;
                break;
            }
        }
    }

    artifact->outOfDateCheckPerformed = true;
    if (artifact->isOutOfDate)
        artifact->inputsScanned = false;
    foreach (Artifact *child, artifact->children)
        doOutOfDateCheck(child);
}

void Executor::setState(ExecutorState s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void Executor::setError(const QString &errorMessage)
{
    setState(ExecutorError);
    qbsError() << errorMessage;
    cancelJobs();
    emit error();
}

} // namespace Internal
} // namespace qbs
