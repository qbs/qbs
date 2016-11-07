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
#include "command.h"
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

namespace qbs {
namespace Internal {

static QSet<ResolvedProductPtr> findDependentProducts(const ResolvedProductPtr &product)
{
    QSet<ResolvedProductPtr> result;
    foreach (const ResolvedProductPtr &parent, product->topLevelProject()->allProducts()) {
        if (parent->dependencies.contains(product))
            result += parent;
    }
    return result;
}

class FindLeafRules : public BuildGraphVisitor
{
public:
    FindLeafRules()
    {
    }

    const QSet<RuleNode *> &apply(const ResolvedProductPtr &product)
    {
        m_result.clear();
        m_product = product;
        QBS_CHECK(product->buildData);
        foreach (BuildGraphNode *n, product->buildData->nodes)
            n->accept(this);
        return m_result;
    }

private:
    virtual bool visit(Artifact *)
    {
        return false;
    }

    virtual bool visit(RuleNode *node)
    {
        if (!hasChildRuleInThisProduct(node))
            m_result << node;
        return false;
    }

    bool hasChildRuleInThisProduct(const RuleNode *node) const
    {
        foreach (BuildGraphNode *c, node->children) {
            if (c->product == m_product && c->type() == BuildGraphNode::RuleNodeType)
                return true;
        }
        return false;
    }

    ResolvedProductPtr m_product;
    QSet<RuleNode *> m_result;
};

class FindRootRules : public BuildGraphVisitor
{
public:
    FindRootRules()
    {
    }

    const QList<RuleNode *> &apply(const ResolvedProductPtr &product)
    {
        m_result.clear();
        foreach (BuildGraphNode *n, product->buildData->roots)
            n->accept(this);
        return m_result;
    }

private:
    bool visit(Artifact *)
    {
        return false;
    }

    bool visit(RuleNode *node)
    {
        m_result << node;
        return false;
    }

    QList<RuleNode *> m_result;
};

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
            ? product->name : product->uniqueName();
}

void ProjectBuildData::insertIntoLookupTable(FileResourceBase *fileres)
{
    QList<FileResourceBase *> &lst
            = m_artifactLookupTable[fileres->fileName()][fileres->dirPath()];
    const auto * const artifact = dynamic_cast<Artifact *>(fileres);
    if (artifact && artifact->artifactType == Artifact::Generated) {
        foreach (const auto *file, lst) {
            const auto * const otherArtifact = dynamic_cast<const Artifact *>(file);
            if (otherArtifact) {
                ErrorInfo error;
                error.append(Tr::tr("Conflicting artifacts for file path '%1'.")
                             .arg(artifact->filePath()));
                error.append(Tr::tr("The first artifact comes from product '%1'.")
                             .arg(productNameForErrorMessage(otherArtifact->product.data())),
                             otherArtifact->product->location);
                error.append(Tr::tr("The second artifact comes from product '%1'.")
                             .arg(productNameForErrorMessage(artifact->product.data())),
                             otherArtifact->product->location);
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
    foreach (BuildGraphNode * const child, artifact->children)
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
    foreach (BuildGraphNode * const parent, artifact->parents) {
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
    foreach (const ResolvedProductPtr &product,
             artifact->product->topLevelProject()->allProducts()) {
        if (!product->buildData)
            continue;
        foreach (BuildGraphNode *n, product->buildData->nodes) {
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
    int count;
    pool.stream() >> count;
    fileDependencies.clear();
    fileDependencies.reserve(count);
    for (; --count >= 0;) {
        FileDependency *fileDependency = pool.idLoad<FileDependency>();
        insertFileDependency(fileDependency);
    }
}

void ProjectBuildData::store(PersistentPool &pool) const
{
    pool.storeContainer(fileDependencies);
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
    foreach (ResolvedProductPtr rProduct, allProducts) {
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
    foreach (const ResolvedProductPtr &product, freshProducts) {
        if (product->enabled)
            resolveProductBuildData(product);
    }

    // Connect the leaf rules of all dependent products to the root rules of the dependency.
    foreach (const ResolvedProductPtr &product, freshProducts) {
        if (!product->enabled)
            continue;
        QBS_CHECK(product->buildData);
        const QList<RuleNode *> rootRules = FindRootRules().apply(product);
        QSet<ResolvedProductPtr> dependents = findDependentProducts(product);
        foreach (const ResolvedProductPtr &dependentProduct, dependents) {
            if (!dependentProduct->enabled)
                continue;
            foreach (RuleNode *leaf, FindLeafRules().apply(dependentProduct)) {
                foreach (RuleNode *root, rootRules) {
                    loggedConnect(leaf, root, m_logger);
                }
            }
        }
    }
}

class CreateRuleNodes : public RuleGraphVisitor
{
public:
    CreateRuleNodes(const ResolvedProductPtr &product, const Logger &logger)
        : m_product(product), m_logger(logger)
    {
    }

    const QSet<RuleNode *> &leaves() const
    {
        return m_leaves;
    }

private:
    const ResolvedProductPtr &m_product;
    const Logger &m_logger;
    QHash<RuleConstPtr, RuleNode *> m_nodePerRule;
    QSet<const Rule *> m_rulesOnPath;
    QList<const Rule *> m_rulePath;
    QSet<RuleNode *> m_leaves;

    void visit(const RuleConstPtr &parentRule, const RuleConstPtr &rule)
    {
        if (m_rulesOnPath.contains(rule.data())) {
            QString pathstr;
            foreach (const Rule *r, m_rulePath) {
                pathstr += QLatin1Char('\n') + r->toString() + QLatin1Char('\t')
                        + r->prepareScript->location.toString();
            }
            throw ErrorInfo(Tr::tr("Cycle detected in rule dependencies: %1").arg(pathstr));
        }
        m_rulesOnPath.insert(rule.data());
        m_rulePath.append(rule.data());
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
        m_rulesOnPath.remove(rule.data());
        m_rulePath.removeLast();
    }
};

void BuildDataResolver::resolveProductBuildData(const ResolvedProductPtr &product)
{
    if (product->buildData)
        return;

    evalContext()->checkForCancelation();

    product->buildData.reset(new ProductBuildData);
    ProductBuildData::ArtifactSetByFileTag artifactsPerFileTag;

    foreach (ResolvedProductPtr dependency, product->dependencies) {
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
    foreach (const SourceArtifactConstPtr &sourceArtifact, product->allEnabledFiles()) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (lookupArtifact(product, filePath))
            continue; // ignore duplicate artifacts

        Artifact *artifact = createArtifact(product, sourceArtifact, m_logger);
        foreach (const FileTag &fileTag, artifact->fileTags())
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    RuleGraph ruleGraph;
    ruleGraph.build(product->rules, product->fileTags);
    CreateRuleNodes crn(product, m_logger);
    ruleGraph.accept(&crn);

    // Connect the leaf rules of this product to the root rules of all product dependencies.
    foreach (const ResolvedProductConstPtr &dep, product->dependencies) {
        if (!dep->buildData)
            continue;
        foreach (BuildGraphNode *depRoot, dep->buildData->roots) {
            RuleNode *depRootRule = dynamic_cast<RuleNode *>(depRoot);
            if (!depRootRule)
                continue;
            foreach (RuleNode *leafRule, crn.leaves())
                loggedConnect(leafRule, depRootRule, m_logger);
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
