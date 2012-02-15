/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "executor.h"
#include "executorjob.h"
#include "scanresultcache.h"
#include "automoc.h"

#include <buildgraph/transformer.h>
#include <language/language.h>
#include <tools/fileinfo.h>
#include <tools/logger.h>
#include <tools/scannerpluginmanager.h>

#ifdef Q_OS_WIN32
#include <Windows.h>
#endif

namespace qbs {

static QHashDummyValue hashDummy;

Executor::Executor(int maxJobs)
    : m_scriptEngine(0)
    , m_runOnceAndForgetMode(false)
    , m_state(ExecutorIdle)
    , m_keepGoing(false)
    , m_maximumJobNumber(maxJobs)
    , m_futureInterface(0)
{
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
}

void Executor::build(const QList<BuildProject::Ptr> projectsToBuild, const QStringList &changedFiles, const QStringList &selectedProductNames,
                     QFutureInterface<bool> &futureInterface)
{
    Q_ASSERT(m_state != ExecutorRunning);
    m_leaves.clear();
    m_futureInterface = &futureInterface;
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
        Artifact *artifact = 0;
        foreach (BuildProject::Ptr project, m_projectsToBuild) {
            artifact = project->findArtifact(filePath);
            if (artifact)
                break;
        }
        if (!artifact) {
            qbsWarning() << QString("Out of date file '%1' provided but not found.").arg(QDir::toNativeSeparators(filePath));
            continue;
        }
        changedArtifacts += artifact;
    }

    // prepare products
    const QProcessEnvironment systemEnvironment = QProcessEnvironment::systemEnvironment();
    foreach (BuildProduct::Ptr product, m_productsToBuild) {
        try {
            product->rProduct->setupBuildEnvironment(m_scriptEngine, systemEnvironment);
        } catch (Error &e) {
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
    if (!changedFilesProvided) {
        doOutOfDateCheck();
    }

    if (success)
        success = runAutoMoc();
    if (success) {
        if (changedFilesProvided) {
            foreach (Artifact *artifact, changedArtifacts)
                scanFileDependencies(artifact);
        } else {
            doDependencyScanTopDown();
            updateBuildGraph(initialBuildState);
        }
        initLeaves(changedArtifacts);
    }

    m_futureInterface->setProgressRange(0 , m_leaves.count());

    if (success) {
        bool stillArtifactsToExecute = run(futureInterface);

        if (!stillArtifactsToExecute)
            finish();
    }
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
    artifact->isExistingFile = FileInfo(artifact->fileName).exists();
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
bool Executor::run(QFutureInterface<bool> &futureInterface)
{
    while (m_state == ExecutorRunning) {
        if (m_futureInterface->isCanceled()) {
            qbsInfo() << "Build canceled.";
            cancelJobs();
            return false;
        }

        futureInterface.setProgressValue(futureInterface.progressValue() + 1);
        if (m_leaves.isEmpty())
            return !m_processingJobs.isEmpty();

        Artifact *leaf = m_leaves.begin().key();
        if (!execute(leaf))
            return true;
    }
    return false;
}

FileTime Executor::timeStamp(Artifact *artifact)
{
    FileTime result = m_timeStampCache.value(artifact);
    if (result.isValid())
        return result;

    FileInfo fi(artifact->fileName);
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

bool Executor::isOutOfDate(Artifact *artifact, bool &fileExists)
{
    FileInfo fi(artifact->fileName);
    fileExists = fi.exists();
    if (!fileExists)
        return true;

    FileTime artifactTimeStamp = fi.lastModified();
    foreach (Artifact *child, artifact->children) {
        if (artifactTimeStamp < timeStamp(child))
            return true;
    }

    return false;
}

/**
  * Returns false if the artifact cannot be executed right now
  * and should be looked at later.
  */
bool Executor::execute(Artifact *artifact)
{
    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[EXEC] " << fileName(artifact);

//    artifact->project->buildGraph()->dump(*artifact->products.begin());

    if (m_availableJobs.isEmpty()) {
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] No jobs available. Trying later.");
        return false;
    }

    if (!artifact->outOfDateCheckPerformed)
        doOutOfDateCheck(artifact);
    bool fileExists = artifact->isExistingFile;
    bool isDirty = artifact->isOutOfDate;
    m_leaves.remove(artifact);

    if (!fileExists && artifact->artifactType == Artifact::SourceFile) {
        setError(QString("Can't find source file '%1'.").arg(artifact->fileName));
        return true;
    }

    if (!artifact->transformer) {
        if (!fileExists)
            qbsWarning() << tr("No transformer builds '%1'").arg(QDir::toNativeSeparators(artifact->fileName));
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] No transformer. Skipping.");
        finishArtifact(artifact);
        return true;
    } else if (!isDirty) {
        if (qbsLogLevel(LoggerDebug))
            qbsDebug("[EXEC] Up to date. Skipping.");
        finishArtifact(artifact);
        return true;
    } else {
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
                return true;
            case Artifact::Building:
                if (qbsLogLevel(LoggerDebug))
                    qbsDebug("[EXEC] Side by side artifact processing. Skipping.");
                artifact->buildState = Artifact::Building;
                return true;
            }
        }

        foreach (Artifact *output, artifact->transformer->outputs) {
            // create the output directories
            QDir outDir = QFileInfo(output->fileName).absoluteDir();
            if (!outDir.exists())
                outDir.mkpath(".");
        }

        ExecutorJob *job = m_availableJobs.takeFirst();
        artifact->buildState = Artifact::Building;
        m_processingJobs.insert(job, artifact);

        if (!artifact->product)
            qbsError() << QString("BUG: Generated artifact %1 belongs to no product.").arg(QDir::toNativeSeparators(artifact->fileName));

        job->run(artifact->transformer.data(), artifact->product);
    }

    return true;
}

void Executor::finishArtifact(Artifact *leaf)
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[EXEC] finishArtifact " << fileName(leaf);

    leaf->buildState = Artifact::Built;
    foreach (Artifact *parent, leaf->parents) {
        if (parent->buildState != Artifact::Buildable)
            continue;

        if (isLeaf(parent)) {
            m_leaves.insert(parent, hashDummy);
            if (qbsLogLevel(LoggerTrace))
                qbsTrace() << "[EXEC] finishArtifact adds leaf " << fileName(parent) << " " << toString(parent->buildState);
        }
    }

    foreach (Artifact *sideBySideArtifact, leaf->sideBySideArtifacts)
        if (leaf->transformer == sideBySideArtifact->transformer && sideBySideArtifact->buildState == Artifact::Building)
            finishArtifact(sideBySideArtifact);
}

void Executor::handleDependencies(Artifact *processedArtifact, Artifact *scannedArtifact, const QSet<QString> &resolvedDependencies)
{
    qbsTrace() << "[DEPSCAN] dependencies found for '" << fileName(scannedArtifact)
               << "' while processing '" << fileName(processedArtifact) << "'";

    foreach (const QString &dependencyFilePath, resolvedDependencies)
        handleDependency(processedArtifact, dependencyFilePath);
}

void Executor::handleDependency(Artifact *processedArtifact, const QString &dependencyFilePath)
{
    BuildProduct *product = processedArtifact->product;
    Artifact *dependency = 0;
    bool insertIntoProduct = true;
    Q_ASSERT(processedArtifact->artifactType == Artifact::Generated);
    Q_CHECK_PTR(processedArtifact->product);
    if ((dependency = product->artifacts.value(dependencyFilePath))) {
        qbsTrace("[DEPSCAN]  ok in product '%s'", qPrintable(dependencyFilePath));
        insertIntoProduct = false;
    } else if ((dependency = processedArtifact->project->dependencyArtifacts().value(dependencyFilePath))) {
        qbsTrace("[DEPSCAN]  ok in deps '%s'", qPrintable(dependencyFilePath));
    } else {
        // try to find the dependency in other products of this project
        foreach (BuildProduct::Ptr otherProduct, product->project->buildProducts()) {
            if (otherProduct == product)
                continue;
            if ((dependency = otherProduct->artifacts.value(dependencyFilePath))) {
                insertIntoProduct = false;
                if (qbsLogLevel(LoggerTrace))
                    qbsTrace("[DEPSCAN]  found in product '%s': '%s'", qPrintable(otherProduct->rProduct->name), qPrintable(dependencyFilePath));
                break;
            }
        }
    }

    // dependency not found in the whole build graph, thus create a new artifact
    if (!dependency) {
        qbsTrace("[DEPSCAN]   + '%s'", qPrintable(dependencyFilePath));
        dependency = new Artifact(processedArtifact->project);
        dependency->artifactType = Artifact::FileDependency;
        dependency->configuration = processedArtifact->configuration;
        dependency->fileName = dependencyFilePath;
        processedArtifact->project->dependencyArtifacts().insert(dependencyFilePath, dependency);
    }

    if (processedArtifact == dependency)
        return;

    if (dependency->artifactType == Artifact::FileDependency) {
        if (qbsLogLevel(LoggerTrace))
            qbsTrace() << "[DEPSCAN] new file dependency " << fileName(dependency);
        processedArtifact->fileDependencies.insert(dependency);
    } else {
        if (processedArtifact->children.contains(dependency))
            return;
        if (qbsLogLevel(LoggerTrace))
            qbsTrace() << "[DEPSCAN] new artifact dependency " << fileName(dependency);
        BuildGraph *buildGraph = product->project->buildGraph();
        if (insertIntoProduct && !product->artifacts.contains(dependency->fileName))
            buildGraph->insert(product, dependency);
        buildGraph->safeConnect(processedArtifact, dependency);
    }
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
    if (jobNumber < 1)
        qbsError() << tr("Maximum job number must be larger than zero.");
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
        foreach (ResolvedModule::Ptr m, product->rProduct->modules) {
            if (m->name == "qt/core") {
                try {
                    autoMocApplied = true;
                    m_autoMoc->apply(product);
                } catch (Error &e) {
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

static bool scanWithScannerPlugin(ScannerPlugin *scannerPlugin,
                                  const QString &filePathToBeScanned,
                                  ScanResultCache *scanResultCache,
                                  ScanResultCache::Result *scanResult)
{
    void *scannerHandle = scannerPlugin->open(filePathToBeScanned.utf16(), 0, 0);
    if (!scannerHandle)
        return false;
    while (true) {
        int flags = 0;
        int length = 0;
        const char *szOutFilePath = scannerPlugin->next(scannerHandle, &length, &flags);
        if (szOutFilePath == 0)
            break;
        QString outFilePath = QString::fromLocal8Bit(szOutFilePath, length);
        if (outFilePath.isEmpty())
            continue;
        bool isLocalInclude = (flags & SC_LOCAL_INCLUDE_FLAG);
        scanResult->deps.insert(outFilePath, isLocalInclude);
    }
    scannerPlugin->close(scannerHandle);
    scanResult->visited = true;
    scanResultCache->insert(filePathToBeScanned, *scanResult);
    return true;
}

static void collectIncludePaths(const QVariantMap &modules, QSet<QString> *collectedPaths)
{
    QMapIterator<QString, QVariant> iterator(modules);
    while (iterator.hasNext()) {
        iterator.next();
        if (iterator.key() == "cpp") {
            QVariant includePathsVariant = iterator .value().toMap().value("includePaths");
            if (includePathsVariant.isValid())
                collectedPaths->unite(QSet<QString>::fromList(includePathsVariant.toStringList()));
        } else {
            collectIncludePaths(iterator .value().toMap().value("modules").toMap(), collectedPaths);
        }
    }
}

static QStringList collectIncludePaths(const QVariantMap &modules)
{
    QSet<QString> collectedPaths;

    collectIncludePaths(modules, &collectedPaths);
    return QStringList(collectedPaths.toList());
}

void Executor::scanFileDependencies(Artifact *processedArtifact)
{
    if (!processedArtifact->transformer)
        return;

    foreach (Artifact *output, processedArtifact->transformer->outputs) {
        // clear the file dependencies - they will be regenerated
        output->fileDependencies.clear();
    }

    foreach (Artifact *inputArtifact, processedArtifact->transformer->inputs) {
        QStringList includePaths;
        foreach (const QString &fileTag, inputArtifact->fileTags) {
            QList<ScannerPlugin *> scanners = ScannerPluginManager::scannersForFileTag(fileTag);
            foreach (ScannerPlugin *scanner, scanners) {
                if (includePaths.isEmpty())
                    includePaths = collectIncludePaths(inputArtifact->configuration->value().value("modules").toMap());

                scanForFileDependencies(scanner, includePaths, processedArtifact, inputArtifact);
            }
        }
    }
}

void Executor::scanForFileDependencies(ScannerPlugin *scannerPlugin, const QStringList &includePaths, Artifact *processedArtifact, Artifact *inputArtifact)
{
    qbsDebug("scanning %s [%s]", qPrintable(inputArtifact->fileName), scannerPlugin->fileTag);
    qbsDebug("  from %s", qPrintable(processedArtifact->fileName));

    QSet<QString> resolvedDependencies;
    QSet<QString> visitedFilePaths;
    QStringList filePathsToScan;
    filePathsToScan.append(inputArtifact->fileName);

    while (!filePathsToScan.isEmpty()) {
        const QString filePathToBeScanned = filePathsToScan.takeFirst();
        if (visitedFilePaths.contains(filePathToBeScanned))
            continue;
        visitedFilePaths.insert(filePathToBeScanned);

        ScanResultCache::Result scanResult = m_scanResultCache.value(filePathToBeScanned);
        if (!scanResult.visited) {
            bool canScan = scanWithScannerPlugin(scannerPlugin, filePathToBeScanned, &m_scanResultCache, &scanResult);
            if (!canScan)
                continue;
        }

        resolveScanResultDependencies(includePaths, inputArtifact, scanResult, filePathToBeScanned, &resolvedDependencies, &filePathsToScan);
    }

    handleDependencies(processedArtifact, inputArtifact, resolvedDependencies);
}

static bool resolveWithIncludePath(const QString &includePath, QString &outFilePath, BuildProduct *buildProduct)
{
    QString filePath = FileInfo::resolvePath(includePath, outFilePath);
    if (buildProduct->artifacts.contains(filePath)) {
        outFilePath = filePath;
        return true;
    } else if (FileInfo::exists(filePath)) {
        outFilePath = filePath;
        return true;
    }
    return false;
}

void Executor::resolveScanResultDependencies(const QStringList &includePaths,
                                             Artifact *inputArtifact,
                                             const ScanResultCache::Result &scanResult,
                                             const QString &filePathToBeScanned,
                                             QSet<QString> *dependencies,
                                             QStringList *filePathsToScan)
{
    QString baseDirOfInFilePath;
    for (QHash<QString, bool>::const_iterator iterator = scanResult.deps.constBegin(); iterator != scanResult.deps.constEnd(); ++iterator) {
        QString outFilePath = iterator.key();
        const bool isLocalInclude = iterator.value();
        bool resolved = FileInfo::isAbsolute(outFilePath);

        if (!resolved && isLocalInclude) {
            // try base directory of source file
            if (baseDirOfInFilePath.isNull())
                baseDirOfInFilePath = FileInfo::path(filePathToBeScanned);
            resolved = resolveWithIncludePath(baseDirOfInFilePath, outFilePath, inputArtifact->product);
        }

        if (!resolved) {
            // try include paths
            foreach (const QString &includePath, includePaths) {
                if (resolveWithIncludePath(includePath, outFilePath, inputArtifact->product)) {
                    resolved = true;
                    break;
                }
            }
        }

        if (resolved) {
            outFilePath = QDir::cleanPath(outFilePath);
            dependencies->insert(outFilePath);
            filePathsToScan->append(outFilePath);
        }
    }
}

void Executor::onProcessError(QString errorString)
{
    if (m_keepGoing) {
        qbsWarning() << tr("ignoring error: %1").arg(errorString);
        onProcessSuccess();
    } else {
        setError(errorString);
        cancelJobs();
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

    if (m_state == ExecutorRunning && !run(*m_futureInterface))
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
        setError(tr("The following products could not be built: %1.").arg(unbuiltProductNames.join(", ")));
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut << TextColorRed
                  << "Build failed.";
        return;
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
    foreach (Artifact *root, m_roots)
        doOutOfDateCheck(root);
}

void Executor::doOutOfDateCheck(Artifact *artifact)
{
    if (artifact->outOfDateCheckPerformed)
        return;
    bool fileExists;
    if (isOutOfDate(artifact, fileExists))
        artifact->isOutOfDate = true;
    artifact->isExistingFile = fileExists;
    artifact->outOfDateCheckPerformed = true;
    foreach (Artifact *child, artifact->children)
        doOutOfDateCheck(child);
}

void Executor::doDependencyScanTopDown()
{
    qbsInfo() << DontPrintLogLevel << "Scanning for file dependencies...";
    QSet<Artifact *> seenArtifacts;
    foreach (Artifact *root, m_roots)
        doDependencyScan_impl(root, seenArtifacts);
}

void Executor::doDependencyScan_impl(Artifact *artifact, QSet<Artifact *> &seenArtifacts)
{
    if (!artifact->transformer || seenArtifacts.contains(artifact))
        return;
    seenArtifacts += artifact;
    if (!artifact->outOfDateCheckPerformed)
        doOutOfDateCheck(artifact);
    if (artifact->isOutOfDate)
        scanFileDependencies(artifact);
    foreach (Artifact *child, artifact->children)
        doDependencyScan_impl(child, seenArtifacts);
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
    emit error();
}

} // namespace qbs
