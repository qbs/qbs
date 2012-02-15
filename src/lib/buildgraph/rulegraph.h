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

#ifndef RULEGRAPH_H
#define RULEGRAPH_H

#include <language.h>

#include <QtCore/QSet>
#include <QtCore/QVector>

namespace qbs {

class RuleGraph
{
public:
    RuleGraph();

    void build(const QSet<qbs::Rule::Ptr> &rules, const QStringList &productFileTag);
    QList<qbs::Rule::Ptr> topSorted();

    void dump() const;

private:
    void dump_impl(QByteArray &indent, int rootIndex) const;
    int insert(qbs::Rule::Ptr rule);
    void connect(qbs::Rule *creatingRule, qbs::Rule *consumingRule);
    void remove(qbs::Rule *rule);
    void removeParents(qbs::Rule *rule);
    void removeSiblings(qbs::Rule *rule);
    QList<qbs::Rule::Ptr> topSort(qbs::Rule::Ptr rule);

private:
    QMap<QString, QList<qbs::Rule*> > m_outputFileTagToRule;
    QVector<qbs::Rule::Ptr> m_artifacts;
    QVector< QVector<int> > m_parents;
    QVector< QVector<int> > m_children;
    QSet<int> m_rootRules;
};

} // namespace qbs

#endif // RULEGRAPH_H
