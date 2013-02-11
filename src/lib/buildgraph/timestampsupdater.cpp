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
#include "timestampsupdater.h"

#include "artifact.h"
#include "artifactvisitor.h"
#include "buildproduct.h"
#include "buildproject.h"
#include "rulesevaluationcontext.h"
#include <language/language.h>
#include <tools/filetime.h>

#include <QFile>

namespace qbs {
namespace Internal {

class TimestampsUpdateVisitor : public ArtifactVisitor
{
public:
    TimestampsUpdateVisitor()
        : ArtifactVisitor(Artifact::Generated), m_now(FileTime::currentTime()) {}

    void visitProduct(const BuildProductConstPtr &product)
    {
        ArtifactVisitor::visitProduct(product);

        // For target artifacts, we have to update the on-disk timestamp, because
        // the executor will look at it.
        foreach (Artifact * const targetArtifact, product->targetArtifacts) {
            if (FileInfo(targetArtifact->filePath()).exists())
                QFile(targetArtifact->filePath()).open(QIODevice::WriteOnly | QIODevice::Append);
        }
    }

private:
    void doVisit(Artifact *artifact)
    {
        if (FileInfo(artifact->filePath()).exists())
            artifact->timestamp = m_now;
    }

    FileTime m_now;
};

void TimestampsUpdater::updateTimestamps(const QList<BuildProductPtr> &products)
{
    TimestampsUpdateVisitor v;
    foreach (const BuildProductPtr &product, products)
        v.visitProduct(product);
    BuildProject * const project = products.first()->project;
    project->markDirty();
    project->store();
}

} // namespace Internal
} // namespace qbs
