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

#ifndef QBS_BUILDGRAPHNODE_H
#define QBS_BUILDGRAPHNODE_H

#include "nodeset.h"
#include <language/forward_decls.h>
#include <tools/persistence.h>
#include <tools/weakpointer.h>

namespace qbs {
namespace Internal {

class BuildGraphVisitor;

class BuildGraphNode
{
    friend NodeSet;
public:
    virtual ~BuildGraphNode();

    NodeSet parents;
    NodeSet children;
    WeakPointer<ResolvedProduct> product;

    enum BuildState
    {
        Untouched = 0,
        Buildable,
        Building,
        Built
    };

    BuildState buildState;                  // Do not serialize. Will be refreshed for every build.

    enum Type
    {
        ArtifactNodeType,
        RuleNodeType
    };

    virtual Type type() const = 0;
    virtual void accept(BuildGraphVisitor *visitor) = 0;
    virtual QString toString() const = 0;
    virtual void onChildDisconnected(BuildGraphNode *child);

    bool isBuilt() const { return buildState == Built; }

    virtual void load(PersistentPool &pool);
    virtual void store(PersistentPool &pool);

protected:
    explicit BuildGraphNode();
    void acceptChildren(BuildGraphVisitor *visitor);

    // Do not store parents to avoid recursion.
    // Parents must be updated after loading all nodes.
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(children);
    }
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPHNODE_H
