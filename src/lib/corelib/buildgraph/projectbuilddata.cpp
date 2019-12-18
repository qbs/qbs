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

#include <language/artifactproperties.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stlutils.h>

#include <memory>

namespace qbs {
namespace Internal {

static Set<ResolvedProductPtr> findDependentProducts(const ResolvedProductPtr &product)
{
    Set<ResolvedProductPtr> result;
    for (const ResolvedProductPtr &parent : product->topLevelProject()->allProducts()) {
        if (contains(parent->dependencies, product))
            result += parent;
    }
    return result;
}

ProjectBuildData::ProjectBuildData(const ProjectBuildData *other)
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
    return buildDir + QLatin1Char('/') + projectId + QStringLiteral(".bg");
}

void ProjectBuildData::insertIntoLookupTable(FileResourceBase *fileres)
{
    auto &lst = m_artifactLookupTable[{fileres->fileName(), fileres->dirPath()}];
    const auto * const artifact = fileres->fileType() == FileResourceBase::FileTypeArtifact
            ? static_cast<Artifact *>(fileres) : nullptr;
    if (artifact && artifact->artifactType == Artifact::Generated) {
        for (const auto *file : lst) {
            if (file->fileType() != FileResourceBase::FileTypeArtifact)
                continue;
            const auto * const otherArtifact = static_cast<const Artifact *>(file);
            ErrorInfo error;
            error.append(Tr::tr("Conflicting artifacts for file path '%1'.")
                         .arg(artifact->filePath()));
            error.append(Tr::tr("The first artifact comes from product '%1'.")
                         .arg(otherArtifact->product->fullDisplayName()),
                         otherArtifact->product->location);
            error.append(Tr::tr("The second artifact comes from product '%1'.")
                         .arg(artifact->product->fullDisplayName()),
                         artifact->product->location);
            throw error;
        }
    }
    QBS_CHECK(!contains(lst, fileres));
    lst.push_back(fileres);
    m_isDirty = true;
}

void ProjectBuildData::removeFromLookupTable(FileResourceBase *fileres)
{
    removeOne(m_artifactLookupTable[{fileres->fileName(), fileres->dirPath()}], fileres);
}

const std::vector<FileResourceBase *> &ProjectBuildData::lookupFiles(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupFiles(dirPath, fileName);
}

const std::vector<FileResourceBase *> &ProjectBuildData::lookupFiles(const QString &dirPath,
        const QString &fileName) const
{
    static const std::vector<FileResourceBase *> emptyResult;
    const auto it = m_artifactLookupTable.find({fileName, dirPath});
    return it != m_artifactLookupTable.end() ? it->second : emptyResult;
}

const std::vector<FileResourceBase *> &ProjectBuildData::lookupFiles(const Artifact *artifact) const
{
    return lookupFiles(artifact->dirPath(), artifact->fileName());
}

void ProjectBuildData::insertFileDependency(FileDependency *dependency)
{
    fileDependencies += dependency;
    insertIntoLookupTable(dependency);
}

static void disconnectArtifactChildren(Artifact *artifact)
{
    qCDebug(lcBuildGraph) << "disconnect children of" << relativeArtifactFileName(artifact);
    for (BuildGraphNode * const child : qAsConst(artifact->children))
        child->parents.remove(artifact);
    artifact->children.clear();
    artifact->childrenAddedByScanner.clear();
}

static void disconnectArtifactParents(Artifact *artifact)
{
    qCDebug(lcBuildGraph) << "disconnect parents of" << relativeArtifactFileName(artifact);
    for (BuildGraphNode * const parent : qAsConst(artifact->parents)) {
        parent->children.remove(artifact);
        if (parent->type() != BuildGraphNode::ArtifactNodeType)
            continue;
        auto const parentArtifact = static_cast<Artifact *>(parent);
        QBS_CHECK(parentArtifact->transformer);
        parentArtifact->childrenAddedByScanner.remove(artifact);
        parentArtifact->transformer->inputs.remove(artifact);
        parentArtifact->transformer->explicitlyDependsOn.remove(artifact);
    }

    artifact->parents.clear();
}

static void disconnectArtifact(Artifact *artifact)
{
    disconnectArtifactChildren(artifact);
    disconnectArtifactParents(artifact);
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
        disconnect(parent, artifact);
        if (parent->children.empty()) {
            removeParent = true;
        } else if (parent->transformer) {
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.empty();
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

static void removeFromRuleNodes(Artifact *artifact)
{
    for (RuleNode * const ruleNode : filterByType<RuleNode>(artifact->parents))
        ruleNode->removeOldInputArtifact(artifact);
}

void ProjectBuildData::removeArtifact(Artifact *artifact,
        const Logger &logger, bool removeFromDisk, bool removeFromProduct)
{
    qCDebug(lcBuildGraph) << "remove artifact" << relativeArtifactFileName(artifact);
    if (removeFromDisk)
        removeGeneratedArtifactFromDisk(artifact, logger);
    removeFromLookupTable(artifact);
    removeFromRuleNodes(artifact);
    disconnectArtifact(artifact);
    if (artifact->transformer)
        artifact->transformer->outputs.remove(artifact);
    if (removeFromProduct)
        artifact->product->buildData->removeArtifact(artifact);
    m_isDirty = false;
}

void ProjectBuildData::setDirty()
{
    qCDebug(lcBuildGraph) << "Marking build graph as dirty";
    m_isDirty = true;
}

void ProjectBuildData::setClean()
{
    qCDebug(lcBuildGraph) << "Marking build graph as clean";
    m_isDirty = false;
}

void ProjectBuildData::load(PersistentPool &pool)
{
    serializationOp<PersistentPool::Load>(pool);
    for (FileDependency * const dep : qAsConst(fileDependencies))
        insertIntoLookupTable(dep);
    m_isDirty = false;
}

void ProjectBuildData::store(PersistentPool &pool)
{
    serializationOp<PersistentPool::Store>(pool);
}


BuildDataResolver::BuildDataResolver(Logger logger) : m_logger(std::move(logger))
{
}

void BuildDataResolver::resolveBuildData(const TopLevelProjectPtr &resolvedProject,
                                          const RulesEvaluationContextPtr &evalContext)
{
    QBS_CHECK(!resolvedProject->buildData);
    m_project = resolvedProject;
    resolvedProject->buildData = std::make_unique<ProjectBuildData>();
    resolvedProject->buildData->evaluationContext = evalContext;
    const std::vector<ResolvedProductPtr> &allProducts = resolvedProject->allProducts();
    evalContext->initializeObserver(Tr::tr("Setting up build graph for configuration %1")
                                    .arg(resolvedProject->id()), int(allProducts.size()) + 1);
    for (const auto &rProduct : allProducts) {
        if (rProduct->enabled)
            resolveProductBuildData(rProduct);
        evalContext->incrementProgressValue();
    }
    evalContext->incrementProgressValue();
    doSanityChecks(resolvedProject, m_logger);
}

void BuildDataResolver::resolveProductBuildDataForExistingProject(const TopLevelProjectPtr &project,
        const std::vector<ResolvedProductPtr> &freshProducts)
{
    m_project = project;
    for (const ResolvedProductPtr &product : freshProducts) {
        if (product->enabled)
            resolveProductBuildData(product);
    }

    QHash<ResolvedProductPtr, std::vector<ResolvedProductPtr>> dependencyMap;
    for (const ResolvedProductPtr &product : freshProducts) {
        if (!product->enabled)
            continue;
        QBS_CHECK(product->buildData);
        const Set<ResolvedProductPtr> dependents = findDependentProducts(product);
        for (const ResolvedProductPtr &dependentProduct : dependents) {
            if (!dependentProduct->enabled)
                continue;
            if (!contains(dependencyMap[dependentProduct], product))
                dependencyMap[dependentProduct].push_back(product);
        }
    }
    for (auto it = dependencyMap.cbegin(); it != dependencyMap.cend(); ++it) {
        if (!contains(freshProducts, it.key()))
           connectRulesToDependencies(it.key(), it.value());
    }
}

class CreateRuleNodes : public RuleGraphVisitor
{
public:
    CreateRuleNodes(const ResolvedProductPtr &product)
        : m_product(product)
    {
    }

private:
    const ResolvedProductPtr &m_product;
    QHash<RuleConstPtr, RuleNode *> m_nodePerRule;
    Set<const Rule *> m_rulesOnPath;
    QList<const Rule *> m_rulePath;

    void visit(const RuleConstPtr &parentRule, const RuleConstPtr &rule) override
    {
        if (!m_rulesOnPath.insert(rule.get()).second) {
            QString pathstr;
            for (const Rule *r : qAsConst(m_rulePath)) {
                pathstr += QLatin1Char('\n') + r->toString() + QLatin1Char('\t')
                        + r->prepareScript.location().toString();
            }
            throw ErrorInfo(Tr::tr("Cycle detected in rule dependencies: %1").arg(pathstr));
        }
        m_rulePath.push_back(rule.get());
        RuleNode *node = m_nodePerRule.value(rule);
        if (!node) {
            node = new RuleNode;
            m_nodePerRule.insert(rule, node);
            node->product = m_product;
            node->setRule(rule);
            m_product->buildData->addNode(node);
            qCDebug(lcBuildGraph).noquote() << "create" << node->toString()
                                            << "for product" << m_product->uniqueName();
        }
        if (parentRule) {
            RuleNode *parent = m_nodePerRule.value(parentRule);
            QBS_CHECK(parent);
            connect(parent, node);
        } else {
            m_product->buildData->addRootNode(node);
        }
    }

    void endVisit(const RuleConstPtr &rule) override
    {
        m_rulesOnPath.remove(rule.get());
        m_rulePath.removeLast();
    }
};

static bool areRulesCompatible(const RuleNode *ruleNode, const RuleNode *dependencyRule)
{
    const FileTags outTags = dependencyRule->rule()->collectedOutputFileTags();
    if (ruleNode->rule()->excludedInputs.intersects(outTags))
        return false;
    if (ruleNode->rule()->inputsFromDependencies.intersects(outTags))
        return true;
    if (!dependencyRule->product->fileTags.intersects(outTags))
        return false;
    if (ruleNode->rule()->explicitlyDependsOnFromDependencies.intersects(outTags))
        return true;
    return ruleNode->rule()->auxiliaryInputs.intersects(outTags);
}

void BuildDataResolver::resolveProductBuildData(const ResolvedProductPtr &product)
{
    if (product->buildData)
        return;

    evalContext()->checkForCancelation();

    product->buildData = std::make_unique<ProductBuildData>();
    ArtifactSetByFileTag artifactsPerFileTag;

    for (const auto &dependency : qAsConst(product->dependencies)) {
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
        insertArtifact(product, qbsFileArtifact);
    }
    qbsFileArtifact->addFileTag("qbs");
    artifactsPerFileTag["qbs"].insert(qbsFileArtifact);

    // read sources
    for (const auto &sourceArtifact : product->allEnabledFiles()) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (lookupArtifact(product, filePath))
            continue; // ignore duplicate artifacts

        Artifact *artifact = createArtifact(product, sourceArtifact);
        for (const FileTag &fileTag : artifact->fileTags())
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    RuleGraph ruleGraph;
    ruleGraph.build(product->rules, product->fileTags);
    CreateRuleNodes crn(product);
    ruleGraph.accept(&crn);

    connectRulesToDependencies(product, product->dependencies);
}

static bool isRootRuleNode(RuleNode *ruleNode)
{
    return ruleNode->product->buildData->rootNodes().contains(ruleNode);
}

void BuildDataResolver::connectRulesToDependencies(const ResolvedProductPtr &product,
        const std::vector<ResolvedProductPtr> &dependencies)
{
    // Connect the rules of this product to the compatible rules of all product dependencies.
    // Rules that take "installable" artifacts are connected to all root rules of product
    // dependencies.
    std::vector<RuleNode *> ruleNodes;
    for (RuleNode *ruleNode : filterByType<RuleNode>(product->buildData->allNodes()))
        ruleNodes.push_back(ruleNode);
    for (const auto &dep : dependencies) {
        if (!dep->buildData)
            continue;
        for (RuleNode *depRuleNode : filterByType<RuleNode>(dep->buildData->allNodes())) {
            for (RuleNode *ruleNode : ruleNodes) {
                static const FileTag installableTag("installable");
                if (areRulesCompatible(ruleNode, depRuleNode)
                        || ((ruleNode->rule()->inputsFromDependencies.contains(installableTag)
                             || ruleNode->rule()->auxiliaryInputs.contains(installableTag)
                             || ruleNode->rule()->explicitlyDependsOnFromDependencies.contains(
                                 installableTag))
                            && isRootRuleNode(depRuleNode))) {
                    connect(ruleNode, depRuleNode);
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
