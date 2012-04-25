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

#include <algorithm>

namespace qbs {

static QHashDummyValue hashDummy;

Executor::Executor(int maxJobs)
    : m_scriptEngine(0)
    , m_runOnceAndForgetMode(false)
    , m_state(ExecutorIdle)
    , m_buildResult(SuccessfulBuild)
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
    if (changedFilesProvided) {
        bool newDependencyAdded;
        foreach (Artifact *artifact, changedArtifacts)
            if (!artifact->inputsScanned && artifact->artifactType == Artifact::Generated)
                scanInputArtifacts(artifact, &newDependencyAdded, true);
    } else {
        doOutOfDateCheck();
    }

    if (success)
        success = runAutoMoc();
    if (success)
        initLeaves(changedArtifacts);

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
    bool newDependenciesFound;
    scanInputArtifacts(artifact, &newDependenciesFound);

    // postpone the build of this artifact, if new dependencies found
    if (newDependenciesFound) {
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

    if (!artifact->product)
        qbsError() << QString("BUG: Generated artifact %1 belongs to no product.").arg(QDir::toNativeSeparators(artifact->filePath()));

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
        scanResult->deps += ScanResultCache::Dependency(outFilePath, isLocalInclude);
    }
    scannerPlugin->close(scannerHandle);
    scanResult->valid = true;
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

void Executor::scanInputArtifacts(Artifact *artifact, bool *newDependencyAdded, bool printInfo)
{
    *newDependencyAdded = false;
    if (artifact->inputsScanned)
        return;

    artifact->inputsScanned = true;

    // clear file dependencies; they will be regenerated
    artifact->fileDependencies.clear();

    // Remove all connections to children that do not belong to our transformer.
    // They will be regenerated.
    foreach (Artifact *dependency, artifact->children) {
        if (artifact->transformer->inputs.contains(dependency))
            continue;
        BuildGraph::disconnect(artifact, dependency);
    }

    QSet<Artifact*>::const_iterator it = artifact->transformer->inputs.begin();
    for (; it != artifact->transformer->inputs.end(); ++it) {
        Artifact *inputArtifact = *it;
        QStringList includePaths;

        foreach (const QString &fileTag, inputArtifact->fileTags) {
            QList<ScannerPlugin *> scanners = ScannerPluginManager::scannersForFileTag(fileTag);

            if (printInfo && !scanners.isEmpty())
                qbsInfo() << DontPrintLogLevel
                          << tr("scanning %1").arg(inputArtifact->fileName());

            foreach (ScannerPlugin *scanner, scanners) {
                if (includePaths.isEmpty())
                    includePaths = collectIncludePaths(inputArtifact->configuration->value().value("modules").toMap());

                scanForFileDependencies(scanner, includePaths, artifact, inputArtifact, newDependencyAdded);
            }
        }
    }
}

void Executor::scanForFileDependencies(ScannerPlugin *scannerPlugin, const QStringList &includePaths, Artifact *outputArtifact, Artifact *inputArtifact, bool *newDependencyAdded)
{
    if (qbsLogLevel(LoggerDebug)) {
        qbsDebug("scanning %s [%s]", qPrintable(inputArtifact->filePath()), scannerPlugin->fileTag);
        qbsDebug("    from %s", qPrintable(outputArtifact->filePath()));
    }

    QSet<QString> visitedFilePaths;
    QStringList filePathsToScan;
    filePathsToScan.append(inputArtifact->filePath());

    while (!filePathsToScan.isEmpty()) {
        const QString filePathToBeScanned = filePathsToScan.takeFirst();
        if (visitedFilePaths.contains(filePathToBeScanned))
            continue;
        visitedFilePaths.insert(filePathToBeScanned);

        ScanResultCache::Result scanResult = m_scanResultCache.value(filePathToBeScanned);
        if (!scanResult.valid) {
            bool successfulScan = scanWithScannerPlugin(scannerPlugin, filePathToBeScanned, &scanResult);
            if (!successfulScan)
                continue;
            m_scanResultCache.insert(filePathToBeScanned, scanResult);
        }

        resolveScanResultDependencies(includePaths, outputArtifact, inputArtifact, scanResult, filePathToBeScanned, &filePathsToScan, newDependencyAdded);
    }
}

Executor::Dependency Executor::resolveWithIncludePath(const QString &includePath, const QString &dependencyDirPath, const QString &dependencyFileName, BuildProduct *buildProduct)
{
    QString absDirPath = dependencyDirPath.isEmpty() ? includePath : FileInfo::resolvePath(includePath, dependencyDirPath);
    absDirPath = QDir::cleanPath(absDirPath);

    Executor::Dependency result;
    BuildProject *project = buildProduct->project;
    Artifact *fileDependencyArtifact = 0;
    Artifact *dependencyInProduct = 0;
    Artifact *dependencyInOtherProduct = 0;
    foreach (Artifact *artifact, project->lookupArtifacts(absDirPath, dependencyFileName)) {
        if (artifact->artifactType == Artifact::FileDependency)
            fileDependencyArtifact = artifact;
        else if (artifact->product == buildProduct)
            dependencyInProduct = artifact;
        else
            dependencyInOtherProduct = artifact;
    }

    // prioritize found artifacts
    if ((result.artifact = dependencyInProduct)
        || (result.artifact = dependencyInOtherProduct)
        || (result.artifact = fileDependencyArtifact))
    {
        result.filePath = result.artifact->filePath();
        return result;
    }

    QString absFilePath = FileInfo::resolvePath(absDirPath, dependencyFileName);
    if (FileInfo::exists(absFilePath))
        result.filePath = QDir::cleanPath(absFilePath);

    return result;
}

void Executor::resolveScanResultDependencies(const QStringList &includePaths,
                                             Artifact *processedArtifact,
                                             Artifact *inputArtifact,
                                             const ScanResultCache::Result &scanResult,
                                             const QString &filePathToBeScanned,
                                             QStringList *filePathsToScan,
                                             bool *newDependencyAdded)
{
    QString dependencyDirPath;
    QString dependencyFileName;
    QString baseDirOfInFilePath;
    foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
        const QString &dependencyFilePath = dependency.first;
        const bool isLocalInclude = dependency.second;
        Dependency resolvedDependency;
        if (FileInfo::isAbsolute(dependencyFilePath)) {
            resolvedDependency.filePath = dependencyFilePath;
            goto resolved;
        }

        FileInfo::splitIntoDirectoryAndFileName(dependencyFilePath, &dependencyDirPath, &dependencyFileName);

        if (isLocalInclude) {
            // try base directory of source file
            if (baseDirOfInFilePath.isNull())
                baseDirOfInFilePath = FileInfo::path(filePathToBeScanned);
            resolvedDependency = resolveWithIncludePath(baseDirOfInFilePath, dependencyDirPath, dependencyFileName, inputArtifact->product);
            if (resolvedDependency.isValid())
                goto resolved;
        }

        // try include paths
        foreach (const QString &includePath, includePaths) {
            resolvedDependency = resolveWithIncludePath(includePath, dependencyDirPath, dependencyFileName, inputArtifact->product);
            if (resolvedDependency.isValid())
                goto resolved;
        }

        // we could not resolve the include
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN] unresolved '%s'", qPrintable(dependencyFilePath));
        continue;

resolved:
        filePathsToScan->append(resolvedDependency.filePath);
        handleDependency(processedArtifact, resolvedDependency, newDependencyAdded);
    }
}

void Executor::handleDependency(Artifact *processedArtifact, Dependency &dependency, bool *newDependencyAdded)
{
    BuildProduct *product = processedArtifact->product;
    bool insertIntoProduct = true;
    Q_ASSERT(processedArtifact->artifactType == Artifact::Generated);
    Q_CHECK_PTR(processedArtifact->product);

    if (!dependency.artifact) {
        // The dependency is an existing file but does not exist in the build graph.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]   + '%s'", qPrintable(dependency.filePath));
        dependency.artifact = new Artifact(processedArtifact->project);
        dependency.artifact->artifactType = Artifact::FileDependency;
        dependency.artifact->configuration = processedArtifact->configuration;
        dependency.artifact->setFilePath(dependency.filePath);
        processedArtifact->project->insertFileDependency(dependency.artifact);
    } else if (dependency.artifact->artifactType == Artifact::FileDependency) {
        // The dependency exists in the project's list of file dependencies.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]  ok in deps '%s'", qPrintable(dependency.filePath));
    } else if (dependency.artifact->product == product) {
        // The dependency is in our product.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]  ok in product '%s'", qPrintable(dependency.filePath));
        insertIntoProduct = false;
    } else {
        // The dependency is in some other product.
        BuildProduct *otherProduct = dependency.artifact->product;
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]  found in product '%s': '%s'", qPrintable(otherProduct->rProduct->name), qPrintable(dependency.filePath));
        insertIntoProduct = false;
    }

    if (processedArtifact == dependency.artifact)
        return;

    if (dependency.artifact->artifactType == Artifact::FileDependency) {
        processedArtifact->fileDependencies.insert(dependency.artifact);
    } else {
        if (processedArtifact->children.contains(dependency.artifact))
            return;
        BuildGraph *buildGraph = product->project->buildGraph();
        if (insertIntoProduct && !product->artifacts.contains(dependency.artifact))
            buildGraph->insert(product, dependency.artifact);
        buildGraph->safeConnect(processedArtifact, dependency.artifact);
        *newDependencyAdded = true;
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

    FileInfo fi(artifact->filePath());
    artifact->isExistingFile = fi.exists();
    if (!artifact->isExistingFile) {
        artifact->isOutOfDate = true;
    } else {
        if (!artifact->inputsScanned && artifact->artifactType == Artifact::Generated) {
            // The file exists but no dependency scan has been performed.
            // This happens if the build graph is removed manually.
            bool newDependencyAdded;
            scanInputArtifacts(artifact, &newDependencyAdded, true);
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
    emit error();
}

} // namespace qbs
