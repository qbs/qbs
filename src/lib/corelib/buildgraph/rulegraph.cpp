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
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

RuleGraph::RuleGraph() = default;

void RuleGraph::build(const std::vector<RulePtr> &rules, const FileTags &productFileTags)
{
    QMap<FileTag, QList<const Rule *> > inputFileTagToRule;
    m_rules.reserve(rules.size());
    for (const RulePtr &rule : rules) {
        for (const FileTag &fileTag : rule->collectedOutputFileTags())
            m_outputFileTagToRule[fileTag].push_back(rule.get());
        insert(rule);
    }

    m_parents.resize(rules.size());
    m_children.resize(rules.size());

    for (const auto &rule : qAsConst(m_rules)) {
        FileTags inFileTags = rule->inputs;
        inFileTags += rule->auxiliaryInputs;
        inFileTags += rule->explicitlyDependsOn;
        for (const FileTag &fileTag : qAsConst(inFileTags)) {
            inputFileTagToRule[fileTag].push_back(rule.get());
            for (const Rule * const producingRule : m_outputFileTagToRule.value(fileTag)) {
                if (!producingRule->collectedOutputFileTags().intersects(
                        rule->excludedInputs)) {
                    connect(rule.get(), producingRule);
                }
            }
        }
    }

    QList<const Rule *> productRules;
    for (const FileTag &productFileTag : productFileTags) {
        QList<const Rule *> rules = m_outputFileTagToRule.value(productFileTag);
        productRules << rules;
        //### check: the rule graph must be a in valid shape!
    }
    for (const Rule *r : qAsConst(productRules))
        m_rootRules += r->ruleGraphId;
}

void RuleGraph::accept(RuleGraphVisitor *visitor) const
{
    const RuleConstPtr nullParent;
    for (int rootIndex : qAsConst(m_rootRules))
        traverse(visitor, nullParent, m_rules.at(rootIndex));
}

void RuleGraph::dump() const
{
    QByteArray indent;
    printf("---rule graph dump:\n");
    Set<int> rootRules;
    for (const auto &rule : qAsConst(m_rules))
        if (m_parents[rule->ruleGraphId].empty())
            rootRules += rule->ruleGraphId;
    for (int idx : qAsConst(rootRules))
        dump_impl(indent, idx);
}

void RuleGraph::dump_impl(QByteArray &indent, int rootIndex) const
{
    const RuleConstPtr r = m_rules[rootIndex];
    printf("%s", indent.constData());
    printf("%s", qPrintable(r->toString()));
    printf("\n");

    indent.append("  ");
    for (int childIndex : qAsConst(m_children[rootIndex]))
        dump_impl(indent, childIndex);
    indent.chop(2);
}

int RuleGraph::insert(const RulePtr &rule)
{
    rule->ruleGraphId = int(m_rules.size());
    m_rules.push_back(rule);
    return rule->ruleGraphId;
}

void RuleGraph::connect(const Rule *creatingRule, const Rule *consumingRule)
{
    int maxIndex = std::max(creatingRule->ruleGraphId, consumingRule->ruleGraphId);
    if (static_cast<int>(m_parents.size()) <= maxIndex) {
        const int c = maxIndex + 1;
        m_parents.resize(c);
        m_children.resize(c);
    }
    m_parents[consumingRule->ruleGraphId].push_back(creatingRule->ruleGraphId);
    m_children[creatingRule->ruleGraphId].push_back(consumingRule->ruleGraphId);
}

void RuleGraph::traverse(RuleGraphVisitor *visitor, const RuleConstPtr &parentRule,
        const RuleConstPtr &rule) const
{
    visitor->visit(parentRule, rule);
    for (int childIndex : m_children.at(rule->ruleGraphId))
        traverse(visitor, rule, m_rules.at(childIndex));
    visitor->endVisit(rule);
}

} // namespace Internal
} // namespace qbs
