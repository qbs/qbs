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

#include "nodeset.h"

#include "artifact.h"
#include "rulenode.h"
#include <language/language.h>  // because of RulePtr
#include <tools/persistence.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

NodeSet &NodeSet::unite(const NodeSet &other)
{
    for (auto node = other.m_data.cbegin(); node != other.m_data.cend(); ++node)
        insert(*node);
    return *this;
}

void NodeSet::remove(BuildGraphNode *node)
{
    m_data.removeOne(node);
}

void NodeSet::load(PersistentPool &pool)
{
    clear();
    int i = pool.load<int>();
    reserve(i);
    for (; --i >= 0;) {
        const auto t = pool.load<quint8>();
        BuildGraphNode *node = 0;
        switch (static_cast<BuildGraphNode::Type>(t)) {
        case BuildGraphNode::ArtifactNodeType:
            node = pool.load<Artifact *>();
            break;
        case BuildGraphNode::RuleNodeType:
            node = pool.load<RuleNode *>();
            break;
        }
        QBS_CHECK(node);
        m_data << node;
    }
}

void NodeSet::store(PersistentPool &pool) const
{
    pool.store(count());
    for (NodeSet::const_iterator it = constBegin(); it != constEnd(); ++it) {
        pool.store(static_cast<quint8>((*it)->type()));
        pool.store(*it);
    }
}

} // namespace Internal
} // namespace qbs
