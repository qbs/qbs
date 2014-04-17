/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
    loadArtifactSetByFileTag(pool, addedArtifactsByFileTag);
    loadArtifactSetByFileTag(pool, removedArtifactsByFileTag);

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
    storeArtifactSetByFileTag(pool, addedArtifactsByFileTag);
    storeArtifactSetByFileTag(pool, removedArtifactsByFileTag);

    pool.stream() << artifactsWithChangedInputsPerRule.count();
    for (ArtifactSetByRule::ConstIterator it = artifactsWithChangedInputsPerRule.constBegin();
         it != artifactsWithChangedInputsPerRule.constEnd(); ++it) {
        pool.store(it.key());
        pool.storeContainer(it.value());
    }
}

void addArtifactToSet(Artifact *artifact, ProductBuildData::ArtifactSetByFileTag &container)
{
    foreach (const FileTag &tag, artifact->fileTags)
        container[tag] += artifact;
}

} // namespace Internal
} // namespace qbs
