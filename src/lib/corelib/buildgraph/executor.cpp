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
#include "environmentscriptrunner.h"
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
#include "transformerchangetracking.h"

#include <buildgraph/transformer.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/preferences.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/settings.h>
#include <tools/stringconstants.h>

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
    return x->product->buildData->buildPriority() < y->product->buildData->buildPriority();
}


Executor::Executor(Logger logger, QObject *parent)
    : QObject(parent)
    , m_productInstaller(nullptr)
    , m_logger(std::move(logger))
    , m_progressObserver(nullptr)
    , m_state(ExecutorIdle)
    , m_cancelationTimer(new QTimer(this))
{
    m_inputArtifactScanContext = new InputArtifactScannerContext;
    m_cancelationTimer->setSingleShot(false);
    m_cancelationTimer->setInterval(1000);
    connect(m_cancelationTimer, &QTimer::timeout, this, &Executor::checkForCancellation);
}

Executor::~Executor()
{
    // jobs must be destroyed before deleting the m_inputArtifactScanContext
    m_allJobs.clear();
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

    if (m_buildOptions.changedFiles().empty())
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
        m_partialBuild = size_t(m_productsToBuild.size()) != m_allProducts.size();
        doBuild();
    } catch (const ErrorInfo &e) {
        handleError(e);
    }
}

void Executor::setProject(const TopLevelProjectPtr &project)
{
    m_project = project;
    m_allProducts = project->allProducts();
    m_projectsByName.clear();
    m_projectsByName.insert(std::make_pair(project->name, project.get()));
    for (const ResolvedProjectPtr &p : project->allSubProjects())
        m_projectsByName.insert(std::make_pair(p->name, p.get()));
}

void Executor::setProducts(const QVector<ResolvedProductPtr> &productsToBuild)
{
    m_productsToBuild = productsToBuild;
    m_productsByName.clear();
    for (const ResolvedProductPtr &p : productsToBuild)
        m_productsByName.insert(std::make_pair(p->uniqueName(), p.get()));
}

class ProductPrioritySetter
{
    const std::vector<ResolvedProductPtr> &m_allProducts;
    unsigned int m_priority = 0;
    Set<ResolvedProductPtr> m_seenProducts;
public:
    ProductPrioritySetter(const std::vector<ResolvedProductPtr> &allProducts) // TODO: Use only products to build?
        : m_allProducts(allProducts)
    {
    }

    void apply()
    {
        Set<ResolvedProductPtr> allDependencies;
        for (const ResolvedProductPtr &product : m_allProducts) {
            for (const ResolvedProductPtr &dep : product->dependencies)
                allDependencies += dep;
        }
        const Set<ResolvedProductPtr> rootProducts
                = Set<ResolvedProductPtr>::fromStdVector(m_allProducts) - allDependencies;
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
        product->buildData->setBuildPriority(m_priority--);
    }
};

void Executor::doBuild()
{
    if (m_buildOptions.maxJobCount() <= 0) {
        m_buildOptions.setMaxJobCount(BuildOptions::defaultMaxJobCount());
        qCDebug(lcExec) << "max job count not explicitly set, using value of"
                        << m_buildOptions.maxJobCount();
    }
    QBS_CHECK(m_state == ExecutorIdle);
    m_leaves = Leaves();
    m_error.clear();
    m_explicitlyCanceled = false;
    m_activeFileTags = FileTags::fromStringList(m_buildOptions.activeFileTags());
    m_tagsOfFilesToConsider.clear();
    m_tagsNeededForFilesToConsider.clear();
    m_productsOfFilesToConsider.clear();
    m_artifactsRemovedFromDisk.clear();
    m_jobCountPerPool.clear();

    setupJobLimits();

    // TODO: The "filesToConsider" thing is badly designed; we should know exactly which artifact
    //       it is. Remove this from the BuildOptions class and introduce Project::buildSomeFiles()
    //       instead.
    const QStringList &filesToConsider = m_buildOptions.filesToConsider();
    if (!filesToConsider.empty()) {
        for (const QString &fileToConsider : filesToConsider) {
            const auto &files = m_project->buildData->lookupFiles(fileToConsider);
            for (const FileResourceBase * const file : files) {
                if (file->fileType() != FileResourceBase::FileTypeArtifact)
                    continue;
                auto const artifact = static_cast<const Artifact *>(file);
                if (contains(m_productsToBuild, artifact->product.lock())) {
                    m_tagsOfFilesToConsider.unite(artifact->fileTags());
                    m_productsOfFilesToConsider << artifact->product.lock();
                }
            }
        }
    }

    setState(ExecutorRunning);

    if (m_productsToBuild.empty()) {
        qCDebug(lcExec) << "No products to build, finishing.";
        QTimer::singleShot(0, this, &Executor::finish); // Don't call back on the caller.
        return;
    }

    doSanityChecks();
    QBS_CHECK(!m_project->buildData->evaluationContext);
    m_project->buildData->evaluationContext = std::make_shared<RulesEvaluationContext>(m_logger);
    m_evalContext = m_project->buildData->evaluationContext;

    m_elapsedTimeRules = m_elapsedTimeScanners = m_elapsedTimeInstalling = 0;
    m_evalContext->engine()->enableProfiling(m_buildOptions.logElapsedTime());

    InstallOptions installOptions;
    installOptions.setDryRun(m_buildOptions.dryRun());
    installOptions.setInstallRoot(m_productsToBuild.front()->moduleProperties
            ->qbsPropertyValue(StringConstants::installRootProperty()).toString());
    installOptions.setKeepGoing(m_buildOptions.keepGoing());
    m_productInstaller = new ProductInstaller(m_project, m_productsToBuild, installOptions,
                                              m_progressObserver, m_logger);
    if (m_buildOptions.removeExistingInstallation())
        m_productInstaller->removeInstallRoot();

    addExecutorJobs();
    syncFileDependencies();
    prepareAllNodes();
    prepareProducts();
    setupRootNodes();
    prepareReachableNodes();
    setupProgressObserver();
    initLeaves();
    if (!scheduleJobs()) {
        qCDebug(lcExec) << "Nothing to do at all, finishing.";
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
        if (node->type() == BuildGraphNode::ArtifactNodeType) {
            auto const artifact = static_cast<Artifact *>(node);
            if (artifact->artifactType == Artifact::SourceFile)
                retrieveSourceFileTimestamp(artifact);
        }
    }

    bool isLeaf = true;
    for (BuildGraphNode *child : qAsConst(node->children)) {
        if (child->buildState != BuildGraphNode::Built) {
            isLeaf = false;
            updateLeaves(child, seenNodes);
        }
    }

    if (isLeaf) {
        qCDebug(lcExec).noquote() << "adding leaf" << node->toString();
        m_leaves.push(node);
    }
}

// Returns true if some artifacts are still waiting to be built or currently building.
bool Executor::scheduleJobs()
{
    QBS_CHECK(m_state == ExecutorRunning);
    std::vector<BuildGraphNode *> delayedLeaves;
    while (!m_leaves.empty() && !m_availableJobs.empty()) {
        BuildGraphNode * const nodeToBuild = m_leaves.top();
        m_leaves.pop();

        switch (nodeToBuild->buildState) {
        case BuildGraphNode::Untouched:
            QBS_ASSERT(!"untouched node in leaves list",
                       qDebug("%s", qPrintable(nodeToBuild->toString())));
            break;
        case BuildGraphNode::Buildable: // This is the only state in which we want to build a node.
            // TODO: It's a bit annoying that we have to check this here already, when we
            //       don't know whether the transformer needs to run at all. Investigate
            //       moving the whole job allocation logic to runTransformer().
            if (schedulingBlockedByJobLimit(nodeToBuild)) {
                qCDebug(lcExec).noquote() << "node delayed due to occupied job pool:"
                                          << nodeToBuild->toString();
                delayedLeaves.push_back(nodeToBuild);
            } else {
                nodeToBuild->accept(this);
            }
            break;
        case BuildGraphNode::Building:
            qCDebug(lcExec).noquote() << nodeToBuild->toString();
            qCDebug(lcExec) << "node is currently being built. Skipping.";
            break;
        case BuildGraphNode::Built:
            qCDebug(lcExec).noquote() << nodeToBuild->toString();
            qCDebug(lcExec) << "node already built. Skipping.";
            break;
        }
    }
    for (BuildGraphNode * const delayedLeaf : delayedLeaves)
        m_leaves.push(delayedLeaf);
    return !m_leaves.empty() || !m_processingJobs.empty();
}

bool Executor::schedulingBlockedByJobLimit(const BuildGraphNode *node)
{
    if (node->type() != BuildGraphNode::ArtifactNodeType)
        return false;
    const auto artifact = static_cast<const Artifact *>(node);
    if (artifact->artifactType == Artifact::SourceFile)
        return false;

    const Transformer * const transformer = artifact->transformer.get();
    for (const QString &jobPool : transformer->jobPools()) {
        const int currentJobCount = m_jobCountPerPool[jobPool];
        if (currentJobCount == 0)
            continue;
        const auto jobLimitIsExceeded = [currentJobCount, jobPool, this](const Transformer *t) {
            const int maxJobCount = m_jobLimitsPerProduct.at(t->product().get())
                    .getLimit(jobPool);
            return maxJobCount > 0 && currentJobCount >= maxJobCount;
        };

        // Different products can set different limits. The effective limit is the minimum of what
        // is set in this transformer's product and in the products of all currently
        // running transformers.
        if (jobLimitIsExceeded(transformer))
            return true;
        const auto runningJobs = m_processingJobs.keys();
        for (const ExecutorJob * const runningJob : runningJobs) {
            if (!runningJob->jobPools().contains(jobPool))
                continue;
            const Transformer * const runningTransformer = runningJob->transformer();
            if (!runningTransformer)
                continue; // This can happen if the ExecutorJob has already finished.
            if (runningTransformer->product() == transformer->product())
                continue; // We have already checked this product's job limit.
            if (jobLimitIsExceeded(runningTransformer))
                return true;
        }
    }
    return false;
}

bool Executor::isUpToDate(Artifact *artifact) const
{
    QBS_CHECK(artifact->artifactType == Artifact::Generated);

    qCDebug(lcUpToDateCheck) << "check" << artifact->filePath()
                             << artifact->timestamp().toString();

    if (m_buildOptions.forceTimestampCheck()) {
        artifact->setTimestamp(FileInfo(artifact->filePath()).lastModified());
        qCDebug(lcUpToDateCheck) << "timestamp retrieved from filesystem:"
                                 << artifact->timestamp().toString();
    }

    if (!artifact->timestamp().isValid()) {
        qCDebug(lcUpToDateCheck) << "invalid timestamp. Out of date.";
        return false;
    }

    for (Artifact *childArtifact : filterByType<Artifact>(artifact->children)) {
        QBS_CHECK(!childArtifact->alwaysUpdated || childArtifact->timestamp().isValid());
        qCDebug(lcUpToDateCheck) << "child timestamp"
                                 << childArtifact->timestamp().toString()
                                 << childArtifact->filePath();
        if (artifact->timestamp() < childArtifact->timestamp())
            return false;
    }

    for (FileDependency *fileDependency : qAsConst(artifact->fileDependencies)) {
        if (!fileDependency->timestamp().isValid()) {
            qCDebug(lcUpToDateCheck) << "file dependency doesn't exist"
                                     << fileDependency->filePath();
            return false;
        }
        qCDebug(lcUpToDateCheck) << "file dependency timestamp"
                                 << fileDependency->timestamp().toString()
                                 << fileDependency->filePath();
        if (artifact->timestamp() < fileDependency->timestamp())
            return false;
    }

    return true;
}

bool Executor::mustExecuteTransformer(const TransformerPtr &transformer) const
{
    if (transformer->alwaysRun)
        return true;
    if (transformer->markedForRerun) {
        qCDebug(lcUpToDateCheck) << "explicitly marked for re-run.";
        return true;
    }

    bool hasAlwaysUpdatedArtifacts = false;
    bool hasUpToDateNotAlwaysUpdatedArtifacts = false;
    for (Artifact *artifact : qAsConst(transformer->outputs)) {
        if (isUpToDate(artifact)) {
            if (artifact->alwaysUpdated)
                hasAlwaysUpdatedArtifacts = true;
            else
                hasUpToDateNotAlwaysUpdatedArtifacts = true;
        } else if (artifact->alwaysUpdated || m_buildOptions.forceTimestampCheck()) {
            return true;
        }
    }

    if (commandsNeedRerun(transformer.get(), transformer->product().get(), m_productsByName,
                          m_projectsByName)) {
        return true;
    }

    // If all artifacts in a transformer have "alwaysUpdated" set to false, that transformer is
    // run if and only if *all* of them are out of date.
    return !hasAlwaysUpdatedArtifacts && !hasUpToDateNotAlwaysUpdatedArtifacts;
}

void Executor::buildArtifact(Artifact *artifact)
{
    qCDebug(lcExec) << relativeArtifactFileName(artifact);

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

        qCDebug(lcExec) << "artifact type" << toString(artifact->artifactType) << "Skipping.";
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

    RuleNode::ApplicationResult result;
    ruleNode->apply(m_logger, m_productsByName, m_projectsByName, &result);
    updateLeaves(result.createdArtifacts);
    updateLeaves(result.invalidatedArtifacts);
    m_artifactsRemovedFromDisk << result.removedArtifacts;

    if (m_progressObserver) {
        const int transformerCount = ruleNode->transformerCount();
        if (transformerCount == 0) {
            m_progressObserver->incrementProgressValue();
        } else {
            m_pendingTransformersPerRule.insert(std::make_pair(ruleNode->rule().get(),
                                                               transformerCount));
        }
    }

    finishNode(ruleNode);
}

void Executor::finishJob(ExecutorJob *job, bool success)
{
    QBS_CHECK(job);
    QBS_CHECK(m_state != ExecutorIdle);

    const JobMap::Iterator it = m_processingJobs.find(job);
    QBS_CHECK(it != m_processingJobs.end());
    const TransformerPtr transformer = it.value();
    m_processingJobs.erase(it);
    m_availableJobs.push_back(job);
    updateJobCounts(transformer.get(), -1);
    if (success) {
        m_project->buildData->setDirty();
        for (Artifact * const artifact : qAsConst(transformer->outputs)) {
            if (artifact->alwaysUpdated) {
                artifact->setTimestamp(FileTime::currentTime());
                for (Artifact * const parent : artifact->parentArtifacts())
                    parent->transformer->markedForRerun = true;
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
        qCDebug(lcExec) << "Received cancel request; canceling build.";
        m_explicitlyCanceled = true;
        cancelJobs();
    }

    if (m_state == ExecutorCanceling) {
        if (m_processingJobs.empty()) {
            qCDebug(lcExec) << "All pending jobs are done, finishing.";
            finish();
        }
        return;
    }

    if (!scheduleJobs()) {
        qCDebug(lcExec) << "Nothing left to build; finishing.";
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
            qCDebug(lcExec).noquote() << "parent" << parent->toString()
                                      << "build state:" << toString(parent->buildState);
            continue;
        }

        if (allChildrenBuilt(parent)) {
            m_leaves.push(parent);
            qCDebug(lcExec).noquote() << "finishNode adds leaf"
                                      << parent->toString() << toString(parent->buildState);
        } else {
            qCDebug(lcExec).noquote() << "parent" << parent->toString()
                                      << "build state:" << toString(parent->buildState);
        }
    }
}

void Executor::finishArtifact(Artifact *leaf)
{
    QBS_CHECK(leaf);
    qCDebug(lcExec) << "finishArtifact" << relativeArtifactFileName(leaf);
    finishNode(leaf);
}

QString Executor::configString() const
{
    return tr(" for configuration %1").arg(m_project->id());
}

bool Executor::transformerHasMatchingOutputTags(const TransformerConstPtr &transformer) const
{
    if (m_activeFileTags.empty())
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
    if (m_buildOptions.filesToConsider().empty())
        return true; // No filtering requested.
    if (!m_productsOfFilesToConsider.contains(transformer->product()))
        return false;
    if (transformer->inputs.empty())
        return true;
    for (const Artifact * const input : qAsConst(transformer->inputs)) {
        const auto files = m_buildOptions.filesToConsider();
        for (const QString &filePath : files) {
            if (input->filePath() == filePath
                    || input->fileTags().intersects(m_tagsNeededForFilesToConsider)) {
                return true;
            }
        }
    }

    return false;
}

void Executor::setupJobLimits()
{
    Settings settings(m_buildOptions.settingsDirectory());
    for (const auto &p : qAsConst(m_productsToBuild)) {
        const Preferences prefs(&settings, p->profile());
        const JobLimits &jobLimitsFromSettings = prefs.jobLimits();
        JobLimits effectiveJobLimits;
        if (m_buildOptions.projectJobLimitsTakePrecedence()) {
            effectiveJobLimits.update(jobLimitsFromSettings).update(m_buildOptions.jobLimits())
                    .update(p->jobLimits);
        } else {
            effectiveJobLimits.update(p->jobLimits).update(jobLimitsFromSettings)
                    .update(m_buildOptions.jobLimits());
        }
        m_jobLimitsPerProduct.insert(std::make_pair(p.get(), effectiveJobLimits));
    }
}

void Executor::updateJobCounts(const Transformer *transformer, int diff)
{
    for (const QString &jobPool : transformer->jobPools())
        m_jobCountPerPool[jobPool] += diff;
}

void Executor::cancelJobs()
{
    if (m_state == ExecutorCanceling)
        return;
    qCDebug(lcExec) << "Canceling all jobs.";
    setState(ExecutorCanceling);
    const auto jobs = m_processingJobs.keys();
    for (ExecutorJob *job : jobs)
        job->cancel();
}

void Executor::setupProgressObserver()
{
    if (!m_progressObserver)
        return;
    int totalEffort = 1; // For the effort after the last rule application;
    for (const auto &product : qAsConst(m_productsToBuild)) {
        QBS_CHECK(product->buildData);
        const auto filtered = filterByType<RuleNode>(product->buildData->allNodes());
        totalEffort += std::distance(filtered.begin(), filtered.end());
    }
    m_progressObserver->initialize(tr("Building%1").arg(configString()), totalEffort);
}

void Executor::doSanityChecks()
{
    QBS_CHECK(m_project);
    QBS_CHECK(!m_productsToBuild.empty());
    for (const auto &product : qAsConst(m_productsToBuild)) {
        QBS_CHECK(product->buildData);
        QBS_CHECK(product->topLevelProject() == m_project.get());
    }
}

void Executor::handleError(const ErrorInfo &error)
{
    const auto items = error.items();
    for (const ErrorItem &ei : items)
        m_error.append(ei);
    if (m_processingJobs.empty())
        finish();
    else
        cancelJobs();
}

void Executor::addExecutorJobs()
{
    const int count = m_buildOptions.maxJobCount();
    qCDebug(lcExec) << "preparing executor for" << count << "jobs in parallel";
    m_allJobs.reserve(count);
    m_availableJobs.reserve(count);
    for (int i = 1; i <= count; i++) {
        m_allJobs.push_back(std::make_unique<ExecutorJob>(m_logger));
        const auto job = m_allJobs.back().get();
        job->setMainThreadScriptEngine(m_evalContext->engine());
        job->setObjectName(QStringLiteral("J%1").arg(i));
        job->setDryRun(m_buildOptions.dryRun());
        job->setEchoMode(m_buildOptions.echoMode());
        m_availableJobs.push_back(job);
        connect(job, &ExecutorJob::reportCommandDescription,
                this, &Executor::reportCommandDescription);
        connect(job, &ExecutorJob::reportProcessResult, this, &Executor::reportProcessResult);
        connect(job, &ExecutorJob::finished,
                this, &Executor::onJobFinished, Qt::QueuedConnection);
    }
}

void Executor::rescueOldBuildData(Artifact *artifact, bool *childrenAdded = nullptr)
{
    if (childrenAdded)
        *childrenAdded = false;
    if (!artifact->oldDataPossiblyPresent)
        return;
    artifact->oldDataPossiblyPresent = false;
    if (artifact->artifactType != Artifact::Generated)
        return;

    ResolvedProduct * const product = artifact->product.get();
    const RescuableArtifactData rad
            = product->buildData->removeFromRescuableArtifactData(artifact->filePath());
    if (!rad.isValid())
        return;
    qCDebug(lcBuildGraph) << "Attempting to rescue data of artifact" << artifact->fileName();

    std::vector<Artifact *> childrenToConnect;
    bool canRescue = artifact->transformer->commands == rad.commands;
    if (canRescue) {
        ResolvedProductPtr pseudoProduct = ResolvedProduct::create();
        for (const RescuableArtifactData::ChildData &cd : rad.children) {
            pseudoProduct->name = cd.productName;
            pseudoProduct->multiplexConfigurationId = cd.productMultiplexId;
            Artifact * const child = lookupArtifact(pseudoProduct, m_project->buildData.get(),
                                                    cd.childFilePath, true);
            if (artifact->children.contains(child))
                continue;
            if (!child)  {
                // If a child has disappeared, we must re-build even if the commands
                // are the same. Example: Header file included in cpp file does not exist anymore.
                canRescue = false;
                qCDebug(lcBuildGraph) << "Former child artifact" << cd.childFilePath
                                      << "does not exist anymore.";
                const RescuableArtifactData childRad
                        = product->buildData->removeFromRescuableArtifactData(cd.childFilePath);
                if (childRad.isValid()) {
                    m_artifactsRemovedFromDisk << artifact->filePath();
                    removeGeneratedArtifactFromDisk(cd.childFilePath, m_logger);
                }
            }
            if (!cd.addedByScanner) {
                // If an artifact has disappeared from the list of children, the commands
                // might need to run again.
                canRescue = false;
                qCDebug(lcBuildGraph) << "Former child artifact" << cd.childFilePath <<
                                         "is no longer in the list of children";
            }
            if (canRescue)
                childrenToConnect.push_back(child);
        }
        for (const QString &depPath : rad.fileDependencies) {
            const auto &depList = m_project->buildData->lookupFiles(depPath);
            if (depList.empty()) {
                canRescue = false;
                qCDebug(lcBuildGraph) << "File dependency" << depPath
                                      << "not in the project's list of dependencies anymore.";
                break;
            }
            const auto depFinder = [](const FileResourceBase *f) {
                return f->fileType() == FileResourceBase::FileTypeDependency;
            };
            const auto depIt = std::find_if(depList.cbegin(), depList.cend(), depFinder);
            if (depIt == depList.cend()) {
                canRescue = false;
                qCDebug(lcBuildGraph) << "File dependency" << depPath
                                      << "not in the project's list of dependencies anymore.";
                break;
            }
            artifact->fileDependencies.insert(static_cast<FileDependency *>(*depIt));
        }

        if (canRescue) {
            const TypeFilter<Artifact> childArtifacts(artifact->children);
            const size_t newChildCount = childrenToConnect.size()
                    + std::distance(childArtifacts.begin(), childArtifacts.end());
            QBS_CHECK(newChildCount >= rad.children.size());
            if (newChildCount > rad.children.size()) {
                canRescue = false;
                qCDebug(lcBuildGraph) << "Artifact has children not present in rescue data.";
            }
        }
    } else {
        qCDebug(lcBuildGraph) << "Transformer commands changed.";
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
        artifact->transformer->importedFilesUsedInPrepareScript
                = rad.importedFilesUsedInPrepareScript;
        artifact->transformer->importedFilesUsedInCommands = rad.importedFilesUsedInCommands;
        artifact->transformer->depsRequestedInPrepareScript = rad.depsRequestedInPrepareScript;
        artifact->transformer->depsRequestedInCommands = rad.depsRequestedInCommands;
        artifact->transformer->artifactsMapRequestedInPrepareScript
                = rad.artifactsMapRequestedInPrepareScript;
        artifact->transformer->artifactsMapRequestedInCommands
                = rad.artifactsMapRequestedInCommands;
        artifact->transformer->exportedModulesAccessedInPrepareScript
                = rad.exportedModulesAccessedInPrepareScript;
        artifact->transformer->exportedModulesAccessedInCommands
                = rad.exportedModulesAccessedInCommands;
        artifact->transformer->lastCommandExecutionTime = rad.lastCommandExecutionTime;
        artifact->transformer->lastPrepareScriptExecutionTime = rad.lastPrepareScriptExecutionTime;
        artifact->transformer->commandsNeedChangeTracking = true;
        artifact->setTimestamp(rad.timeStamp);
        artifact->transformer->markedForRerun
                = artifact->transformer->markedForRerun || rad.knownOutOfDate;
        if (childrenAdded && !childrenToConnect.empty())
            *childrenAdded = true;
        for (Artifact * const child : childrenToConnect) {
            if (safeConnect(artifact, child))
                artifact->childrenAddedByScanner << child;
        }
        qCDebug(lcBuildGraph) << "Data was rescued.";
    } else {
        removeGeneratedArtifactFromDisk(artifact, m_logger);
        m_artifactsRemovedFromDisk << artifact->filePath();
        qCDebug(lcBuildGraph) << "Data not rescued.";
    }
}

bool Executor::checkForUnbuiltDependencies(Artifact *artifact)
{
    bool buildingDependenciesFound = false;
    NodeSet unbuiltDependencies;
    for (BuildGraphNode * const dependency : qAsConst(artifact->children)) {
        switch (dependency->buildState) {
        case BuildGraphNode::Untouched:
        case BuildGraphNode::Buildable:
            qCDebug(lcExec).noquote() << "unbuilt dependency:" << dependency->toString();
            unbuiltDependencies += dependency;
            break;
        case BuildGraphNode::Building: {
            qCDebug(lcExec).noquote() << "dependency in state 'Building':" << dependency->toString();
            buildingDependenciesFound = true;
            break;
        }
        case BuildGraphNode::Built:
            // do nothing
            break;
        }
    }
    if (!unbuiltDependencies.empty()) {
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
        qCDebug(lcExec) << "file tags do not match. Skipping.";
        finishTransformer(transformer);
        return;
    }

    if (!transformerHasMatchingInputFiles(transformer)) {
        qCDebug(lcExec) << "input files do not match. Skipping.";
        finishTransformer(transformer);
        return;
    }

    const bool mustExecute = mustExecuteTransformer(transformer);
    if (mustExecute || m_buildOptions.forceTimestampCheck()) {
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
    }

    if (!mustExecute) {
        qCDebug(lcExec) << "Up to date. Skipping.";
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
            if (!outDir.exists() && !outDir.mkpath(StringConstants::dot())) {
                    throw ErrorInfo(tr("Failed to create directory '%1'.")
                                    .arg(QDir::toNativeSeparators(outDir.absolutePath())));
            }
        }
    }

    QBS_CHECK(!m_availableJobs.empty());
    ExecutorJob *job = m_availableJobs.takeFirst();
    for (Artifact * const artifact : qAsConst(transformer->outputs))
        artifact->buildState = BuildGraphNode::Building;
    m_processingJobs.insert(job, transformer);
    updateJobCounts(transformer.get(), 1);
    job->run(transformer.get());
}

void Executor::finishTransformer(const TransformerPtr &transformer)
{
    transformer->markedForRerun = false;
    for (Artifact * const artifact : qAsConst(transformer->outputs)) {
        possiblyInstallArtifact(artifact);
        finishArtifact(artifact);
    }
    if (m_progressObserver) {
        const auto it = m_pendingTransformersPerRule.find(transformer->rule.get());
        QBS_CHECK(it != m_pendingTransformersPerRule.cend());
        if (--it->second == 0) {
            m_progressObserver->incrementProgressValue();
            m_pendingTransformersPerRule.erase(it);
        }
    }
}

void Executor::possiblyInstallArtifact(const Artifact *artifact)
{
    AccumulatingTimer installTimer(m_buildOptions.logElapsedTime()
                                   ? &m_elapsedTimeInstalling : nullptr);

    if (m_buildOptions.install() && !m_buildOptions.executeRulesOnly()
            && (m_activeFileTags.empty() || artifactHasMatchingOutputTags(artifact))
            && artifact->properties->qbsPropertyValue(StringConstants::installProperty())
                    .toBool()) {
            m_productInstaller->copyFile(artifact);
    }
}

void Executor::onJobFinished(const qbs::ErrorInfo &err)
{
    try {
        auto const job = qobject_cast<ExecutorJob *>(sender());
        QBS_CHECK(job);
        if (m_evalContext->engine()->isActive()) {
            qCDebug(lcExec) << "Executor job finished while rule execution is pausing. "
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
    std::vector<ResolvedProductPtr> unbuiltProducts;
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild)) {
        bool productBuilt = true;
        for (BuildGraphNode *rootNode : qAsConst(product->buildData->rootNodes())) {
            if (rootNode->buildState != BuildGraphNode::Built) {
                productBuilt = false;
                unbuiltProducts.push_back(product);
                break;
            }
        }
        if (productBuilt) {
            // Any element still left after a successful build has not been re-created
            // by any rule and therefore does not exist anymore as an artifact.
            const AllRescuableArtifactData rad = product->buildData->rescuableArtifactData();
            for (auto it = rad.cbegin(); it != rad.cend(); ++it) {
                removeGeneratedArtifactFromDisk(it.key(), m_logger);
                product->buildData->removeFromRescuableArtifactData(it.key());
                m_artifactsRemovedFromDisk << it.key();
            }
        }
    }

    if (unbuiltProducts.empty()) {
        m_logger.qbsInfo() << Tr::tr("Build done%1.").arg(configString());
    } else {
        m_error.append(Tr::tr("The following products could not be built%1:").arg(configString()));
        QStringList productNames;
        std::transform(unbuiltProducts.cbegin(), unbuiltProducts.cend(),
                       std::back_inserter(productNames),
                       [](const ResolvedProductConstPtr &p) { return p->fullDisplayName(); });
        std::sort(productNames.begin(), productNames.end());
        m_error.append(productNames.join(QLatin1String(", ")));
    }
}

bool Executor::checkNodeProduct(BuildGraphNode *node)
{
    if (!m_partialBuild || contains(m_productsToBuild, node->product.lock()))
        return true;

    // TODO: Turn this into a warning once we have a reliable C++ scanner.
    qCDebug(lcExec).noquote()
            << "Ignoring node " << node->toString() << ", which belongs to a "
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
    for (const ResolvedProductPtr &product : m_allProducts) {
        if (product->enabled) {
            QBS_CHECK(product->buildData);
            for (BuildGraphNode * const node : qAsConst(product->buildData->allNodes()))
                node->buildState = BuildGraphNode::Untouched;
        }
    }
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild)) {
        QBS_CHECK(product->buildData);
        for (Artifact * const artifact : filterByType<Artifact>(product->buildData->allNodes()))
            prepareArtifact(artifact);
    }
}

void Executor::syncFileDependencies()
{
    Set<FileDependency *> &globalFileDepList = m_project->buildData->fileDependencies;
    for (auto it = globalFileDepList.begin(); it != globalFileDepList.end(); ) {
        FileDependency * const dep = *it;
        FileInfo fi(dep->filePath());
        if (fi.exists()) {
            dep->setTimestamp(fi.lastModified());
            ++it;
            continue;
        }
        qCDebug(lcBuildGraph()) << "file dependency" << dep->filePath() << "no longer exists; "
                                   "removing from lookup table";
        m_project->buildData->removeFromLookupTable(dep);
        bool isReferencedByArtifact = false;
        for (const auto &product : m_allProducts) {
            if (!product->buildData)
                continue;
            const auto artifactList = filterByType<Artifact>(product->buildData->allNodes());
            isReferencedByArtifact = std::any_of(artifactList.begin(), artifactList.end(),
                    [dep](const Artifact *a) { return a->fileDependencies.contains(dep); });
            // TODO: Would it be safe to mark the artifact as "not up to date" here and clear
            //       its list of file dependencies, rather than doing the check again in
            //       isUpToDate()?
            if (isReferencedByArtifact)
                break;
        }
        if (!isReferencedByArtifact) {
            qCDebug(lcBuildGraph()) << "dependency is not referenced by any artifact, deleting";
            it = globalFileDepList.erase(it);
            delete dep;
        } else {
            dep->clearTimestamp();
            ++it;
        }
    }
}

void Executor::prepareArtifact(Artifact *artifact)
{
    artifact->inputsScanned = false;
    artifact->timestampRetrieved = false;

    if (artifact->artifactType == Artifact::SourceFile) {
        retrieveSourceFileTimestamp(artifact);
        possiblyInstallArtifact(artifact);
    }
}

void Executor::setupForBuildingSelectedFiles(const BuildGraphNode *node)
{
    if (node->type() != BuildGraphNode::RuleNodeType)
        return;
    if (m_buildOptions.filesToConsider().empty())
        return;
    if (!m_productsOfFilesToConsider.contains(node->product.lock()))
        return;
    const auto ruleNode = static_cast<const RuleNode *>(node);
    const Rule * const rule = ruleNode->rule().get();
    if (rule->inputs.intersects(m_tagsOfFilesToConsider)) {
        FileTags otherInputs = rule->auxiliaryInputs;
        otherInputs.unite(rule->explicitlyDependsOn).subtract(rule->excludedInputs);
        m_tagsNeededForFilesToConsider.unite(otherInputs);
    } else if (rule->collectedOutputFileTags().intersects(m_tagsNeededForFilesToConsider)) {
        FileTags allInputs = rule->inputs;
        allInputs.unite(rule->auxiliaryInputs).unite(rule->explicitlyDependsOn)
                .subtract(rule->excludedInputs);
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
    ProductPrioritySetter prioritySetter(m_allProducts);
    prioritySetter.apply();
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild)) {
        EnvironmentScriptRunner(product.get(), m_evalContext.get(), m_project->environment)
                .setupForBuild();
    }
}

void Executor::setupRootNodes()
{
    m_roots.clear();
    for (const ResolvedProductPtr &product : qAsConst(m_productsToBuild))
        m_roots += product->buildData->rootNodes();
}

void Executor::setState(ExecutorState s)
{
    if (m_state == s)
        return;
    m_state = s;
}

} // namespace Internal
} // namespace qbs
