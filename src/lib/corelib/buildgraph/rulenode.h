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

#ifndef QBS_RULENODE_H
#define QBS_RULENODE_H

#include "artifact.h"
#include "buildgraphnode.h"
#include "forward_decls.h"
#include <language/forward_decls.h>
#include <tools/dynamictypecheck.h>
#include <tools/persistence.h>

#include <unordered_map>

namespace qbs {
namespace Internal {

class Logger;

class RuleNode : public BuildGraphNode
{
public:
    RuleNode();
    ~RuleNode() override;

    void setRule(const RuleConstPtr &rule) { m_rule = rule; }
    const RuleConstPtr &rule() const { return m_rule; }

    Type type() const override { return RuleNodeType; }
    void accept(BuildGraphVisitor *visitor) override;
    QString toString() const override;

    struct ApplicationResult
    {
        NodeSet createdArtifacts;
        NodeSet invalidatedArtifacts;
        QStringList removedArtifacts;
    };

    void apply(const Logger &logger,
               const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
               const std::unordered_map<QString, const ResolvedProject *> &projectsByName,
               ApplicationResult *result);
    void removeOldInputArtifact(Artifact *artifact);

    void load(PersistentPool &pool) override;
    void store(PersistentPool &pool) override;

    int transformerCount() const;

private:
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_rule, m_oldInputArtifacts, m_oldExplicitlyDependsOn,
                                     m_oldAuxiliaryInputs, m_lastApplicationTime,
                                     m_needsToConsiderChangedInputs);
    }

    ArtifactSet currentInputArtifacts() const;
    ArtifactSet changedInputArtifacts(const ArtifactSet &allCompatibleInputs,
                                      const ArtifactSet &explicitlyDependsOn, const ArtifactSet &auxiliaryInputs) const;

    RuleConstPtr m_rule;

    // These three can contain null pointers, which represent a "dummy artifact" encoding
    // the information that an artifact that used to be in here has ceased to exist.
    // This is okay, because no code outside this class has access to these sets, so
    // we cannot break any assumptions about non-nullness.
    ArtifactSet m_oldInputArtifacts;
    ArtifactSet m_oldExplicitlyDependsOn;
    ArtifactSet m_oldAuxiliaryInputs;

    FileTime m_lastApplicationTime;
    bool m_needsToConsiderChangedInputs = false;
};

template<> inline bool hasDynamicType<RuleNode>(const BuildGraphNode *n)
{
    return n->type() == BuildGraphNode::RuleNodeType;
}

} // namespace Internal
} // namespace qbs

#endif // QBS_RULENODE_H
