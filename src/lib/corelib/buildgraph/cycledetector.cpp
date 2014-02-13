/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "cycledetector.h"

#include "artifact.h"
#include "buildgraph.h"
#include "projectbuilddata.h"
#include "rulenode.h"

#include <language/language.h>
#include <logging/translator.h>
#include <tools/error.h>

namespace qbs {
namespace Internal {

CycleDetector::CycleDetector(const Logger &logger)
    : m_parent(0), m_logger(logger)
{
}

void CycleDetector::visitProject(const TopLevelProjectConstPtr &project)
{
    const QString description = QString::fromLocal8Bit("Cycle detection for project '%1'")
            .arg(project->name);
    TimedActivityLogger timeLogger(m_logger, description, QLatin1String("[BG] "), LoggerTrace);

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
        foreach (const BuildGraphNode * const n, cycle(node))
            error.append(n->toString());
        throw error;
    }

    if (m_allNodes.contains(node))
        return false;

    m_nodesInCurrentPath += node;
    m_parent = node;
    foreach (BuildGraphNode * const child, node->children)
        child->accept(this);
    m_nodesInCurrentPath -= node;
    m_allNodes += node;
    return false;
}

QList<BuildGraphNode *> CycleDetector::cycle(BuildGraphNode *doubleEntry)
{
    QList<BuildGraphNode *> path;
    findPath(doubleEntry, m_parent, path);
    return path << doubleEntry;
}

} // namespace Internal
} // namespace qbs
