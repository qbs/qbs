/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
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

class RuleGraphVisitor
{
public:
    virtual void visit(const RuleConstPtr &parentRule, const RuleConstPtr &rule) = 0;
    virtual void endVisit(const RuleConstPtr &rule) { Q_UNUSED(rule); }
};

class RuleGraph
{
public:
    RuleGraph();

    void build(const QSet<RulePtr> &rules, const FileTags &productFileTag);
    void accept(RuleGraphVisitor *visitor) const;

    void dump() const;

private:
    void dump_impl(QByteArray &indent, int rootIndex) const;
    int insert(const RulePtr &rule);
    void connect(const Rule *creatingRule, const Rule *consumingRule);
    void traverse(RuleGraphVisitor *visitor, const RuleConstPtr &parentRule,
            const RuleConstPtr &rule) const;

private:
    QMap<FileTag, QList<const Rule*> > m_outputFileTagToRule;
    QVector<RulePtr> m_rules;
    QVector< QVector<int> > m_parents;
    QVector< QVector<int> > m_children;
    QSet<int> m_rootRules;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULEGRAPH_H
