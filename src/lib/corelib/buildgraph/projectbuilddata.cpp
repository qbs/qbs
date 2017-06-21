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
#include "projectbuilddata.h"

#include "artifact.h"
#include "buildgraph.h"
#include "buildgraphvisitor.h"
#include "productbuilddata.h"
#include "rulecommands.h"
#include "rulegraph.h"
#include "rulenode.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

static Set<ResolvedProductPtr> findDependentProducts(const ResolvedProductPtr &product)
{
    Set<ResolvedProductPtr> result;
    for (const ResolvedProductPtr &parent : product->topLevelProject()->allProducts()) {
        if (parent->dependencies.contains(product))
            result += parent;
    }
    return result;
}

ProjectBuildData::ProjectBuildData(const ProjectBuildData *other)
    : isDirty(true), m_doCleanupInDestructor(true)
{
    // This is needed for temporary duplication of build data when doing change tracking.
    if (other) {
        *this = *other;
        m_doCleanupInDestructor = false;
    }
}

ProjectBuildData::~ProjectBuildData()
{
    if (!m_doCleanupInDestructor)
        return;
    qDeleteAll(fileDependencies);
}

QString ProjectBuildData::deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId)
{
    return buildDir + QLatin1Char('/') + projectId + QLatin1String(".bg");
}

static QString productNameForErrorMessage(const ResolvedProduct *product)
{
    return product->profile == product->topLevelProject()->profile()
            && product->multiplexConfigurationId.isEmpty()
            ? product->name : product->uniqueName();
}

void ProjectBuildData::insertIntoLookupTable(FileResourceBase *fileres)
{
    QList<FileResourceBase *> &lst
            = m_artifactLookupTable[fileres->fileName()][fileres->dirPath()];
    const auto * const artifact = dynamic_cast<Artifact *>(fileres);
    if (artifact && artifact->artifactType == Artifact::Generated) {
        for (const auto *file : lst) {
            const auto * const otherArtifact = dynamic_cast<const Artifact *>(file);
            if (otherArtifact) {
                ErrorInfo error;
                error.append(Tr::tr("Conflicting artifacts for file path '%1'.")
                             .arg(artifact->filePath()));
                error.append(Tr::tr("The first artifact comes from product '%1'.")
                             .arg(productNameForErrorMessage(otherArtifact->product.get())),
                             otherArtifact->product->location);
                error.append(Tr::tr("The second artifact comes from product '%1'.")
                             .arg(productNameForErrorMessage(artifact->product.get())),
                             artifact->product->location);
                throw error;
            }
        }
    }
    QBS_CHECK(!lst.contains(fileres));
    lst.append(fileres);
}

void ProjectBuildData::removeFromLookupTable(FileResourceBase *fileres)
{
    m_artifactLookupTable[fileres->fileName()][fileres->dirPath()].removeOne(fileres);
}

QList<FileResourceBase *> ProjectBuildData::lookupFiles(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupFiles(dirPath, fileName);
}

QList<FileResourceBase *> ProjectBuildData::lookupFiles(const QString &dirPath,
        const QString &fileName) const
{
    return m_artifactLookupTable.value(fileName).value(dirPath);
}

QList<FileResourceBase *> ProjectBuildData::lookupFiles(const Artifact *artifact) const
{
    return lookupFiles(artifact->dirPath(), artifact->fileName());
}

void ProjectBuildData::insertFileDependency(FileDependency *dependency)
{
    fileDependencies += dependency;
    insertIntoLookupTable(dependency);
}

static void disconnectArtifactChildren(Artifact *artifact, const Logger &logger)
{
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLatin1("[BG] disconnectChildren: '%1'")
                             .arg(relativeArtifactFileName(artifact));
    }
    for (BuildGraphNode * const child : qAsConst(artifact->children))
        child->parents.remove(artifact);
    artifact->children.clear();
    artifact->childrenAddedByScanner.clear();
}

static void disconnectArtifactParents(Artifact *artifact, const Logger &logger)
{
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLatin1("[BG] disconnectParents: '%1'")
                             .arg(relativeArtifactFileName(artifact));
    }
    for (BuildGraphNode * const parent : qAsConst(artifact->parents)) {
        parent->children.remove(artifact);
        Artifact *parentArtifact = dynamic_cast<Artifact *>(parent);
        if (parentArtifact) {
            QBS_CHECK(parentArtifact->transformer);
            parentArtifact->childrenAddedByScanner.remove(artifact);
            parentArtifact->transformer->inputs.remove(artifact);
            parentArtifact->product->registerArtifactWithChangedInputs(parentArtifact);
        }
    }

    artifact->parents.clear();
}

static void disconnectArtifact(Artifact *artifact, const Logger &logger)
{
    disconnectArtifactChildren(artifact, logger);
    disconnectArtifactParents(artifact, logger);
}

/*!
  * Removes the artifact and all the artifacts that depend exclusively on it.
  * Example: if you remove a cpp artifact then the obj artifact is removed but
  * not the resulting application (if there's more then one cpp artifact).
  */
void ProjectBuildData::removeArtifactAndExclusiveDependents(Artifact *artifact,
        const Logger &logger, bool removeFromProduct,
        ArtifactSet *removedArtifacts)
{
    if (removedArtifacts)
        removedArtifacts->insert(artifact);

    // Iterate over a copy of the artifact's parents, because we'll change
    // artifact->parents with the disconnect call.
    const NodeSet parentsCopy = artifact->parents;
    for (Artifact *parent : filterByType<Artifact>(parentsCopy)) {
        bool removeParent = false;
        disconnect(parent, artifact, logger);
        if (parent->children.isEmpty()) {
            removeParent = true;
        } else if (parent->transformer) {
            parent->product->registerArtifactWithChangedInputs(parent);
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.isEmpty();
        }
        if (removeParent) {
            removeArtifactAndExclusiveDependents(parent, logger, removeFromProduct,
                                                 removedArtifacts);
        } else {
            parent->clearTimestamp();
        }
    }
    const bool removeFromDisk = artifact->artifactType == Artifact::Generated;
    removeArtifact(artifact, logger, removeFromDisk, removeFromProduct);
}

static void removeFromRuleNodes(Artifact *artifact, const Logger &logger)
{
    for (const ResolvedProductPtr &product : artifact->product->topLevelProject()->allProducts()) {
        if (!product->buildData)
            continue;
        for (BuildGraphNode *n : qAsConst(product->buildData->nodes)) {
            if (n->type() != BuildGraphNode::RuleNodeType)
                continue;
            RuleNode * const ruleNode = static_cast<RuleNode *>(n);
            if (logger.traceEnabled()) {
                logger.qbsTrace() << "[BG] remove old input " << artifact->filePath()
                                  << " from rule " << ruleNode->rule()->toString();
            }
            ruleNode->removeOldInputArtifact(artifact);
        }
    }
}

void ProjectBuildData::removeArtifact(Artifact *artifact,
        const Logger &logger, bool removeFromDisk, bool removeFromProduct)
{
    if (logger.traceEnabled())
        logger.qbsTrace() << "[BG] remove artifact " << relativeArtifactFileName(artifact);

    if (removeFromDisk)
        removeGeneratedArtifactFromDisk(artifact, logger);
    removeFromLookupTable(artifact);
    removeFromRuleNodes(artifact, logger);
    disconnectArtifact(artifact, logger);
    if (artifact->transformer) {
        artifact->product->unregisterArtifactWithChangedInputs(artifact);
        artifact->transformer->outputs.remove(artifact);
    }
    if (removeFromProduct) {
        artifact->product->buildData->nodes.remove(artifact);
        artifact->product->buildData->roots.remove(artifact);
        removeArtifactFromSet(artifact, artifact->product->buildData->artifactsByFileTag);
    }
    isDirty = true;
}

void ProjectBuildData::load(PersistentPool &pool)
{
    pool.load(fileDependencies);
    for (FileDependency * const dep : qAsConst(fileDependencies))
        insertFileDependency(dep);
    pool.load(rawScanResults);
}

void ProjectBuildData::store(PersistentPool &pool) const
{
    pool.store(fileDependencies);
    pool.store(rawScanResults);
}


BuildDataResolver::BuildDataResolver(const Logger &logger) : m_logger(logger)
{
}

void BuildDataResolver::resolveBuildData(const TopLevelProjectPtr &resolvedProject,
                                          const RulesEvaluationContextPtr &evalContext)
{
    QBS_CHECK(!resolvedProject->buildData);
    m_project = resolvedProject;
    resolvedProject->buildData.reset(new ProjectBuildData);
    resolvedProject->buildData->evaluationContext = evalContext;
    const QList<ResolvedProductPtr> allProducts = resolvedProject->allProducts();
    evalContext->initializeObserver(Tr::tr("Setting up build graph for configuration %1")
                                    .arg(resolvedProject->id()), allProducts.count() + 1);
    for (ResolvedProductPtr rProduct : allProducts) {
        if (rProduct->enabled)
            resolveProductBuildData(rProduct);
        evalContext->incrementProgressValue();
    }
    evalContext->incrementProgressValue();
    doSanityChecks(resolvedProject, m_logger);
}

void BuildDataResolver::resolveProductBuildDataForExistingProject(const TopLevelProjectPtr &project,
        const QList<ResolvedProductPtr> &freshProducts)
{
    m_project = project;
    for (const ResolvedProductPtr &product : freshProducts) {
        if (product->enabled)
            resolveProductBuildData(product);
    }

    QHash<ResolvedProductPtr, Set<ResolvedProductPtr>> dependencyMap;
    foreach (const ResolvedProductPtr &product, freshProducts) {
        if (!product->enabled)
            continue;
        QBS_CHECK(product->buildData);
        Set<ResolvedProductPtr> dependents = findDependentProducts(product);
        foreach (const ResolvedProductPtr &dependentProduct, dependents) {
            if (!dependentProduct->enabled)
                continue;
            dependencyMap[dependentProduct] << product;
        }
    }
    for (auto it = dependencyMap.cbegin(); it != dependencyMap.cend(); ++it) {
        if (!freshProducts.contains(it.key()))
           connectRulesToDependencies(it.key(), it.value());
    }
}

class CreateRuleNodes : public RuleGraphVisitor
{
public:
    CreateRuleNodes(const ResolvedProductPtr &product, const Logger &logger)
        : m_product(product), m_logger(logger)
    {
    }

    const Set<RuleNode *> &leaves() const
    {
        return m_leaves;
    }

private:
    const ResolvedProductPtr &m_product;
    const Logger &m_logger;
    QHash<RuleConstPtr, RuleNode *> m_nodePerRule;
    Set<const Rule *> m_rulesOnPath;
    QList<const Rule *> m_rulePath;
    Set<RuleNode *> m_leaves;

    void visit(const RuleConstPtr &parentRule, const RuleConstPtr &rule)
    {
        if (!m_rulesOnPath.insert(rule.get()).second) {
            QString pathstr;
            for (const Rule *r : qAsConst(m_rulePath)) {
                pathstr += QLatin1Char('\n') + r->toString() + QLatin1Char('\t')
                        + r->prepareScript->location.toString();
            }
            throw ErrorInfo(Tr::tr("Cycle detected in rule dependencies: %1").arg(pathstr));
        }
        m_rulePath.append(rule.get());
        RuleNode *node = m_nodePerRule.value(rule);
        if (!node) {
            node = new RuleNode;
            m_leaves.insert(node);
            m_nodePerRule.insert(rule, node);
            node->product = m_product;
            node->setRule(rule);
            m_product->buildData->nodes += node;
            if (m_logger.debugEnabled()) {
                m_logger.qbsDebug() << "[BG] create " << node->toString()
                                    << " for product " << m_product->uniqueName();
            }
        }
        if (parentRule) {
            RuleNode *parent = m_nodePerRule.value(parentRule);
            QBS_CHECK(parent);
            loggedConnect(parent, node, m_logger);
            m_leaves.remove(parent);
        } else {
            m_product->buildData->roots += node;
        }
    }

    void endVisit(const RuleConstPtr &rule)
    {
        m_rulesOnPath.remove(rule.get());
        m_rulePath.removeLast();
    }
};

static bool areRulesCompatible(const RuleNode *ruleNode, const RuleNode *dependencyRule)
{
    const FileTags outTags = dependencyRule->rule()->collectedOutputFileTags();
    if (ruleNode->rule()->inputsFromDependencies.intersects(outTags))
        return true;
    if (!dependencyRule->product->fileTags.intersects(outTags))
        return false;
    if (ruleNode->rule()->explicitlyDependsOn.intersects(outTags))
        return true;
    return ruleNode->rule()->auxiliaryInputs.intersects(outTags)
            && !ruleNode->rule()->excludedAuxiliaryInputs.intersects(outTags);
}

void BuildDataResolver::resolveProductBuildData(const ResolvedProductPtr &product)
{
    if (product->buildData)
        return;

    evalContext()->checkForCancelation();

    product->buildData.reset(new ProductBuildData);
    ProductBuildData::ArtifactSetByFileTag artifactsPerFileTag;

    for (ResolvedProductPtr dependency : qAsConst(product->dependencies)) {
        QBS_CHECK(dependency->enabled);
        resolveProductBuildData(dependency);
    }

    //add qbsFile artifact
    Artifact *qbsFileArtifact = lookupArtifact(product, product->location.filePath());
    if (!qbsFileArtifact) {
        qbsFileArtifact = new Artifact;
        qbsFileArtifact->artifactType = Artifact::SourceFile;
        qbsFileArtifact->setFilePath(product->location.filePath());
        qbsFileArtifact->properties = product->moduleProperties;
        insertArtifact(product, qbsFileArtifact, m_logger);
    }
    qbsFileArtifact->addFileTag("qbs");
    artifactsPerFileTag["qbs"].insert(qbsFileArtifact);

    // read sources
    for (const SourceArtifactConstPtr &sourceArtifact : product->allEnabledFiles()) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (lookupArtifact(product, filePath))
            continue; // ignore duplicate artifacts

        Artifact *artifact = createArtifact(product, sourceArtifact, m_logger);
        for (const FileTag &fileTag : artifact->fileTags())
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    RuleGraph ruleGraph;
    ruleGraph.build(product->rules, product->fileTags);
    CreateRuleNodes crn(product, m_logger);
    ruleGraph.accept(&crn);

    connectRulesToDependencies(product, product->dependencies);
}

static bool isRootRuleNode(RuleNode *ruleNode)
{
    return ruleNode->product->buildData->roots.contains(ruleNode);
}

void BuildDataResolver::connectRulesToDependencies(const ResolvedProductPtr &product,
                                                   const Set<ResolvedProductPtr> &dependencies)
{
    // Connect the rules of this product to the compatible rules of all product dependencies.
    // Rules that take "installable" artifacts are connected to all root rules of product
    // dependencies.
    std::vector<RuleNode *> ruleNodes;
    for (RuleNode *ruleNode : filterByType<RuleNode>(product->buildData->nodes))
        ruleNodes.push_back(ruleNode);
    foreach (const ResolvedProductConstPtr &dep, dependencies) {
        if (!dep->buildData)
            continue;
        for (RuleNode *depRuleNode : filterByType<RuleNode>(dep->buildData->nodes)) {
            for (RuleNode *ruleNode : ruleNodes) {
                static const FileTag installableTag("installable");
                if (areRulesCompatible(ruleNode, depRuleNode)
                        || (ruleNode->rule()->inputsFromDependencies.contains(installableTag)
                            && isRootRuleNode(depRuleNode))) {
                    loggedConnect(ruleNode, depRuleNode, m_logger);
                }
            }
        }
    }
}

RulesEvaluationContextPtr BuildDataResolver::evalContext() const
{
    return m_project->buildData->evaluationContext;
}

ScriptEngine *BuildDataResolver::engine() const
{
    return evalContext()->engine();
}

QScriptValue BuildDataResolver::scope() const
{
    return evalContext()->scope();
}

} // namespace Internal
} // namespace qbs
