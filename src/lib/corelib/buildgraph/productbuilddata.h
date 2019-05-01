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
#ifndef QBS_PRODUCTBUILDDATA_H
#define QBS_PRODUCTBUILDDATA_H

#include "artifact.h"
#include "nodeset.h"
#include "rescuableartifactdata.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <tools/persistence.h>

#include <QtCore/qlist.h>

#include <mutex>

namespace qbs {
namespace Internal {

class Logger;

using ArtifactSetByFileTag = QHash<FileTag, ArtifactSet>;

class QBS_AUTOTEST_EXPORT ProductBuildData
{
public:
    ~ProductBuildData();

    const TypeFilter<Artifact> rootArtifacts() const;
    const NodeSet &allNodes() const { return m_nodes; }
    const NodeSet &rootNodes() const { return m_roots; }

    void addNode(BuildGraphNode *node) { m_nodes.insert(node); }
    void addRootNode(BuildGraphNode *node) { m_roots.insert(node); }
    void removeFromRootNodes(BuildGraphNode *node) { m_roots.remove(node); }
    void addArtifact(Artifact *artifact);
    void addArtifactToSet(Artifact *artifact);
    void removeArtifact(Artifact *artifact);
    void removeArtifactFromSetByFileTag(Artifact *artifact, const FileTag &fileTag);
    void addFileTagToArtifact(Artifact *artifact, const FileTag &tag);

    ArtifactSetByFileTag artifactsByFileTag() const;

    AllRescuableArtifactData rescuableArtifactData() const { return m_rescuableArtifactData; }
    void setRescuableArtifactData(const AllRescuableArtifactData &rad);
    RescuableArtifactData removeFromRescuableArtifactData(const QString &filePath);
    void addRescuableArtifactData(const QString &filePath, const RescuableArtifactData &rad);

    unsigned int buildPriority() const { return m_buildPriority; }
    void setBuildPriority(unsigned int prio) { m_buildPriority = prio; }

    bool checkAndSetJsArtifactsMapUpToDateFlag();

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_nodes, m_roots, m_rescuableArtifactData,
                                     m_artifactsByFileTag);
    }

private:
    void removeArtifactFromSet(Artifact *artifact);

    NodeSet m_nodes;
    NodeSet m_roots;

    // After change tracking, this is the relevant data of artifacts that were in the build data
    // of the restored product, and will potentially be re-created by our rules.
    // If and when that happens, the relevant data will be copied over to the newly created
    // artifact.
    AllRescuableArtifactData m_rescuableArtifactData;

    // Do not store, initialized in executor. Higher prioritized artifacts are built first.
    unsigned int m_buildPriority = 0;

    ArtifactSetByFileTag m_artifactsByFileTag;
    mutable std::mutex m_artifactsMapMutex;

    bool m_jsArtifactsMapUpToDate = true;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_PRODUCTBUILDDATA_H
