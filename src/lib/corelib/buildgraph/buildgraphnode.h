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

#ifndef QBS_BUILDGRAPHNODE_H
#define QBS_BUILDGRAPHNODE_H

#include "nodeset.h"
#include <language/forward_decls.h>
#include <tools/persistentobject.h>
#include <tools/weakpointer.h>

namespace qbs {
namespace Internal {

class BuildGraphVisitor;
class TopLevelProject;

class BuildGraphNode : public virtual PersistentObject
{
    friend class NodeSet;
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

protected:
    explicit BuildGraphNode();
    void acceptChildren(BuildGraphVisitor *visitor);
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPHNODE_H
