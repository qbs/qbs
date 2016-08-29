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
    m_rules.reserve(rules.count());
    foreach (const RulePtr &rule, rules) {
        foreach (const FileTag &fileTag, rule->collectedOutputFileTags())
            m_outputFileTagToRule[fileTag].append(rule.data());
        insert(rule);
    }

    m_parents.resize(rules.count());
    m_children.resize(rules.count());

    foreach (const RuleConstPtr &rule, m_rules) {
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
        traverse(visitor, nullParent, m_rules.at(rootIndex));
}

void RuleGraph::dump() const
{
    QByteArray indent;
    printf("---rule graph dump:\n");
    QSet<int> rootRules;
    foreach (const RuleConstPtr &rule, m_rules)
        if (m_parents[rule->ruleGraphId].isEmpty())
            rootRules += rule->ruleGraphId;
    foreach (int idx, rootRules) {
        dump_impl(indent, idx);
    }
}

void RuleGraph::dump_impl(QByteArray &indent, int rootIndex) const
{
    const RuleConstPtr r = m_rules[rootIndex];
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
    rule->ruleGraphId = m_rules.count();
    m_rules.append(rule);
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
        traverse(visitor, rule, m_rules.at(childIndex));
    visitor->endVisit(rule);
}

} // namespace Internal
} // namespace qbs
