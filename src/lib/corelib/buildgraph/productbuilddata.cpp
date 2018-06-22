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
#include "productbuilddata.h"

#include "artifact.h"
#include "projectbuilddata.h"
#include "rulecommands.h"
#include <language/language.h>
#include <logging/logger.h>
#include <tools/error.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ProductBuildData::~ProductBuildData()
{
    qDeleteAll(m_nodes);
}

const TypeFilter<Artifact> ProductBuildData::rootArtifacts() const
{
    return TypeFilter<Artifact>(m_roots);
}

void ProductBuildData::addArtifact(Artifact *artifact)
{
    QBS_CHECK(m_nodes.insert(artifact).second);
    addArtifactToSet(artifact);
}

void ProductBuildData::addArtifactToSet(Artifact *artifact)
{
    std::lock_guard<std::mutex> l(m_artifactsMapMutex);
    for (const FileTag &tag : artifact->fileTags()) {
        m_artifactsByFileTag[tag] += artifact;
        m_jsArtifactsMapUpToDate = false;
    }
}

void ProductBuildData::removeArtifact(Artifact *artifact)
{
    m_roots.remove(artifact);
    m_nodes.remove(artifact);
    removeArtifactFromSet(artifact);
}

void ProductBuildData::removeArtifactFromSetByFileTag(Artifact *artifact, const FileTag &fileTag)
{
    std::lock_guard<std::mutex> l(m_artifactsMapMutex);
    const auto it = m_artifactsByFileTag.find(fileTag);
    if (it == m_artifactsByFileTag.end())
        return;
    it->remove(artifact);
    if (it->empty())
        m_artifactsByFileTag.erase(it);
    m_jsArtifactsMapUpToDate = false;
}

void ProductBuildData::addFileTagToArtifact(Artifact *artifact, const FileTag &tag)
{
    std::lock_guard<std::mutex> l(m_artifactsMapMutex);
    m_artifactsByFileTag[tag] += artifact;
    m_jsArtifactsMapUpToDate = false;
}

ArtifactSetByFileTag ProductBuildData::artifactsByFileTag() const
{
    std::lock_guard<std::mutex> l(m_artifactsMapMutex);
    return m_artifactsByFileTag;
}

void ProductBuildData::setRescuableArtifactData(const AllRescuableArtifactData &rad)
{
    m_rescuableArtifactData = rad;
}

RescuableArtifactData ProductBuildData::removeFromRescuableArtifactData(const QString &filePath)
{
    return m_rescuableArtifactData.take(filePath);
}

void ProductBuildData::addRescuableArtifactData(const QString &filePath,
                                                const RescuableArtifactData &rad)
{
    m_rescuableArtifactData.insert(filePath, rad);
}

bool ProductBuildData::checkAndSetJsArtifactsMapUpToDateFlag()
{
    std::lock_guard<std::mutex> l(m_artifactsMapMutex);
    if (!m_jsArtifactsMapUpToDate) {
        m_jsArtifactsMapUpToDate = true;
        return false;
    }
    return true;
}

void ProductBuildData::removeArtifactFromSet(Artifact *artifact)
{
    for (const FileTag &t : artifact->fileTags())
        removeArtifactFromSetByFileTag(artifact, t);
}

} // namespace Internal
} // namespace qbs
