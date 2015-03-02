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

#ifndef QBS_RULENODE_H
#define QBS_RULENODE_H

#include "artifactset.h"
#include "buildgraphnode.h"
#include "forward_decls.h"
#include <language/forward_decls.h>

namespace qbs {
namespace Internal {

class Logger;

class RuleNode : public BuildGraphNode
{
public:
    RuleNode();
    ~RuleNode();

    void setRule(const RuleConstPtr &rule) { m_rule = rule; }
    const RuleConstPtr &rule() const { return m_rule; }

    Type type() const { return RuleNodeType; }
    void accept(BuildGraphVisitor *visitor);
    QString toString() const;

    struct ApplicationResult
    {
        bool upToDate;
        NodeSet createdNodes;
        NodeSet invalidatedNodes;
    };

    void apply(const Logger &logger, const ArtifactSet &changedInputs, ApplicationResult *result);
    void removeOldInputArtifact(Artifact *artifact) { m_oldInputArtifacts.remove(artifact); }

protected:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    ArtifactSet currentInputArtifacts() const;

    RuleConstPtr m_rule;
    ArtifactSet m_oldInputArtifacts;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_RULENODE_H
