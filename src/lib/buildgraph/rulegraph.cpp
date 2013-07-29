/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
        foreach (const FileTag &fileTag, rule->outputFileTags())
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
            foreach (const Rule * const consumingRule, m_outputFileTagToRule.value(fileTag)) {
                connect(rule.data(), consumingRule);
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

QList<RuleConstPtr> RuleGraph::topSorted()
{
    QSet<int> rootRules = m_rootRules;
    QList<RuleConstPtr> result;
    foreach (int rootIndex, rootRules) {
        RuleConstPtr rule = m_artifacts.at(rootIndex);
        result.append(topSort(rule));
    }

    // remove duplicates from the result of our post-order traversal
    QSet<const Rule*> seenRules;
    seenRules.reserve(result.count());
    for (int i = 0; i < result.count();) {
        const Rule * const rule = result.at(i).data();
        if (seenRules.contains(rule))
            result.removeAt(i);
        else
            ++i;
        seenRules.insert(rule);
    }

    return result;
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

void RuleGraph::remove(Rule *rule)
{
    m_parents[rule->ruleGraphId].clear();
    m_children[rule->ruleGraphId].clear();
    m_artifacts[rule->ruleGraphId] = RulePtr();
    rule->ruleGraphId = -1;
}

void RuleGraph::removeParents(const Rule *rule)
{
    foreach (int parentIndex, m_parents[rule->ruleGraphId]) {
        const RulePtr parent = m_artifacts.at(parentIndex);
        removeParents(parent.data());
        remove(parent.data());
    }
    m_parents[rule->ruleGraphId].clear();
}

void RuleGraph::removeSiblings(const Rule *rule)
{
    foreach (int childIndex, m_children[rule->ruleGraphId]) {
        const RuleConstPtr child = m_artifacts.at(childIndex);
        QList<int> toRemove;
        foreach (int siblingIndex, m_parents.at(child->ruleGraphId)) {
            const RulePtr sibling = m_artifacts.at(siblingIndex);
            if (sibling == rule)
                continue;
            toRemove.append(sibling->ruleGraphId);
            remove(sibling.data());
        }
        QVector<int> &parents = m_parents[child->ruleGraphId];
        qSort(parents);
        foreach (int id, toRemove) {
            QVector<int>::iterator it = qBinaryFind(parents.begin(), parents.end(), id);
            if (it != parents.end())
                parents.erase(it);
        }
    }
}

QList<RuleConstPtr> RuleGraph::topSort(const RuleConstPtr &rule)
{
    QList<RuleConstPtr> result;
    foreach (int childIndex, m_children.at(rule->ruleGraphId))
        result.append(topSort(m_artifacts.at(childIndex)));

    result.append(rule);
    return result;
}

} // namespace Internal
} // namespace qbs
