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
#include "command.h"
#include "projectbuilddata.h"
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

static void loadArtifactSetByFileTag(PersistentPool &pool,
                                     ProductBuildData::ArtifactSetByFileTag &s)
{
    int elemCount;
    pool.stream() >> elemCount;
    for (int i = 0; i < elemCount; ++i) {
        const QVariant fileTag = pool.loadVariant();
        ArtifactSet artifacts;
        pool.loadContainer(artifacts);
        s.insert(FileTag::fromSetting(fileTag), artifacts);
    }
}

void ProductBuildData::load(PersistentPool &pool)
{
    nodes.load(pool);
    roots.load(pool);
    int count;
    pool.stream() >> count;
    rescuableArtifactData.reserve(count);
    for (int i = 0; i < count; ++i) {
        const QString filePath = pool.idLoadString();
        RescuableArtifactData elem;
        elem.load(pool);
        rescuableArtifactData.insert(filePath, elem);
    }
    loadArtifactSetByFileTag(pool, artifactsByFileTag);

    pool.stream() >> count;
    for (int i = 0; i < count; ++i) {
        const RulePtr r = pool.idLoadS<Rule>();
        ArtifactSet s;
        pool.loadContainer(s);
        artifactsWithChangedInputsPerRule.insert(r, s);
    }
}

static void storeArtifactSetByFileTag(PersistentPool &pool,
                                      const ProductBuildData::ArtifactSetByFileTag &s)
{
    pool.stream() << s.count();
    ProductBuildData::ArtifactSetByFileTag::ConstIterator it;
    for (it = s.constBegin(); it != s.constEnd(); ++it) {
        pool.store(it.key().toSetting());
        pool.storeContainer(it.value());
    }
}

void ProductBuildData::store(PersistentPool &pool) const
{
    nodes.store(pool);
    roots.store(pool);
    pool.stream() << rescuableArtifactData.count();
    for (AllRescuableArtifactData::ConstIterator it = rescuableArtifactData.constBegin();
             it != rescuableArtifactData.constEnd(); ++it) {
        pool.storeString(it.key());
        it.value().store(pool);
    }
    storeArtifactSetByFileTag(pool, artifactsByFileTag);

    pool.stream() << artifactsWithChangedInputsPerRule.count();
    for (ArtifactSetByRule::ConstIterator it = artifactsWithChangedInputsPerRule.constBegin();
         it != artifactsWithChangedInputsPerRule.constEnd(); ++it) {
        pool.store(it.key());
        pool.storeContainer(it.value());
    }
}

void addArtifactToSet(Artifact *artifact, ProductBuildData::ArtifactSetByFileTag &container)
{
    foreach (const FileTag &tag, artifact->fileTags())
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
    foreach (const FileTag &t, artifact->fileTags())
        removeArtifactFromSetByFileTag(artifact, t, container);
}

} // namespace Internal
} // namespace qbs
