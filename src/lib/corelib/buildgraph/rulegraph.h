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

#ifndef QBS_RULEGRAPH_H
#define QBS_RULEGRAPH_H

#include <language/filetags.h>
#include <language/forward_decls.h>
#include <tools/set.h>

#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>

#include <vector>

namespace qbs {
namespace Internal {

class RuleGraphVisitor
{
public:
    virtual ~RuleGraphVisitor() = default;
    virtual void visit(const RuleConstPtr &parentRule, const RuleConstPtr &rule) = 0;
    virtual void endVisit(const RuleConstPtr &rule) { Q_UNUSED(rule); }
};

class RuleGraph
{
public:
    RuleGraph();

    void build(const std::vector<RulePtr> &rules, const FileTags &productFileTag);
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
    std::vector<RulePtr> m_rules;
    std::vector< std::vector<int> > m_parents;
    std::vector< std::vector<int> > m_children;
    Set<int> m_rootRules;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULEGRAPH_H
