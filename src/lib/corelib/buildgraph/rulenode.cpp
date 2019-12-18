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

#include "rulenode.h"

#include "buildgraph.h"
#include "buildgraphvisitor.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rulesapplicator.h"
#include "transformer.h"
#include "transformerchangetracking.h"

#include <language/language.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

RuleNode::RuleNode() = default;

RuleNode::~RuleNode() = default;

void RuleNode::accept(BuildGraphVisitor *visitor)
{
    if (visitor->visit(this))
        acceptChildren(visitor);
    visitor->endVisit(this);
}

QString RuleNode::toString() const
{
    return QLatin1String("RULE ") + m_rule->toString() + QLatin1String(" [")
            + (!product.expired() ? product->name : QLatin1String("<null>")) + QLatin1Char(']')
            + QLatin1String(" located at ") + m_rule->prepareScript.location().toString();
}

void RuleNode::apply(const Logger &logger,
                     const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
                     const std::unordered_map<QString, const ResolvedProject *> &projectsByName,
                     ApplicationResult *result)
{
    ArtifactSet allCompatibleInputs = currentInputArtifacts();
    const ArtifactSet explicitlyDependsOn
            = RulesApplicator::collectExplicitlyDependsOn(m_rule.get(), product.get());
    const ArtifactSet auxiliaryInputs
            = RulesApplicator::collectAuxiliaryInputs(m_rule.get(), product.get());
    const ArtifactSet addedInputs = allCompatibleInputs - m_oldInputArtifacts;
    const ArtifactSet removedInputs = m_oldInputArtifacts - allCompatibleInputs;
    const ArtifactSet changedInputs = changedInputArtifacts(allCompatibleInputs,
                                                            explicitlyDependsOn,
                                                            auxiliaryInputs);
    bool upToDate = changedInputs.empty() && addedInputs.empty() && removedInputs.empty();

    qCDebug(lcBuildGraph).noquote().nospace()
            << "consider " << (m_rule->isDynamic() ? "dynamic " : "")
            << (m_rule->multiplex ? "multiplex " : "")
            << "rule node " << m_rule->toString()
            << "\n\tchanged: " << changedInputs.toString()
            << "\n\tcompatible: " << allCompatibleInputs.toString()
            << "\n\tadded: " << addedInputs.toString()
            << "\n\tremoved: " << removedInputs.toString();

    ArtifactSet inputs = changedInputs;
    if (m_rule->multiplex)
        inputs = allCompatibleInputs;
    else
        inputs += addedInputs;

    for (Artifact * const input : allCompatibleInputs) {
        for (const Artifact * const output : input->parentArtifacts()) {
            if (output->transformer->rule != m_rule)
                continue;
            if (prepareScriptNeedsRerun(output->transformer.get(),
                                        output->transformer->product().get(),
                                        productsByName, projectsByName)) {
                upToDate = false;
                inputs += input;
            }
            break;
        }
        if (m_rule->multiplex)
            break;
    }

    // Handle rules without inputs: We want to run such a rule if and only if it has not run yet
    // or its transformer is not up to date regarding the prepare script.
    if (upToDate && (!m_rule->declaresInputs() || !m_rule->requiresInputs) && inputs.empty()) {
        bool hasOutputs = false;
        for (const Artifact * const output : filterByType<Artifact>(parents)) {
            if (output->transformer->rule != m_rule)
                continue;
            hasOutputs = true;
            if (prepareScriptNeedsRerun(output->transformer.get(),
                                        output->transformer->product().get(),
                                        productsByName, projectsByName)) {
                upToDate = false;
                break;
            }
            if (m_rule->multiplex)
                break;
        }
        if (!hasOutputs)
            upToDate = false;
    }

    if (upToDate) {
        qCDebug(lcExec) << "rule is up to date. Skipping.";
        return;
    }

    const bool mustApplyRule = !inputs.empty() || !m_rule->declaresInputs()
            || !m_rule->requiresInputs;

    // For a non-multiplex rule, the removal of an input always implies that the
    // corresponding outputs disappear.
    // For a multiplex rule, the outputs disappear only if *all* inputs are gone *and*
    // the rule requires inputs. This is exactly the opposite condition of whether to
    // re-apply the rule.
    const bool removedInputForcesOutputRemoval = !m_rule->multiplex || !mustApplyRule;
    ArtifactSet outputArtifactsToRemove;
    std::vector<std::pair<Artifact *, Artifact *>> connectionsToBreak;
    for (Artifact * const artifact : removedInputs) {
        if (!artifact) // dummy artifact
            continue;
        for (Artifact *parent : filterByType<Artifact>(artifact->parents)) {
            if (parent->transformer->rule != m_rule) {
                // parent was not created by our rule.
                continue;
            }

            // parent must always have a transformer, because it's generated.
            QBS_CHECK(parent->transformer);

            // artifact is a former input of m_rule and parent was created by m_rule
            // the inputs of the transformer must contain artifact
            QBS_CHECK(parent->transformer->inputs.contains(artifact));

            if (removedInputForcesOutputRemoval)
                outputArtifactsToRemove += parent;
            else
                connectionsToBreak.emplace_back(parent, artifact);
        }
        disconnect(this, artifact);
    }
    for (const auto &connection : connectionsToBreak)
        disconnect(connection.first, connection.second);
    if (!outputArtifactsToRemove.empty()) {
        RulesApplicator::handleRemovedRuleOutputs(inputs, outputArtifactsToRemove,
                                                  result->removedArtifacts, logger);
    }

    if (mustApplyRule) {
        RulesApplicator applicator(product.lock(), productsByName, projectsByName, logger);
        applicator.applyRule(this, inputs, explicitlyDependsOn);
        result->createdArtifacts = applicator.createdArtifacts();
        result->invalidatedArtifacts = applicator.invalidatedArtifacts();
        m_lastApplicationTime = FileTime::currentTime();
        if (applicator.ruleUsesIo())
            m_needsToConsiderChangedInputs = true;
    } else {
        qCDebug(lcExec).noquote() << "prepare script does not need to run";
    }
    m_oldInputArtifacts = allCompatibleInputs;
    m_oldExplicitlyDependsOn = explicitlyDependsOn;
    m_oldAuxiliaryInputs = auxiliaryInputs;
    product->topLevelProject()->buildData->setDirty();
}

void RuleNode::load(PersistentPool &pool)
{
    BuildGraphNode::load(pool);
    serializationOp<PersistentPool::Load>(pool);
}

void RuleNode::store(PersistentPool &pool)
{
    BuildGraphNode::store(pool);
    serializationOp<PersistentPool::Store>(pool);
}

int RuleNode::transformerCount() const
{
    Set<const Transformer *> transformers;
    for (const Artifact * const output : filterByType<Artifact>(parents))
        transformers.insert(output->transformer.get());
    return int(transformers.size());
}

ArtifactSet RuleNode::currentInputArtifacts() const
{
    ArtifactSet s;
    for (const FileTag &t : qAsConst(m_rule->inputs)) {
        for (Artifact *artifact : product->lookupArtifactsByFileTag(t)) {
            if (artifact->isTargetOfModule())
                continue;
            if (artifact->transformer && artifact->transformer->rule == m_rule) {
                // Do not add compatible artifacts as inputs that were created by this rule.
                // This can e.g. happen for the ["cpp", "hpp"] -> ["hpp", "cpp", "unmocable"] rule.
                continue;
            }
            if (artifact->fileTags().intersects(m_rule->excludedInputs))
                continue;
            s += artifact;
        }
    }

    if (m_rule->inputsFromDependencies.empty())
        return s;
    for (const FileTag &t : qAsConst(m_rule->inputsFromDependencies)) {
        for (Artifact *artifact : product->lookupArtifactsByFileTag(t)) {
            if (!artifact->isTargetOfModule())
                continue;
            if (artifact->transformer && artifact->transformer->rule == m_rule)
                continue;
            if (artifact->fileTags().intersects(m_rule->excludedInputs))
                continue;
            s += artifact;
        }
    }

    for (const auto &dep : qAsConst(product->dependencies)) {
        if (!dep->buildData)
            continue;
        for (Artifact * const a : filterByType<Artifact>(dep->buildData->allNodes())) {
            if (a->fileTags().intersects(m_rule->inputsFromDependencies)
                    && !a->fileTags().intersects(m_rule->excludedInputs))
                s += a;
        }
    }

    return s;
}

ArtifactSet RuleNode::changedInputArtifacts(const ArtifactSet &allCompatibleInputs,
                                            const ArtifactSet &explicitlyDependsOn,
                                            const ArtifactSet &auxiliaryInputs) const
{
    ArtifactSet changedInputArtifacts;
    if (explicitlyDependsOn != m_oldExplicitlyDependsOn)
        return allCompatibleInputs;
    if (!m_needsToConsiderChangedInputs)
        return changedInputArtifacts;

    for (Artifact * const artifact : explicitlyDependsOn) {
        if (artifact->timestamp() > m_lastApplicationTime)
            return allCompatibleInputs;
    }
    if (auxiliaryInputs != m_oldAuxiliaryInputs)
        return allCompatibleInputs;
    for (Artifact * const artifact : auxiliaryInputs) {
        if (artifact->timestamp() > m_lastApplicationTime)
            return allCompatibleInputs;
    }
    for (Artifact * const artifact : allCompatibleInputs) {
        if (artifact->timestamp() > m_lastApplicationTime)
            changedInputArtifacts.insert(artifact);
    }
    return changedInputArtifacts;
}

void RuleNode::removeOldInputArtifact(Artifact *artifact)
{
    if (m_oldInputArtifacts.remove(artifact)) {
        qCDebug(lcBuildGraph) << "remove old input" << artifact->filePath()
                              << "from rule" << rule()->toString();
        m_oldInputArtifacts.insert(nullptr);
    }
    if (m_oldExplicitlyDependsOn.remove(artifact)) {
        qCDebug(lcBuildGraph) << "remove old explicitlyDependsOn" << artifact->filePath()
                              << "from rule" << rule()->toString();
        m_oldExplicitlyDependsOn.insert(nullptr);
    }
    if (m_oldAuxiliaryInputs.remove(artifact)) {
        qCDebug(lcBuildGraph) << "remove old auxiliaryInput" << artifact->filePath()
                              << "from rule" << rule()->toString();
        m_oldAuxiliaryInputs.insert(nullptr);
    }
}

} // namespace Internal
} // namespace qbs
