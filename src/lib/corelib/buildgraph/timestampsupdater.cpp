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
#include "timestampsupdater.h"

#include "artifact.h"
#include "artifactvisitor.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"

#include <language/language.h>
#include <tools/fileinfo.h>
#include <tools/filetime.h>
#include <tools/qbsassert.h>

#include <QtCore/qfile.h>

namespace qbs {
namespace Internal {

class TimestampsUpdateVisitor : public ArtifactVisitor
{
public:
    TimestampsUpdateVisitor()
        : ArtifactVisitor(Artifact::Generated), m_now(FileTime::currentTime()) {}

    void visitProduct(const ResolvedProductConstPtr &product)
    {
        QBS_CHECK(product->buildData);
        ArtifactVisitor::visitProduct(product);

        // For target artifacts, we have to update the on-disk timestamp, because
        // the executor will look at it.
        for (Artifact * const targetArtifact : product->targetArtifacts()) {
            if (FileInfo(targetArtifact->filePath()).exists())
                QFile(targetArtifact->filePath()).open(QIODevice::WriteOnly | QIODevice::Append);
        }
    }

private:
    void doVisit(Artifact *artifact) override
    {
        if (FileInfo(artifact->filePath()).exists())
            artifact->setTimestamp(m_now);
    }

    FileTime m_now;
};

void TimestampsUpdater::updateTimestamps(const TopLevelProjectPtr &project,
        const QVector<ResolvedProductPtr> &products, const Logger &logger)
{
    TimestampsUpdateVisitor v;
    for (const ResolvedProductPtr &product : products)
        v.visitProduct(product);
    if (!products.empty())
        project->buildData->setDirty();
    project->store(logger);
}

} // namespace Internal
} // namespace qbs
