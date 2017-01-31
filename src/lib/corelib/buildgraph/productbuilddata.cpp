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
#include <tools/persistence.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ProductBuildData::~ProductBuildData()
{
    qDeleteAll(nodes);
}

const TypeFilter<Artifact> ProductBuildData::rootArtifacts() const
{
    return TypeFilter<Artifact>(roots);
}

void ProductBuildData::load(PersistentPool &pool)
{
    nodes.load(pool);
    roots.load(pool);
    pool.load(rescuableArtifactData);
    pool.load(artifactsByFileTag);
    pool.load(artifactsWithChangedInputsPerRule);
}

void ProductBuildData::store(PersistentPool &pool) const
{
    nodes.store(pool);
    roots.store(pool);
    pool.store(rescuableArtifactData);
    pool.store(artifactsByFileTag);
    pool.store(artifactsWithChangedInputsPerRule);
}

void addArtifactToSet(Artifact *artifact, ProductBuildData::ArtifactSetByFileTag &container)
{
    for (const FileTag &tag : artifact->fileTags())
        container[tag] += artifact;
}

void removeArtifactFromSetByFileTag(Artifact *artifact, const FileTag &fileTag,
        ProductBuildData::ArtifactSetByFileTag &container)
{
    ProductBuildData::ArtifactSetByFileTag::iterator it = container.find(fileTag);
    if (it == container.end())
        return;
    it->remove(artifact);
    if (it->isEmpty())
        container.erase(it);
}

void removeArtifactFromSet(Artifact *artifact, ProductBuildData::ArtifactSetByFileTag &container)
{
    for (const FileTag &t : artifact->fileTags())
        removeArtifactFromSetByFileTag(artifact, t, container);
}

} // namespace Internal
} // namespace qbs
