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
#include "artifactvisitor.h"

#include "artifact.h"
#include "productbuilddata.h"
#include <language/language.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ArtifactVisitor::ArtifactVisitor(int artifactType) : m_artifactType(artifactType)
{
}

void ArtifactVisitor::visitArtifact(Artifact *artifact)
{
    QBS_ASSERT(artifact, return);
    if (m_allArtifacts.contains(artifact))
        return;
    m_allArtifacts << artifact;
    if (m_artifactType & artifact->artifactType)
        doVisit(artifact);
    else if (m_artifactType == Artifact::Generated)
        return;
    foreach (Artifact * const child, artifact->children)
        visitArtifact(child);
}

void ArtifactVisitor::visitProduct(const ResolvedProductConstPtr &product)
{
    if (!product->buildData)
        return;
    foreach (Artifact * const artifact, product->buildData->targetArtifacts)
        visitArtifact(artifact);
}

void ArtifactVisitor::visitProject(const ResolvedProjectConstPtr &project)
{
    foreach (const ResolvedProductConstPtr &product, project->products)
        visitProduct(product);
    foreach (const ResolvedProjectConstPtr &subProject, project->subProjects)
        visitProject(subProject);
}

} // namespace Internal
} // namespace qbs
