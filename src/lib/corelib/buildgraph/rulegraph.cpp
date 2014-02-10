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

#include "rulegraph.h"
#include <language/language.h>
#include <logging/translator.h>
#include <tools/error.h>

namespace qbs {
namespace Internal {

RuleGraph::RuleGraph()
{
}

void RuleGraph::build(const QSet<RulePtr> &rules, const FileTags &productFileTags)
{
    QMap<FileTag, QList<const Rule *> > inputFileTagToRule;
    m_artifacts.reserve(rules.count());
    foreach (const RulePtr &rule, rules) {
        foreach (const FileTag &fileTag, rule->collectedOutputFileTags())
            m_outputFileTagToRule[fileTag].append(rule.data());
        insert(rule);
    }

    m_parents.resize(rules.count());
    m_children.resize(rules.count());

    foreach (const RuleConstPtr &rule, m_artifacts) {
        FileTags inFileTags = rule->inputs;
        inFileTags += rule->auxiliaryInputs;
        inFileTags += rule->explicitlyDependsOn;
        foreach (const FileTag &fileTag, inFileTags) {
            inputFileTagToRule[fileTag].append(rule.data());
            foreach (const Rule * const producingRule, m_outputFileTagToRule.value(fileTag)) {
                if (!producingRule->collectedOutputFileTags().matches(
                        rule->excludedAuxiliaryInputs)) {
                    connect(rule.data(), producingRule);
                }
            }
        }
    }

    QList<const Rule *> productRules;
    foreach (const FileTag &productFileTag, productFileTags) {
        QList<const Rule *> rules = m_outputFileTagToRule.value(productFileTag);
        productRules += rules;
        //### check: the rule graph must be a in valid shape!
    }
    foreach (const Rule *r, productRules)
        m_rootRules += r->ruleGraphId;
}

void RuleGraph::accept(RuleGraphVisitor *visitor) const
{
    const RuleConstPtr nullParent;
    foreach (int rootIndex, m_rootRules)
        traverse(visitor, nullParent, m_artifacts.at(rootIndex));
}

void RuleGraph::dump() const
{
    QByteArray indent;
    printf("---rule graph dump:\n");
    QSet<int> rootRules;
    foreach (const RuleConstPtr &rule, m_artifacts)
        if (m_parents[rule->ruleGraphId].isEmpty())
            rootRules += rule->ruleGraphId;
    foreach (int idx, rootRules) {
        dump_impl(indent, idx);
    }
}

void RuleGraph::dump_impl(QByteArray &indent, int rootIndex) const
{
    const RuleConstPtr r = m_artifacts[rootIndex];
    printf("%s", indent.constData());
    printf("%s", qPrintable(r->toString()));
    printf("\n");

    indent.append("  ");
    foreach (int childIndex, m_children[rootIndex])
        dump_impl(indent, childIndex);
    indent.chop(2);
}

int RuleGraph::insert(const RulePtr &rule)
{
    rule->ruleGraphId = m_artifacts.count();
    m_artifacts.append(rule);
    return rule->ruleGraphId;
}

void RuleGraph::connect(const Rule *creatingRule, const Rule *consumingRule)
{
    int maxIndex = qMax(creatingRule->ruleGraphId, consumingRule->ruleGraphId);
    if (m_parents.count() <= maxIndex) {
        const int c = maxIndex + 1;
        m_parents.resize(c);
        m_children.resize(c);
    }
    m_parents[consumingRule->ruleGraphId].append(creatingRule->ruleGraphId);
    m_children[creatingRule->ruleGraphId].append(consumingRule->ruleGraphId);
}

void RuleGraph::traverse(RuleGraphVisitor *visitor, const RuleConstPtr &parentRule,
        const RuleConstPtr &rule) const
{
    visitor->visit(parentRule, rule);
    foreach (int childIndex, m_children.at(rule->ruleGraphId))
        traverse(visitor, rule, m_artifacts.at(childIndex));
    visitor->endVisit(rule);
}

} // namespace Internal
} // namespace qbs
