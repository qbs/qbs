/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "rulenode.h"

#include "artifact.h"
#include "buildgraph.h"
#include "buildgraphvisitor.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rulesapplicator.h"
#include "transformer.h"
#include <language/language.h>
#include <logging/logger.h>
#include <tools/persistence.h>

namespace qbs {
namespace Internal {

RuleNode::RuleNode()
{
}

RuleNode::~RuleNode()
{
}

void RuleNode::accept(BuildGraphVisitor *visitor)
{
    if (visitor->visit(this))
        acceptChildren(visitor);
}

QString RuleNode::toString() const
{
    return QLatin1String("RULE ") + m_rule->toString();
}

void RuleNode::apply(const Logger &logger, const ArtifactSet &changedInputs,
        ApplicationResult *result)
{
    ArtifactSet allCompatibleInputs = currentInputArtifacts();
    const ArtifactSet addedInputs = allCompatibleInputs - m_oldInputArtifacts;
    const ArtifactSet removedInputs = m_oldInputArtifacts - allCompatibleInputs;
    result->upToDate = changedInputs.isEmpty() && addedInputs.isEmpty() && removedInputs.isEmpty();

    if (logger.traceEnabled()) {
        logger.qbsTrace()
                << "[BG] consider " << (m_rule->isDynamic() ? "dynamic " : "")
                << (m_rule->multiplex ? "multiplex " : "")
                << "rule node " << m_rule->toString()
                << "\n\tchanged: " << changedInputs.toString()
                << "\n\tcompatible: " << allCompatibleInputs.toString()
                << "\n\tadded: " << addedInputs.toString()
                << "\n\tremoved: " << removedInputs.toString();
    }

    ArtifactSet inputs = changedInputs;
    if (product->isMarkedForReapplication(m_rule)) {
        QBS_CHECK(m_rule->multiplex);
        result->upToDate = false;
        product->unmarkForReapplication(m_rule);
        if (logger.traceEnabled())
            logger.qbsTrace() << "[BG] rule is marked for reapplication " << m_rule->toString();
        inputs += allCompatibleInputs;
    } else {
        inputs += addedInputs;
    }
    if (result->upToDate)
        return;
    if (!removedInputs.isEmpty()) {
        ArtifactSet outputArtifactsToRemove;
        foreach (Artifact *artifact, removedInputs) {
            foreach (Artifact *parent, ArtifactSet::fromNodeSet(artifact->parents)) {
                if (!parent->transformer || parent->transformer->rule != m_rule
                        || !parent->transformer->inputs.contains(artifact)) {
                    // TODO: turn two of the three conditions above into QBS_CHECKs
                    // parent must always have a transformer, because it's generated.
                    //     QBS_CHECK(parent->transformer)
                    // artifact is a former input of m_rule and parent was created by m_rule
                    // the inputs of the transformer must contain artifact
                    //     QBS_CHECK(parent->transformer->inputs.contains(artifact))

                    // parent was not created by our rule.
                    continue;
                }
                outputArtifactsToRemove += parent;
            }
        }
        RulesApplicator::handleRemovedRuleOutputs(inputs, outputArtifactsToRemove, logger);
    }
    if (!inputs.isEmpty()) {
        RulesApplicator applicator(product, logger);
        result->createdNodes = applicator.applyRuleInEvaluationContext(m_rule, inputs);
        m_oldInputArtifacts.unite(inputs);
    }
}

void RuleNode::load(PersistentPool &pool)
{
    BuildGraphNode::load(pool);
    m_rule = pool.idLoadS<Rule>();
    pool.loadContainer(m_oldInputArtifacts);
}

void RuleNode::store(PersistentPool &pool) const
{
    BuildGraphNode::store(pool);
    pool.store(m_rule);
    pool.storeContainer(m_oldInputArtifacts);
}

ArtifactSet RuleNode::currentInputArtifacts() const
{
    ArtifactSet s;
    foreach (const FileTag &t, m_rule->inputs) {
        foreach (Artifact *artifact, product->lookupArtifactsByFileTag(t)) {
            if (artifact->transformer && artifact->transformer->rule == m_rule) {
                // Do not add compatible artifacts as inputs that were created by this rule.
                // This can e.g. happen for the ["cpp", "hpp"] -> ["hpp", "cpp", "unmocable"] rule.
                continue;
            }
            s += artifact;
        }
    }

    foreach (const ResolvedProductConstPtr &dep, product->dependencies) {
        if (!dep->buildData)
            continue;
        ArtifactSet artifactsToCheck;
        foreach (Artifact *targetArtifact, dep->targetArtifacts())
            artifactsToCheck += targetArtifact->transformer->outputs;
        foreach (Artifact *artifact, artifactsToCheck) {
            if (artifact->fileTags().matches(m_rule->usings))
                s += artifact;
        }
    }

    return s;
}

} // namespace Internal
} // namespace qbs
