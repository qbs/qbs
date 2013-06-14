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
#include "cycledetector.h"

#include "artifact.h"
#include "buildgraph.h"

#include <language/language.h>
#include <logging/translator.h>
#include <tools/error.h>

namespace qbs {
namespace Internal {

CycleDetector::CycleDetector(const Logger &logger)
    : ArtifactVisitor(0), m_parent(0), m_logger(logger)
{
}

void CycleDetector::visitProject(const ResolvedProjectConstPtr &project)
{
    const QString description = QString::fromLocal8Bit("Cycle detection for project '%1'")
            .arg(project->name);
    TimedActivityLogger timeLogger(m_logger, description, QLatin1String("[BG] "), LoggerTrace);
    ArtifactVisitor::visitProject(project);
}

void CycleDetector::visitArtifact(Artifact *artifact)
{
    if (Q_UNLIKELY(m_artifactsInCurrentPath.contains(artifact))) {
        ErrorInfo error(Tr::tr("Cycle in build graph detected."));
        foreach (const Artifact * const a, cycle(artifact))
            error.append(a->filePath());
        throw error;
    }

    if (m_allArtifacts.contains(artifact))
        return;

    m_artifactsInCurrentPath += artifact;
    m_parent = artifact;
    foreach (Artifact * const child, artifact->children)
        visitArtifact(child);
    m_artifactsInCurrentPath -= artifact;
    m_allArtifacts += artifact;
}

void CycleDetector::doVisit(Artifact *) { }

QList<Artifact *> CycleDetector::cycle(Artifact *doubleEntry)
{
    QList<Artifact *> path;
    findPath(doubleEntry, m_parent, path);
    return path << doubleEntry;
}

} // namespace Internal
} // namespace qbs
