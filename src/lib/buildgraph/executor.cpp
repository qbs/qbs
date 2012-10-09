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

#include "automoc.h"
#include "executor.h"
#include "executorjob.h"
#include "inputartifactscanner.h"

#include <buildgraph/transformer.h>
#include <language/language.h>
#include <tools/fileinfo.h>
#include <tools/logger.h>
#include <tools/progressobserver.h>

#include <algorithm>

namespace qbs {

static QHashDummyValue hashDummy;

Executor::Executor(int maxJobs)
    : m_scriptEngine(0)
    , m_progressObserver(0)
    , m_runOnceAndForgetMode(false)
    , m_state(ExecutorIdle)
    , m_buildResult(SuccessfulBuild)
    , m_keepGoing(false)
    , m_maximumJobNumber(maxJobs)
{
    m_inputArtifactScanContext = new InputArtifactScannerContext(&m_scanResultCache);
    m_autoMoc = new AutoMoc;
    m_autoMoc->setScanResultCache(&m_scanResultCache);
    if (!m_runOnceAndForgetMode) {
        connect(this, SIGNAL(finished()), SLOT(resetArtifactsToUntouched()));
    }
    qbsDebug("[EXEC] preparing executor for %d jobs in parallel", maxJobs);
    addExecutorJobs(maxJobs);
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

void Executor::build(const QList<BuildProject::Ptr> projectsToBuild, const QStringList &changedFiles, const QStringList &selectedProductNames)
{
    Q_ASSERT(m_state != ExecutorRunning);
    m_leaves.clear();
    m_buildResult = SuccessfulBuild;
    bool success = true;

    setState(ExecutorRunning);
    Artifact::BuildState initialBuildState = changedFiles.isEmpty() ? Artifact::Buildable : Artifact::Built;

    if (!m_scriptEngine) {
        m_scriptEngine = new QScriptEngine(this);
        foreach (ExecutorJob *job, findChildren<ExecutorJob *>())
            job->setMainThreadScriptEngine(m_scriptEngine);
    }

    // determine the products we want to build
    m_projectsToBuild = projectsToBuild;
    if (selectedProductNames.isEmpty()) {
        // Use all products we have in the build graph.
        m_productsToBuild.clear();
        foreach (BuildProject::Ptr project, m_projectsToBuild)
            foreach (BuildProduct::Ptr product, project->buildProducts())
                m_productsToBuild += product;
    } else {
        // Try to find the selected products and their dependencies.
        QHash<QString, BuildProduct::Ptr> productsPerName;
        foreach (BuildProject::Ptr project, m_projectsToBuild)
            foreach (BuildProduct::Ptr product, project->buildProducts())
                productsPerName.insert(product->rProduct->name.toLower(), product);

        QSet<BuildProduct::Ptr> selectedProducts;
        foreach (const QString &productName, selectedProductNames) {
            BuildProduct::Ptr product = productsPerName.value(productName.toLower());
            if (!product) {
                qbsWarning() << "Selected product " << productName << " not found.";
                continue;
            }
            selectedProducts += product;
        }
        QSet<BuildProduct::Ptr> s = selectedProducts;
        do {
            QSet<BuildProduct::Ptr> t;
            foreach (const BuildProduct::Ptr &product, s)
                foreach (BuildProduct *dependency, product->usings)
                    t += productsPerName.value(dependency->rProduct->name.toLower());
            selectedProducts += t;
            s = t;
        } while (!s.isEmpty());
        m_productsToBuild = selectedProducts.toList();
    }

    QList<Artifact *> changedArtifacts;
    foreach (const QString &filePath, changedFiles) {
        QList<Artifact *> artifacts;
        foreach (BuildProject::Ptr project, m_projectsToBuild)
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
            product->rProduct->setupBuildEnvironment(m_scriptEngine, systemEnvironment);
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

    // determine which artifacts are out of date
    const bool changedFilesProvided = !changedFiles.isEmpty();
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

    if (success)
        success = runAutoMoc();
    if (success)
        initLeaves(changedArtifacts);

    if (m_progressObserver)
        m_progressObserver->setProgressRange(0 , m_leaves.count());

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

void Executor::setDryRun(bool b)
{
    foreach (ExecutorJob *job, m_availableJobs)
        job->setDryRun(b);
}

void Executor::setMaximumJobs(int numberOfJobs)
{
    if (numberOfJobs == m_maximumJobNumber)
        return;

    m_maximumJobNumber = numberOfJobs;
    int actualJobNumber = m_availableJobs.count() + m_processingJobs.count();
    if (actualJobNumber > m_maximumJobNumber) {
        removeExecutorJobs(actualJobNumber - m_maximumJobNumber);
    } else {
        addExecutorJobs(m_maximumJobNumber - actualJobNumber);
    }
}

int Executor::maximumJobs() const
{
    return m_maximumJobNumber;
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
        if (m_progressObserver)
            m_progressObserver->incrementProgressValue();
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

    // skip artifacts without transformer
    if (!artifact->transformer) {
        if (!fileExists)
            qbsWarning() << tr("No transformer builds '%1'").arg(QDir::toNativeSeparators(artifact->filePath()));
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] No transformer. Skipping.");
        finishArtifact(artifact);
        return;
    }

    // skip artifacts that are up-to-date
    if (!isDirty) {
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] Up to date. Skipping.");
        finishArtifact(artifact);
        return;
    }

    // skip if side-by-side artifacts have been built
    foreach (Artifact *sideBySideArtifact, artifact->sideBySideArtifacts) {
        if (sideBySideArtifact->transformer != artifact->transformer)
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

    foreach (Artifact *sideBySideArtifact, leaf->sideBySideArtifacts)
        if (leaf->transformer == sideBySideArtifact->transformer && sideBySideArtifact->buildState == Artifact::Building)
            finishArtifact(sideBySideArtifact);
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
    Q_ASSERT(jobNumber > 0);

    for (int i = 1; i <= jobNumber; i++) {
        ExecutorJob *job = new ExecutorJob(this);
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
        foreach (BuildProject::Ptr prj, m_projectsToBuild)
            BuildGraph::detectCycle(prj.data());

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
    if (m_keepGoing) {
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

    setState(ExecutorIdle);
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

} // namespace qbs
