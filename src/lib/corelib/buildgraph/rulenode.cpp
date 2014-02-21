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
    bool hasAddedTags = false;
    bool hasRemovedTags = false;
    result->upToDate = changedInputs.isEmpty();

    ProductBuildData::ArtifactSetByFileTag relevantArtifacts;
    if (product->isMarkedForReapplication(m_rule)) {
        QBS_CHECK(m_rule->multiplex);
        result->upToDate = false;
        product->unmarkForReapplication(m_rule);
        if (logger.traceEnabled())
            logger.qbsTrace() << "[BG] rule is marked for reapplication " << m_rule->toString();

        foreach (Artifact *artifact, ArtifactSet::fromNodeSet(product->buildData->nodes)) {
            if (m_rule->acceptsAsInput(artifact))
                addArtifactToSet(artifact, relevantArtifacts);
        }
    } else {
        foreach (const FileTag &tag, m_rule->inputs) {
            if (product->addedArtifactsByFileTag(tag).count()) {
                hasAddedTags = true;
                result->upToDate = false;
            }
            if (product->removedArtifactsByFileTag(tag).count()) {
                hasRemovedTags = true;
                result->upToDate = false;
            }
            if (hasAddedTags && hasRemovedTags)
                break;
        }

        relevantArtifacts = product->buildData->addedArtifactsByFileTag;
        if (!changedInputs.isEmpty()) {
            foreach (Artifact *artifact, changedInputs)
                addArtifactToSet(artifact, relevantArtifacts);
        }
    }
    if (result->upToDate)
        return;
    if (hasRemovedTags) {
        ArtifactSet outputArtifactsToRemove;
        foreach (const FileTag &tag, m_rule->inputs) {
            foreach (Artifact *artifact, product->removedArtifactsByFileTag(tag)) {
                foreach (Artifact *parent, ArtifactSet::fromNodeSet(artifact->parents)) {
                    if (!parent->transformer || parent->transformer->rule != m_rule
                            || !parent->transformer->inputs.contains(artifact)) {
                        // parent was not created by our rule.
                        continue;
                    }
                    outputArtifactsToRemove += parent;
                }
            }
        }
        RulesApplicator::handleRemovedRuleOutputs(outputArtifactsToRemove, logger);
    }
    if (!relevantArtifacts.isEmpty()) {
        RulesApplicator applicator(product, relevantArtifacts, logger);
        result->createdNodes = applicator.applyRuleInEvaluationContext(m_rule);
        foreach (BuildGraphNode *node, result->createdNodes) {
            if (Artifact *artifact = dynamic_cast<Artifact *>(node))
                product->registerAddedArtifact(artifact);
        }
    }
}

void RuleNode::load(PersistentPool &pool)
{
    BuildGraphNode::load(pool);
    m_rule = pool.idLoadS<Rule>();
}

void RuleNode::store(PersistentPool &pool) const
{
    BuildGraphNode::store(pool);
    pool.store(m_rule);
}

} // namespace Internal
} // namespace qbs
