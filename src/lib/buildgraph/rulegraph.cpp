/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "rulegraph.h"
#include <tools/error.h>
#include <QDebug>

namespace qbs {

RuleGraph::RuleGraph()
{
}

void RuleGraph::build(const QSet<Rule::Ptr> &rules, const QStringList &productFileTags)
{
    QMap<QString, QList<Rule*> > inputFileTagToRule;
    m_artifacts.reserve(rules.count());
    foreach (const Rule::Ptr &rule, rules) {
        foreach (const QString &fileTag, rule->outputFileTags())
            m_outputFileTagToRule[fileTag].append(rule.data());
        insert(rule);
    }

    m_parents.resize(rules.count());
    m_children.resize(rules.count());

    foreach (Rule::Ptr rule, m_artifacts) {
        QStringList inFileTags = rule->inputs;
        inFileTags += rule->explicitlyDependsOn;
        foreach (const QString &fileTag, inFileTags) {
            inputFileTagToRule[fileTag].append(rule.data());
            foreach (Rule *consumingRule, m_outputFileTagToRule.value(fileTag)) {
                connect(rule.data(), consumingRule);
            }
        }
    }

    QList<Rule*> productRules;
    for (int i=0; i < productFileTags.count(); ++i) {
        QList<Rule*> rules = m_outputFileTagToRule.value(productFileTags.at(i));
        productRules += rules;
        //### check: the rule graph must be a in valid shape!
    }
    foreach (Rule *r, productRules)
        m_rootRules += r->ruleGraphId;
}

QList<Rule::Ptr> RuleGraph::topSorted()
{
    QSet<int> rootRules = m_rootRules;
    QList<Rule::Ptr> result;
    foreach (int rootIndex, rootRules) {
        Rule::Ptr rule = m_artifacts.at(rootIndex);
        result.append(topSort(rule));
    }

    // remove duplicates from the result of our post-order traversal
    QSet<Rule*> seenRules;
    seenRules.reserve(result.count());
    for (int i = 0; i < result.count();) {
        Rule *rule = result.at(i).data();
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
    foreach (Rule::Ptr rule, m_artifacts)
        if (m_parents[rule->ruleGraphId].isEmpty())
            rootRules += rule->ruleGraphId;
    foreach (int idx, rootRules) {
        dump_impl(indent, idx);
    }
}

void RuleGraph::dump_impl(QByteArray &indent, int rootIndex) const
{
    Rule::Ptr r = m_artifacts[rootIndex];
    printf("%s", indent.constData());
    printf("%s", qPrintable(r->toString()));
    printf("\n");

    indent.append("  ");
    foreach (int childIndex, m_children[rootIndex])
        dump_impl(indent, childIndex);
    indent.chop(2);
}

int RuleGraph::insert(Rule::Ptr rule)
{
    rule->ruleGraphId = m_artifacts.count();
    m_artifacts.append(rule);
    return rule->ruleGraphId;
}

void RuleGraph::connect(Rule *creatingRule, Rule *consumingRule)
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
    m_artifacts[rule->ruleGraphId] = Rule::Ptr();
    rule->ruleGraphId = -1;
}

void RuleGraph::removeParents(Rule *rule)
{
    foreach (int parentIndex, m_parents[rule->ruleGraphId]) {
        Rule::Ptr parent = m_artifacts.at(parentIndex);
        removeParents(parent.data());
        remove(parent.data());
    }
    m_parents[rule->ruleGraphId].clear();
}

void RuleGraph::removeSiblings(Rule *rule)
{
    foreach (int childIndex, m_children[rule->ruleGraphId]) {
        Rule::Ptr child = m_artifacts.at(childIndex);
        QList<int> toRemove;
        foreach (int siblingIndex, m_parents.at(child->ruleGraphId)) {
            Rule::Ptr sibling = m_artifacts.at(siblingIndex);
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

QList<Rule::Ptr> RuleGraph::topSort(Rule::Ptr rule)
{
    QList<Rule::Ptr> result;
    foreach (int childIndex, m_children.at(rule->ruleGraphId))
        result.append(topSort(m_artifacts.at(childIndex)));

    result.append(rule);
    return result;
}

} // namespace qbs
