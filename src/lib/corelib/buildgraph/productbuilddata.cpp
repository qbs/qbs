/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

ArtifactSet ProductBuildData::rootArtifacts() const
{
    return ArtifactSet::fromNodeSet(roots);
}

static void loadArtifactSetByFileTag(PersistentPool &pool,
                                     ProductBuildData::ArtifactSetByFileTag &s)
{
    int elemCount;
    pool.stream() >> elemCount;
    for (int i = 0; i < elemCount; ++i) {
        QVariant fileTag;
        pool.stream() >> fileTag;
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
        pool.stream() << it.key().toSetting();
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
