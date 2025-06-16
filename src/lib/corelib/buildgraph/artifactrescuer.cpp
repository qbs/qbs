/****************************************************************************
**
** Copyright (C) 2026 The Qt Company Ltd.
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com)
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

#include "artifactrescuer.h"

#include "artifact.h"
#include "buildgraph.h"
#include "filedependency.h"
#include "nodeset.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rescuableartifactdata.h"
#include "transformer.h"

#include <language/language.h>
#include <logging/categories.h>
#include <tools/qbsassert.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace qbs {
namespace Internal {

ArtifactRescuer::ArtifactRescuer(
    TopLevelProject *project, Logger logger, QStringList &artifactsRemovedFromDisk)
    : m_project(project)
    , m_logger(std::move(logger))
    , m_artifactsRemovedFromDisk(artifactsRemovedFromDisk)
{}

bool ArtifactRescuer::rescueOldBuildData(Artifact *artifact)
{
    bool childrenAdded = false;
    ResolvedProduct * const product = artifact->product.get();
    RescuableArtifactData rad = product->buildData->removeFromRescuableArtifactData(
        artifact->filePath());
    if (!rad.isValid())
        return childrenAdded;
    qCDebug(lcBuildGraph) << "Attempting to rescue data of artifact" << artifact->fileName();

    std::vector<Artifact *> childrenToConnect;
    bool canRescue = artifact->transformer->commands == rad.commands;
    if (canRescue) {
        ResolvedProductPtr pseudoProduct = ResolvedProduct::create();
        for (const RescuableArtifactData::ChildData &cd : rad.children) {
            pseudoProduct->name = cd.productName;
            pseudoProduct->multiplexConfigurationId = cd.productMultiplexId;
            Artifact * const child = lookupArtifact(
                pseudoProduct, m_project->buildData.get(), cd.childFilePath, true);
            if (artifact->children.contains(child))
                continue;
            if (!child) {
                // If a child has disappeared, we must re-build even if the commands
                // are the same. Example: Header file included in cpp file does not exist anymore.
                canRescue = false;
                qCDebug(lcBuildGraph)
                    << "Former child artifact" << cd.childFilePath << "does not exist anymore.";
                const RescuableArtifactData childRad
                    = product->buildData->removeFromRescuableArtifactData(cd.childFilePath);
                if (childRad.isValid()) {
                    m_artifactsRemovedFromDisk << artifact->filePath();
                    removeGeneratedArtifactFromDisk(cd.childFilePath, m_logger);
                }
            }
            if (!cd.addedByScanner) {
                // If an artifact has disappeared from the list of children, the commands
                // might need to run again.
                canRescue = false;
                qCDebug(lcBuildGraph) << "Former child artifact" << cd.childFilePath
                                      << "is no longer in the list of children";
            }
            if (canRescue)
                childrenToConnect.push_back(child);
        }
        for (const QString &depPath : rad.fileDependencies) {
            const auto &depList = m_project->buildData->lookupFiles(depPath);
            if (depList.empty()) {
                canRescue = false;
                qCDebug(lcBuildGraph) << "File dependency" << depPath
                                      << "not in the project's list of dependencies anymore.";
                break;
            }
            const auto depFinder = [](const FileResourceBase *f) {
                return f->fileType() == FileResourceBase::FileTypeDependency;
            };
            const auto depIt = std::find_if(depList.cbegin(), depList.cend(), depFinder);
            if (depIt == depList.cend()) {
                canRescue = false;
                qCDebug(lcBuildGraph) << "File dependency" << depPath
                                      << "not in the project's list of dependencies anymore.";
                break;
            }
            artifact->fileDependencies.insert(static_cast<FileDependency *>(*depIt));
        }

        if (canRescue) {
            const TypeFilter<Artifact> childArtifacts(artifact->children);
            const size_t newChildCount = childrenToConnect.size()
                                         + std::distance(
                                             childArtifacts.begin(), childArtifacts.end());
            QBS_CHECK(newChildCount >= rad.children.size());
            if (newChildCount > rad.children.size()) {
                canRescue = false;
                qCDebug(lcBuildGraph) << "Artifact has children not present in rescue data.";
            }
        }
    } else {
        qCDebug(lcBuildGraph) << "Transformer commands changed.";
    }

    if (canRescue) {
        artifact->setTimestamp(rad.timeStamp);
        artifact->transformer->rescueFromArtifactData(std::move(rad));

        childrenAdded = !childrenToConnect.empty();
        for (Artifact * const child : childrenToConnect) {
            if (safeConnect(artifact, child))
                artifact->childrenAddedByScanner << child;
        }
        qCDebug(lcBuildGraph) << "Data was rescued.";
    } else {
        removeGeneratedArtifactFromDisk(artifact, m_logger);
        m_artifactsRemovedFromDisk << artifact->filePath();
        qCDebug(lcBuildGraph) << "Data not rescued.";
    }
    return childrenAdded;
}

} // namespace Internal
} // namespace qbs
