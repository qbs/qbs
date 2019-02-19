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
#include "cycledetector.h"

#include "artifact.h"
#include "buildgraph.h"
#include "projectbuilddata.h"
#include "rulenode.h"

#include <language/language.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

CycleDetector::CycleDetector(Logger logger)
    : m_parent(nullptr), m_logger(std::move(logger))
{
}

void CycleDetector::visitProject(const TopLevelProjectConstPtr &project)
{
    project->accept(this);
}

void CycleDetector::visitProduct(const ResolvedProductConstPtr &product)
{
    product->accept(this);
}

bool CycleDetector::visit(Artifact *artifact)
{
    return visitNode(artifact);
}

bool CycleDetector::visit(RuleNode *ruleNode)
{
    return visitNode(ruleNode);
}

bool CycleDetector::visitNode(BuildGraphNode *node)
{
    if (Q_UNLIKELY(m_nodesInCurrentPath.contains(node))) {
        ErrorInfo error(Tr::tr("Cycle in build graph detected."));
        const auto nodes = cycle(node);
        for (const BuildGraphNode * const n : nodes)
            error.append(n->toString());
        throw error;
    }

    if (m_allNodes.contains(node))
        return false;

    m_nodesInCurrentPath += node;
    m_parent = node;
    for (BuildGraphNode * const child : qAsConst(node->children))
        child->accept(this);
    m_nodesInCurrentPath -= node;
    m_allNodes += node;
    return false;
}

QList<BuildGraphNode *> CycleDetector::cycle(BuildGraphNode *doubleEntry)
{
    QList<BuildGraphNode *> path;
    findPath(doubleEntry, m_parent, path);
    path.push_back(doubleEntry);
    return path;
}

} // namespace Internal
} // namespace qbs
