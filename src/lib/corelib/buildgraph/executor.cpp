/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "executor.h"

#include "buildgraph.h"
#include "emptydirectoriesremover.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "cycledetector.h"
#include "executorjob.h"
#include "inputartifactscanner.h"
#include "productinstaller.h"
#include "rescuableartifactdata.h"
#include "rulecommands.h"
#include "rulenode.h"
#include "rulesevaluationcontext.h"

#include <buildgraph/transformer.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qtimer.h>

#include <algorithm>
#include <climits>
#include <iterator>
#include <utility>

namespace qbs {
namespace Internal {

bool Executor::ComparePriority::operator() (const BuildGraphNode *x, const BuildGraphNode *y) const
{
    return x->product->buildData->buildPriority < y->product->buildData->buildPriority;
}


Executor::Executor(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_productInstaller(0)
    , m_logger(logger)
    , m_progressObserver(0)
    , m_state(ExecutorIdle)
    , m_cancelationTimer(new QTimer(this))
    , m_doTrace(logger.traceEnabled())
    , m_doDebug(logger.debugEnabled())
{
    m_inputArtifactScanContext = new InputArtifactScannerContext;
    m_cancelationTimer->setSingleShot(false);
    m_cancelationTimer->setInterval(1000);
    connect(m_cancelationTimer, &QTimer::timeout, this, &Executor::checkForCancellation);
}

Executor::~Executor()
{
    // jobs must be destroyed before deleting the shared scan result cache
    for (ExecutorJob *job : qAsConst(m_availableJobs))
        delete job;
    for (ExecutorJob *job : m_processingJobs.keys())
        delete job;
    delete m_inputArtifactScanContext;
    delete m_productInstaller;
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
    newest = std::max(fileInfo.lastModified(), fileInfo.lastStatusChange());
    if (!fileInfo.isDir())
        return newest;
    const QStringList dirContents = QDir(filePath)
            .entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &curFileName : dirContents) {
        const FileTime ft = recursiveFileTime(filePath + QLatin1Char('/') + curFileName);
        if (ft > newest)
            newest = ft;
    }
    return newest;
}

void Executor::retrieveSourceFileTimestamp(Artifact *artifact) const
{
    QBS_CHECK(artifact->artifactType == Artifact::SourceFile);

    if (m_buildOptions.changedFiles().isEmpty())
        artifact->setTimestamp(recursiveFileTime(artifact->filePath()));
    else if (m_buildOptions.changedFiles().contains(artifact->filePath()))
        artifact->setTimestamp(FileTime::currentTime());
    else if (!artifact->timestamp().isValid())
        artifact->setTimestamp(recursiveFileTime(artifact->filePath()));

    artifact->timestampRetrieved = true;
    if (!artifact->timestamp().isValid())
        throw ErrorInfo(Tr::tr("Source file '%1' has disappeared.").arg(artifact->filePath()));
}

void Executor::build()
{
    try {
        m_partialBuild = m_productsToBuild.count() != m_project->allProducts().count();
        doBuild();
    } catch (const ErrorInfo &e) {
        handleError(e);
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

class ProductPrioritySetter
{
    const TopLevelProject *m_topLevelProject;
    unsigned int m_priority;
    Set<ResolvedProductPtr> m_seenProducts;
public:
    ProductPrioritySetter(const TopLevelProject *tlp)
        : m_topLevelProject(tlp)
    {
    }

    void apply()
    {
        const QList<ResolvedProductPtr> &allProducts = m_topLevelProject->allProducts();
        Set<ResolvedProductPtr> allDependencies;
        for (const ResolvedProductPtr &product : allProducts)
            allDependencies += product->dependencies;
        const Set<ResolvedProductPtr> rootProducts
                = Set<ResolvedProductPtr>::fromList(allProducts) - allDependencies;
        m_priority = UINT_MAX;
        m_seenProducts.clear();
        for (const ResolvedProductPtr &rootProduct : rootProducts)
            traverse(rootProduct);
    }

private:
    void traverse(const ResolvedProductPtr &product)
    {
        if (!m_seenProducts.insert(product).second)
            return;
        for (const ResolvedProductPtr &dependency : qAsConst(product->dependencies))
            traverse(dependency);
        if (!product->buildData)
            return;
        product->buildData->buildPriority = m_priority--;
    }
};

void Executor::doBuild()
{
    if (m_buildOptions.maxJobCount() <= 0) {
        m_buildOptions.setMaxJobCount(BuildOptions::defaultMaxJobCount());
        m_logger.qbsDebug() << "max job count not explicitly set, using value of "
                            << m_buildOptions.maxJobCount();
    }
    QBS_CHECK(m_state == ExecutorIdle);
    m_leaves = Leaves();
    m_changedSourceArtifacts.clear();
    m_error.clear();
    m_explicitlyCanceled = false;
    m_activeFileTags = FileTags::fromStringList(m_buildOptions.activeFileTags());
    m_tagsOfFilesToConsider.clear();
    m_tagsNeededForFilesToConsider.clear();
    m_productsOfFilesToConsider.clear();
    m_artifactsRemovedFromDisk.clear();

    // TODO: The "filesToConsider" thing is badly designed; we should know exactly which artifact
    //       it is. Remove this from the BuildOptions class and introduce Project::buildSomeFiles()
    //       instead.
    const QStringList &filesToConsider = m_buildOptions.filesToConsider();
    if (!filesToConsider.isEmpty()) {
        for (const QString &fileToConsider : filesToConsider) {
            const QList<FileResourceBase *> &files
                    = m_project->buildData->lookupFiles(fileToConsider);
            for (const FileResourceBase * const file : files) {
                const Artifact * const artifact = dynamic_cast<const Artifact *>(file);
                if (artifact && m_productsToBuild.contains(artifact->product.lock())) {
                    m_tagsOfFilesToConsider.unite(artifact->fileTags());
                    m_productsOfFilesToConsider << artifact->product.lock();
                }
            }
        }
    }

    setState(ExecutorRunning);

    if (m_productsToBuild.isEmpty()) {
        m_logger.qbsTrace() << "No products to build, finishing.";
        QTimer::singleShot(0, this, &Executor::finish); // Don't call back on the caller.
        return;
    }

    doSanityChecks();
    QBS_CHECK(!m_project->buildData->evaluationContext);
    m_project->buildData->evaluationContext
            = RulesEvaluationContextPtr(new RulesEvaluationContext(m_logger));
    m_evalContext = m_project->buildData->evaluationContext;

    m_elapsedTimeRules = m_elapsedTimeScanners = m_elapsedTimeInstalling = 0;
    m_evalContext->engine()->enableProfiling(m_buildOptions.logElapsedTime());

    InstallOptions installOptions;
    installOptions.setDryRun(m_buildOptions.dryRun());
    installOptions.setInstallRoot(m_productsToBuild.first()->moduleProperties
                                  ->qbsPropertyValue(QLatin1String("installRoot")).toString());
    installOptions.setKeepGoing(m_buildOptions.keepGoing());
    m_productInstaller = new ProductInstaller(m_project, m_productsToBuild, installOptions,
                                              m_progressObserver, m_logger);
    if (m_buildOptions.removeExistingInstallation())
        m_productInstaller->removeInstallRoot();

    addExecutorJobs();
    prepareAllNodes();
    prepareProducts();
    setupRootNodes();
    prepareReachableNodes();
    setupProgressObserver();
    initLeaves();
    if (!scheduleJobs()) {
        m_logger.qbsTrace() << "Nothing to do at all, finishing.";
        QTimer::singleShot(0, this, &Executor::finish); // Don't call back on the caller.
    }
    if (m_progressObserver)
        m_cancelationTimer->start();
}

void Executor::setBuildOptions(const BuildOptions &buildOptions)
{
    m_buildOptions = buildOptions;
}


void Executor::initLeaves()
{
    updateLeaves(m_roots);
}

void Executor::updateLeaves(const NodeSet &nodes)
{
    NodeSet seenNodes;
    for (BuildGraphNode * const node : nodes)
        updateLeaves(node, seenNodes);
}

void Executor::updateLeaves(BuildGraphNode *node, NodeSet &seenNodes)
{
    if (!seenNodes.insert(node).second)
        return;

    // Artifacts that appear in the build graph after
    // prepareBuildGraph() has been called, must be initialized.
    if (node->buildState == BuildGraphNode::Untouched) {
        node->buildState = BuildGraphNode::Buildable;
        Artifact *artifact = dynamic_cast<Artifact *>(node);
        if (artifact && artifact->artifactType == Artifact::SourceFile)
            retrieveSourceFileTimestamp(artifact);
    }

    bool isLeaf = true;
    for (BuildGraphNode *child : qAsConst(node->children)) {
        if (child->buildState != BuildGraphNode::Built) {
            isLeaf = false;
            updateLeaves(child, seenNodes);
        }
    }

    if (isLeaf) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] adding leaf " << node->toString();
        m_leaves.push(node);
    }
}

// Returns true if some artifacts are still waiting to be built or currently building.
bool Executor::scheduleJobs()
{
    QBS_CHECK(m_state == ExecutorRunning);
    while (!m_leaves.empty() && !m_availableJobs.isEmpty()) {
        BuildGraphNode * const nodeToBuild = m_leaves.top();
        m_leaves.pop();

        switch (nodeToBuild->buildState) {
        case BuildGraphNode::Untouched:
            QBS_ASSERT(!"untouched node in leaves list",
                       qDebug("%s", qPrintable(nodeToBuild->toString())));
            break;
        case BuildGraphNode::Buildable:
            // This is the only state in which we want to build a node.
            nodeToBuild->accept(this);
            break;
        case BuildGraphNode::Building:
            if (m_doDebug) {
                m_logger.qbsDebug() << "[EXEC] " << nodeToBuild->toString();
                m_logger.qbsDebug() << "[EXEC] node is currently being built. Skipping.";
            }
            break;
        case BuildGraphNode::Built:
            if (m_doDebug) {
                m_logger.qbsDebug() << "[EXEC] " << nodeToBuild->toString();
                m_logger.qbsDebug() << "[EXEC] node already built. Skipping.";
            }
            break;
        }
    }
    return !m_leaves.empty() || !m_processingJobs.isEmpty();
}

bool Executor::isUpToDate(Artifact *artifact) const
{
    QBS_CHECK(artifact->artifactType == Artifact::Generated);

    if (m_doDebug) {
        m_logger.qbsDebug() << "[UTD] check " << artifact->filePath() << " "
                            << artifact->timestamp().toString();
    }

    if (m_buildOptions.forceTimestampCheck()) {
        artifact->setTimestamp(FileInfo(artifact->filePath()).lastModified());
        if (m_doDebug) {
            m_logger.qbsDebug() << "[UTD] timestamp retrieved from filesystem: "
                                << artifact->timestamp().toString();
        }
    }

    if (!artifact->timestamp().isValid()) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[UTD] invalid timestamp. Out of date.";
        return false;
    }

    for (Artifact *childArtifact : filterByType<Artifact>(artifact->children)) {
        QBS_CHECK(childArtifact->timestamp().isValid());
        if (m_doDebug) {
            m_logger.qbsDebug() << "[UTD] child timestamp "
                                << childArtifact->timestamp().toString() << " "
                                << childArtifact->filePath();
        }
        if (artifact->timestamp() < childArtifact->timestamp())
            return false;
    }

    for (FileDependency *fileDependency : qAsConst(artifact->fileDependencies)) {
        if (!fileDependency->timestamp().isValid()) {
            FileInfo fi(fileDependency->filePath());
            fileDependency->setTimestamp(fi.lastModified());
            if (!fileDependency->timestamp().isValid()) {
                if (m_doDebug) {
                    m_logger.qbsDebug() << "[UTD] file dependency doesn't exist "
                                        << fileDependency->filePath();
                }
                return false;
            }
        }
        if (m_doDebug) {
            m_logger.qbsDebug() << "[UTD] file dependency timestamp "
                                << fileDependency->timestamp().toString() << " "
                                << fileDependency->filePath();
        }
        if (artifact->timestamp() < fileDependency->timestamp())
            return false;
    }

    return true;
}

bool Executor::mustExecuteTransformer(const TransformerPtr &transformer) const
{
    if (transformer->alwaysRun)
        return true;
    bool hasAlwaysUpdatedArtifacts = false;
    for (Artifact *artifact : qAsConst(transformer->outputs)) {
        if (artifact->alwaysUpdated)
            hasAlwaysUpdatedArtifacts = true;
        else if (!m_buildOptions.forceTimestampCheck())
            continue;
        if (!isUpToDate(artifact))
            return true;
    }

    // If all artifacts in a transformer have "alwaysUpdated" set to false, that transformer
    // is always run.
    return !hasAlwaysUpdatedArtifacts;
}

void Executor::buildArtifact(Artifact *artifact)
{
    if (m_doDebug)
        m_logger.qbsDebug() << "[EXEC] " << relativeArtifactFileName(artifact);

    QBS_CHECK(artifact->buildState == BuildGraphNode::Buildable);

    if (artifact->artifactType != Artifact::SourceFile && !checkNodeProduct(artifact))
        return;

    // skip artifacts without transformer
    if (artifact->artifactType != Artifact::Generated) {
        // For source artifacts, that were not reachable when initializing the build, we must
        // retrieve timestamps. This can happen, if a dependency that's added during the build
        // makes the source artifact reachable.
        if (artifact->artifactType == Artifact::SourceFile && !artifact->timestampRetrieved)
            retrieveSourceFileTimestamp(artifact);

        if (m_doDebug)
            m_logger.qbsDebug() << QString::fromLatin1("[EXEC] artifact type %1. Skipping.")
                                   .arg(toString(artifact->artifactType));
        finishArtifact(artifact);
        return;
    }

    // Every generated artifact must have a transformer.
    QBS_CHECK(artifact->transformer);
    potentiallyRunTransformer(artifact->transformer);
}

void Executor::executeRuleNode(RuleNode *ruleNode)
{
    AccumulatingTimer rulesTimer(m_buildOptions.logElapsedTime() ? &m_elapsedTimeRules : nullptr);

    if (!checkNodeProduct(ruleNode))
        return;

    QBS_CHECK(!m_evalContext->engine()->isActive());
    ArtifactSet changedInputArtifacts;
    if (ruleNode->rule()->isDynamic()) {
        for (Artifact * const artifact : qAsConst(m_changedSourceArtifacts)) {
            if (artifact->product != ruleNode->product)
                continue;
            if (ruleNode->rule()->acceptsAsInput(artifact))
                changedInputArtifacts += artifact;
        }
        for (Artifact *artifact : filterByType<Artifact>(ruleNode->product->buildData->nodes)) {
            if (artifact->artifactType == Artifact::SourceFile)
                continue;
            if (ruleNode->rule()->acceptsAsInput(artifact)) {
                for (const Artifact * const parent : artifact->parentArtifacts()) {
                    if (parent->transformer->rule != ruleNode->rule())
                        continue;
                    if (!parent->alwaysUpdated)
                        continue;
                    if (parent->timestamp() < artifact->timestamp()) {
                        changedInputArtifacts += artifact;
                        break;
                    }
                }
            }
        }
    }

    RuleNode::ApplicationResult result;
    ruleNode->apply(m_logger, changedInputArtifacts, &result);

    if (result.upToDate) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] " << ruleNode->toString()
                                << " is up to date. Skipping.";
    } else {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] " << ruleNode->toString();
        const WeakPointer<ResolvedProduct> &product = ruleNode->product;
        Set<RuleNode *> parentRules;
        if (!result.createdNodes.isEmpty()) {
            for (BuildGraphNode *parent : qAsConst(ruleNode->parents)) {
                if (RuleNode *parentRule = dynamic_cast<RuleNode *>(parent))
                    parentRules += parentRule;
            }
        }
        for (BuildGraphNode *node : qAsConst(result.createdNodes)) {
            if (m_doDebug)
                m_logger.qbsDebug() << "[EXEC] rule created " << node->toString();
            loggedConnect(node, ruleNode, m_logger);
            Artifact *outputArtifact = dynamic_cast<Artifact *>(node);
            if (!outputArtifact)
                continue;
            if (outputArtifact->fileTags().intersects(product->fileTags))
                product->buildData->roots += outputArtifact;

            for (Artifact *inputArtifact : qAsConst(outputArtifact->transformer->inputs))
                loggedConnect(ruleNode, inputArtifact, m_logger);

            for (RuleNode *parentRule : qAsConst(parentRules))
                loggedConnect(parentRule, outputArtifact, m_logger);
        }
        updateLeaves(result.createdNodes);
        updateLeaves(result.invalidatedNodes);
    }
    finishNode(ruleNode);
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();
}

void Executor::finishJob(ExecutorJob *job, bool success)
{
    QBS_CHECK(job);
    QBS_CHECK(m_state != ExecutorIdle);

    const JobMap::Iterator it = m_processingJobs.find(job);
    QBS_CHECK(it != m_processingJobs.end());
    const TransformerPtr transformer = it.value();
    m_processingJobs.erase(it);
    m_availableJobs.append(job);
    if (success) {
        m_project->buildData->isDirty = true;
        for (Artifact * const artifact : qAsConst(transformer->outputs)) {
            if (artifact->alwaysUpdated) {
                artifact->setTimestamp(FileTime::currentTime());
                if (m_buildOptions.forceOutputCheck()
                        && !m_buildOptions.dryRun() && !FileInfo(artifact->filePath()).exists()) {
                    if (transformer->rule) {
                        if (!transformer->rule->name.isEmpty()) {
                            throw ErrorInfo(tr("Rule '%1' declares artifact '%2', "
                                               "but the artifact was not produced.")
                                            .arg(transformer->rule->name, artifact->filePath()));
                        }
                        throw ErrorInfo(tr("Rule declares artifact '%1', "
                                           "but the artifact was not produced.")
                                        .arg(artifact->filePath()));
                    }
                    throw ErrorInfo(tr("Transformer declares artifact '%1', "
                                       "but the artifact was not produced.")
                                    .arg(artifact->filePath()));
                }
            } else {
                artifact->setTimestamp(FileInfo(artifact->filePath()).lastModified());
            }
        }
        finishTransformer(transformer);
    }

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

static bool allChildrenBuilt(BuildGraphNode *node)
{
    return std::all_of(node->children.cbegin(), node->children.cend(),
                       std::mem_fn(&BuildGraphNode::isBuilt));
}

void Executor::finishNode(BuildGraphNode *leaf)
{
    leaf->buildState = BuildGraphNode::Built;
    for (BuildGraphNode * const parent : qAsConst(leaf->parents)) {
        if (parent->buildState != BuildGraphNode::Buildable) {
            if (m_doTrace) {
                m_logger.qbsTrace() << "[EXEC] parent " << parent->toString()
                                    << " build state: " << toString(parent->buildState);
            }
            continue;
        }

        if (allChildrenBuilt(parent)) {
            m_leaves.push(parent);
            if (m_doTrace) {
                m_logger.qbsTrace() << "[EXEC] finishNode adds leaf "
                        << parent->toString() << " " << toString(parent->buildState);
            }
        } else {
            if (m_doTrace) {
                m_logger.qbsTrace() << "[EXEC] parent " << parent->toString()
                                    << " build state: " << toString(parent->buildState);
            }
        }
    }
}

void Executor::finishArtifact(Artifact *leaf)
{
    QBS_CHECK(leaf);
    if (m_doTrace)
        m_logger.qbsTrace() << "[EXEC] finishArtifact " << relativeArtifactFileName(leaf);

    finishNode(leaf);
}

QString Executor::configString() const
{
    return tr(" for configuration %1").arg(m_project->id());
}

bool Executor::transformerHasMatchingOutputTags(const TransformerConstPtr &transformer) const
{
    if (m_activeFileTags.isEmpty())
        return true; // No filtering requested.

    return std::any_of(transformer->outputs.cbegin(), transformer->outputs.cend(),
                       [this](const Artifact *a) { return artifactHasMatchingOutputTags(a); });
}

bool Executor::artifactHasMatchingOutputTags(const Artifact *artifact) const
{
    return m_activeFileTags.intersects(artifact->fileTags())
            || m_tagsNeededForFilesToConsider.intersects(artifact->fileTags());
}

bool Executor::transformerHasMatchingInputFiles(const TransformerConstPtr &transformer) const
{
    if (m_buildOptions.filesToConsider().isEmpty())
        return true; // No filtering requested.
    if (!m_productsOfFilesToConsider.contains(transformer->product()))
        return false;
    if (transformer->inputs.isEmpty())
        return true;
    for (const Artifact * const input : qAsConst(transformer->inputs)) {
        for (const QString &filePath : m_buildOptions.filesToConsider()) {
            if (input->filePath() == filePath
                    || input->fileTags().intersects(m_tagsNeededForFilesToConsider)) {
                return true;
            }
        }
    }

    return false;
}

void Executor::cancelJobs()
{
    if (m_state == ExecutorCanceling)
        return;
    m_logger.qbsTrace() << "Canceling all jobs.";
    setState(ExecutorCanceling);
    for (ExecutorJob *job : m_processingJobs.keys())
        job->cancel();
}

void Executor::setupProgressObserver()
{
    if (!m_progressObserver)
        return;
    int totalEffort = 1; // For the effort after the last rule application;
    for (const ResolvedProductConstPtr &product : qAsConst(m_productsToBuild)) {
        QBS_CHECK(product->buildData);
        const auto filtered = filterByType<RuleNode>(product->buildData->nodes);
        totalEffort += std::distance(filtered.begin(), filtered.end());
    }
    m_progressObserver->initialize(tr("Building%1").arg(configString()), totalEffort);
}

void Executor::doSanityChecks()
{
    QBS_CHECK(m_project);
    QBS_CHECK(!m_productsToBuild.isEmpty());
    for (const ResolvedProductConstPtr &product : qAsConst(m_productsToBuild)) {
        QBS_CHECK(product->buildData);
        QBS_CHECK(product->topLevelProject() == m_project.get());
    }
}

void Executor::handleError(const ErrorInfo &error)
{
    for (const ErrorItem &ei : error.items())
        m_error.append(ei);
    if (m_processingJobs.isEmpty())
        finish();
    else
        cancelJobs();
}

void Executor::addExecutorJobs()
{
    m_logger.qbsDebug() << QString::fromLatin1("[EXEC] preparing executor for %1 jobs in parallel")
                           .arg(m_buildOptions.maxJobCount());
    for (int i = 1; i <= m_buildOptions.maxJobCount(); i++) {
        ExecutorJob *job = new ExecutorJob(m_logger, this);
        job->setMainThreadScriptEngine(m_evalContext->engine());
        job->setObjectName(QString::fromLatin1("J%1").arg(i));
        job->setDryRun(m_buildOptions.dryRun());
        job->setEchoMode(m_buildOptions.echoMode());
        m_availableJobs.append(job);
        connect(job, &ExecutorJob::reportCommandDescription,
                this, &Executor::reportCommandDescription);
        connect(job, &ExecutorJob::reportProcessResult, this, &Executor::reportProcessResult);
        connect(job, &ExecutorJob::finished,
                this, &Executor::onJobFinished, Qt::QueuedConnection);
    }
}

void Executor::rescueOldBuildData(Artifact *artifact, bool *childrenAdded = 0)
{
    if (childrenAdded)
        *childrenAdded = false;
    if (!artifact->oldDataPossiblyPresent)
        return;
    artifact->oldDataPossiblyPresent = false;
    if (artifact->artifactType != Artifact::Generated)
        return;

    ResolvedProduct * const product = artifact->product.get();
    AllRescuableArtifactData::Iterator it
            = product->buildData->rescuableArtifactData.find(artifact->filePath());
    if (it == product->buildData->rescuableArtifactData.end())
        return;

    const RescuableArtifactData &rad = it.value();
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << QString::fromLatin1("[BG] Attempting to rescue data of "
                                                   "artifact '%1'").arg(artifact->fileName());
    }

    typedef std::pair<Artifact *, bool> ChildArtifactData;
    QList<ChildArtifactData> childrenToConnect;
    bool canRescue = commandListsAreEqual(artifact->transformer->commands, rad.commands);
    if (canRescue) {
        ResolvedProductPtr pseudoProduct = ResolvedProduct::create();
        for (const RescuableArtifactData::ChildData &cd : qAsConst(rad.children)) {
            pseudoProduct->name = cd.productName;
            pseudoProduct->multiplexConfigurationId = cd.productMultiplexId;
            Artifact * const child = lookupArtifact(pseudoProduct, m_project->buildData.data(),
                                                    cd.childFilePath, true);
            if (artifact->children.contains(child))
                continue;
            if (!child)  {
                // If a child has disappeared, we must re-build even if the commands
                // are the same. Example: Header file included in cpp file does not exist anymore.
                canRescue = false;
                if (m_logger.traceEnabled())
                    m_logger.qbsTrace() << "Former child artifact '" << cd.childFilePath
                                        << "' does not exist anymore";
                break;
            }
            // TODO: Shouldn't addedByScanner always be true here? Otherwise the child would be
            //       in the list already, no?
            childrenToConnect << std::make_pair(child, cd.addedByScanner);
        }

        if (canRescue) {
            const TypeFilter<Artifact> childArtifacts(artifact->children);
            const int newChildCount = childrenToConnect.count()
                    + std::distance(childArtifacts.begin(), childArtifacts.end());
            QBS_CHECK(newChildCount >= rad.children.count());
            if (newChildCount > rad.children.count()) {
                canRescue = false;
                if (m_logger.traceEnabled())
                    m_logger.qbsTrace() << "Artifact has children not present in rescue data";
            }
        }
    } else {
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "Transformer commands changed.";
    }

    if (canRescue) {
        artifact->transformer->propertiesRequestedInPrepareScript
                = rad.propertiesRequestedInPrepareScript;
        artifact->transformer->propertiesRequestedInCommands
                = rad.propertiesRequestedInCommands;
        artifact->transformer->propertiesRequestedFromArtifactInPrepareScript
                = rad.propertiesRequestedFromArtifactInPrepareScript;
        artifact->transformer->propertiesRequestedFromArtifactInCommands
                = rad.propertiesRequestedFromArtifactInCommands;
        artifact->setTimestamp(rad.timeStamp);
        if (childrenAdded && !childrenToConnect.isEmpty())
            *childrenAdded = true;
        for (const ChildArtifactData &cad : qAsConst(childrenToConnect)) {
            safeConnect(artifact, cad.first, m_logger);
            if (cad.second)
                artifact->childrenAddedByScanner << cad.first;
        }
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "Data was rescued.";
    } else {
        removeGeneratedArtifactFromDisk(artifact, m_logger);
        m_artifactsRemovedFromDisk << artifact->filePath();
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "Data not rescued.";
    }
    product->buildData->rescuableArtifactData.erase(it);
}

bool Executor::checkForUnbuiltDependencies(Artifact *artifact)
{
    bool buildingDependenciesFound = false;
    NodeSet unbuiltDependencies;
    for (BuildGraphNode * const dependency : qAsConst(artifact->children)) {
        switch (dependency->buildState) {
        case BuildGraphNode::Untouched:
        case BuildGraphNode::Buildable:
            if (m_logger.debugEnabled()) {
                m_logger.qbsDebug() << "[EXEC] unbuilt dependency: "
                                    << dependency->toString();
            }
            unbuiltDependencies += dependency;
            break;
        case BuildGraphNode::Building: {
            if (m_logger.debugEnabled()) {
                m_logger.qbsDebug() << "[EXEC] dependency in state 'Building': "
                                    << dependency->toString();
            }
            buildingDependenciesFound = true;
            break;
        }
        case BuildGraphNode::Built:
            // do nothing
            break;
        }
    }
    if (!unbuiltDependencies.isEmpty()) {
        artifact->inputsScanned = false;
        updateLeaves(unbuiltDependencies);
        return true;
    }
    if (buildingDependenciesFound) {
        artifact->inputsScanned = false;
        return true;
    }
    return false;
}

void Executor::potentiallyRunTransformer(const TransformerPtr &transformer)
{
    for (Artifact * const output : qAsConst(transformer->outputs)) {
        // Rescuing build data can introduce new dependencies, potentially delaying execution of
        // this transformer.
        bool childrenAddedDueToRescue;
        rescueOldBuildData(output, &childrenAddedDueToRescue);
        if (childrenAddedDueToRescue && checkForUnbuiltDependencies(output))
            return;
    }

    if (!transformerHasMatchingOutputTags(transformer)) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] file tags do not match. Skipping.";
        finishTransformer(transformer);
        return;
    }

    if (!transformerHasMatchingInputFiles(transformer)) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] input files do not match. Skipping.";
        finishTransformer(transformer);
        return;
    }

    const bool mustExecute = mustExecuteTransformer(transformer);
    if (!mustExecute && !m_buildOptions.forceTimestampCheck()) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] Up to date. Skipping.";
        finishTransformer(transformer);
        return;
    }

    for (Artifact * const output : qAsConst(transformer->outputs)) {
        // Scan all input artifacts. If new dependencies were found during scanning, delay
        // execution of this transformer.
        InputArtifactScanner scanner(output, m_inputArtifactScanContext, m_logger);
        AccumulatingTimer scanTimer(m_buildOptions.logElapsedTime()
                                    ? &m_elapsedTimeScanners : nullptr);
        scanner.scan();
        scanTimer.stop();
        if (scanner.newDependencyAdded() && checkForUnbuiltDependencies(output))
            return;
    }

    if (!mustExecute) {
        if (m_doDebug)
            m_logger.qbsDebug() << "[EXEC] Up to date. Skipping.";
        finishTransformer(transformer);
        return;
    }

    if (m_buildOptions.executeRulesOnly())
        finishTransformer(transformer);
    else
        runTransformer(transformer);
}

void Executor::runTransformer(const TransformerPtr &transformer)
{
    QBS_CHECK(transformer);

    // create the output directories
    if (!m_buildOptions.dryRun()) {
        for (Artifact * const output : qAsConst(transformer->outputs)) {
            QDir outDir = QFileInfo(output->filePath()).absoluteDir();
            if (!outDir.exists() && !outDir.mkpath(QLatin1String("."))) {
                    throw ErrorInfo(tr("Failed to create directory '%1'.")
                                    .arg(QDir::toNativeSeparators(outDir.absolutePath())));
            }
        }
    }

    QBS_CHECK(!m_availableJobs.isEmpty());
    ExecutorJob *job = m_availableJobs.takeFirst();
    for (Artifact * const artifact : qAsConst(transformer->outputs))
        artifact->buildState = BuildGraphNode::Building;
    m_processingJobs.insert(job, transformer);
    job->run(transformer.get());
}

void Executor::finishTransformer(const TransformerPtr &transformer)
{
    for (Artifact * const artifact : qAsConst(transformer->outputs)) {
        possiblyInstallArtifact(artifact);
        finishArtifact(artifact);
    }
}

void Executor::possiblyInstallArtifact(const Artifact *artifact)
{
    AccumulatingTimer installTimer(m_buildOptions.logElapsedTime()
                                   ? &m_elapsedTimeInstalling : nullptr);

    if (m_buildOptions.install() && !m_buildOptions.executeRulesOnly()
            && (m_activeFileTags.isEmpty() || artifactHasMatchingOutputTags(artifact))
            && artifact->properties->qbsPropertyValue(QLatin1String("install")).toBool()) {
            m_productInstaller->copyFile(artifact);
    }
}

void Executor::onJobFinished(const qbs::ErrorInfo &err)
{
    try {
        ExecutorJob * const job = qobject_cast<ExecutorJob *>(sender());
        QBS_CHECK(job);
        if (m_evalContext->engine()->isActive()) {
            m_logger.qbsDebug() << "Executor job finished while rule execution is pausing. "
                                   "Delaying slot execution.";
            QTimer::singleShot(0, job, [job, err] { job->finished(err); });
            return;
        }

        if (err.hasError()) {
            if (m_buildOptions.keepGoing()) {
                ErrorInfo fullWarning(err);
                fullWarning.prepend(Tr::tr("Ignoring the following errors on user request:"));
                m_logger.printWarning(fullWarning);
            } else {
                if (!m_error.hasError())
                    m_error = err; // All but the first one could be due to canceling.
            }
        }

        finishJob(job, !err.hasError());
    } catch (const ErrorInfo &error) {
        handleError(error);
    }
}

void Executor::checkForUnbuiltProducts()
{
    if (m_buildOptions.executeRulesOnly())
        return;
    QList<ResolvedProductPtr> unbuiltProducts;
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild)) {
        bool productBuilt = true;
        for (BuildGraphNode *rootNode : qAsConst(product->buildData->roots)) {
            if (rootNode->buildState != BuildGraphNode::Built) {
                productBuilt = false;
                unbuiltProducts += product;
                break;
            }
        }
        if (productBuilt) {
            // Any element still left after a successful build has not been re-created
            // by any rule and therefore does not exist anymore as an artifact.
            for (auto it = product->buildData->rescuableArtifactData.cbegin();
                 it != product->buildData->rescuableArtifactData.cend(); ++it) {
                removeGeneratedArtifactFromDisk(it.key(), m_logger);
                m_artifactsRemovedFromDisk << it.key();
            }
            product->buildData->rescuableArtifactData.clear();
        }
    }

    if (unbuiltProducts.isEmpty()) {
        m_logger.qbsInfo() << Tr::tr("Build done%1.").arg(configString());
    } else {
        m_error.append(Tr::tr("The following products could not be built%1:").arg(configString()));
        for (const ResolvedProductConstPtr &p : qAsConst(unbuiltProducts)) {
            QString errorMessage = Tr::tr("\t%1").arg(p->name);
            if (p->profile != m_project->profile())
                errorMessage += Tr::tr(" (for profile '%1')").arg(p->profile);
            m_error.append(errorMessage);
        }
    }
}

bool Executor::checkNodeProduct(BuildGraphNode *node)
{
    if (!m_partialBuild || m_productsToBuild.contains(node->product.lock()))
        return true;

    // TODO: Turn this into a warning once we have a reliable C++ scanner.
    m_logger.qbsTrace() << "Ignoring node " << node->toString() << ", which belongs to a "
                            "product that is not part of the list of products to build. "
                            "Possible reasons are an erroneous project design or a false "
                            "positive from a dependency scanner.";
    finishNode(node);
    return false;
}

void Executor::finish()
{
    QBS_ASSERT(m_state != ExecutorIdle, /* ignore */);
    QBS_ASSERT(!m_evalContext || !m_evalContext->engine()->isActive(), /* ignore */);

    checkForUnbuiltProducts();
    if (m_explicitlyCanceled) {
        QString message = Tr::tr(m_buildOptions.executeRulesOnly()
                                 ? "Rule execution canceled" : "Build canceled");
        m_error.append(Tr::tr("%1%2.").arg(message, configString()));
    }
    setState(ExecutorIdle);
    if (m_progressObserver) {
        m_progressObserver->setFinished();
        m_cancelationTimer->stop();
    }

    EmptyDirectoriesRemover(m_project.get(), m_logger)
            .removeEmptyParentDirectories(m_artifactsRemovedFromDisk);

    if (m_buildOptions.logElapsedTime()) {
        m_logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("Rule execution took %1.")
                                             .arg(elapsedTimeString(m_elapsedTimeRules));
        m_logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("Artifact scanning took %1.")
                                             .arg(elapsedTimeString(m_elapsedTimeScanners));
        m_logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("Installing artifacts took %1.")
                                             .arg(elapsedTimeString(m_elapsedTimeInstalling));
    }

    emit finished();
}

void Executor::checkForCancellation()
{
    QBS_ASSERT(m_progressObserver, return);
    if (m_state == ExecutorRunning && m_progressObserver->canceled()) {
        cancelJobs();
        if (m_evalContext->engine()->isActive())
            m_evalContext->engine()->cancel();
    }
}

bool Executor::visit(Artifact *artifact)
{
    QBS_CHECK(artifact->buildState != BuildGraphNode::Untouched);
    buildArtifact(artifact);
    return false;
}

bool Executor::visit(RuleNode *ruleNode)
{
    QBS_CHECK(ruleNode->buildState != BuildGraphNode::Untouched);
    executeRuleNode(ruleNode);
    return false;
}

/**
  * Sets the state of all artifacts in the graph to "untouched".
  * This must be done before doing a build.
  *
  * Retrieves the timestamps of source artifacts.
  *
  * This function also fills the list of changed source files.
  */
void Executor::prepareAllNodes()
{
    for (const ResolvedProductPtr &product : m_project->allProducts()) {
        if (product->enabled) {
            QBS_CHECK(product->buildData);
            for (BuildGraphNode * const node : qAsConst(product->buildData->nodes))
                node->buildState = BuildGraphNode::Untouched;
        }
    }
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild)) {
        QBS_CHECK(product->buildData);
        for (Artifact * const artifact : filterByType<Artifact>(product->buildData->nodes))
            prepareArtifact(artifact);
    }
}

void Executor::prepareArtifact(Artifact *artifact)
{
    artifact->inputsScanned = false;
    artifact->timestampRetrieved = false;

    if (artifact->artifactType == Artifact::SourceFile) {
        const FileTime oldTimestamp = artifact->timestamp();
        retrieveSourceFileTimestamp(artifact);
        if (oldTimestamp != artifact->timestamp())
            m_changedSourceArtifacts.append(artifact);
        possiblyInstallArtifact(artifact);
    }

    // Timestamps of file dependencies must be invalid for every build.
    for (FileDependency * const fileDependency : qAsConst(artifact->fileDependencies))
        fileDependency->clearTimestamp();
}

void Executor::setupForBuildingSelectedFiles(const BuildGraphNode *node)
{
    if (node->type() != BuildGraphNode::RuleNodeType)
        return;
    if (m_buildOptions.filesToConsider().isEmpty())
        return;
    if (!m_productsOfFilesToConsider.contains(node->product.lock()))
        return;
    const RuleNode * const ruleNode = static_cast<const RuleNode *>(node);
    const Rule * const rule = ruleNode->rule().get();
    if (rule->inputs.intersects(m_tagsOfFilesToConsider)) {
        FileTags otherInputs = rule->auxiliaryInputs;
        otherInputs.unite(rule->explicitlyDependsOn).subtract(rule->excludedAuxiliaryInputs);
        m_tagsNeededForFilesToConsider.unite(otherInputs);
    } else if (rule->collectedOutputFileTags().intersects(m_tagsNeededForFilesToConsider)) {
        FileTags allInputs = rule->inputs;
        allInputs.unite(rule->auxiliaryInputs).unite(rule->explicitlyDependsOn)
                .subtract(rule->excludedAuxiliaryInputs);
        m_tagsNeededForFilesToConsider.unite(allInputs);
    }
}

/**
 * Walk the build graph top-down from the roots and for each reachable node N
 *  - mark N as buildable.
 */
void Executor::prepareReachableNodes()
{
    for (BuildGraphNode * const root : qAsConst(m_roots))
        prepareReachableNodes_impl(root);
}

void Executor::prepareReachableNodes_impl(BuildGraphNode *node)
{
    setupForBuildingSelectedFiles(node);

    if (node->buildState != BuildGraphNode::Untouched)
        return;

    node->buildState = BuildGraphNode::Buildable;
    for (BuildGraphNode *child : qAsConst(node->children))
        prepareReachableNodes_impl(child);
}

void Executor::prepareProducts()
{
    ProductPrioritySetter prioritySetter(m_project.get());
    prioritySetter.apply();
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild))
        product->setupBuildEnvironment(m_evalContext->engine(), m_project->environment);
}

void Executor::setupRootNodes()
{
    m_roots.clear();
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild))
        m_roots += product->buildData->roots;
}

void Executor::setState(ExecutorState s)
{
    if (m_state == s)
        return;
    m_state = s;
}

} // namespace Internal
} // namespace qbs
