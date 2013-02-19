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

#ifndef QBS_RULEGRAPH_H
#define QBS_RULEGRAPH_H

#include <language/filetags.h>
#include <language/forward_decls.h>

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>

namespace qbs {
namespace Internal {

class RuleGraph
{
public:
    RuleGraph();

    void build(const QSet<RulePtr> &rules, const FileTags &productFileTag);
    QList<RuleConstPtr> topSorted();

    void dump() const;

private:
    void dump_impl(QByteArray &indent, int rootIndex) const;
    int insert(const RulePtr &rule);
    void connect(const Rule *creatingRule, const Rule *consumingRule);
    void remove(Rule *rule);
    void removeParents(const Rule *rule);
    void removeSiblings(const Rule *rule);
    QList<RuleConstPtr> topSort(const RuleConstPtr &rule);

private:
    QMap<FileTag, QList<const Rule*> > m_outputFileTagToRule;
    QVector<RulePtr> m_artifacts;
    QVector< QVector<int> > m_parents;
    QVector< QVector<int> > m_children;
    QSet<int> m_rootRules;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULEGRAPH_H
