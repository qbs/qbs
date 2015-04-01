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

#include "artifact.h"

#include "transformer.h"
#include "buildgraphvisitor.h"
#include "productbuilddata.h"
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>

QT_BEGIN_NAMESPACE

static QDataStream &operator >>(QDataStream &s, qbs::Internal::Artifact::ArtifactType &t)
{
    int i;
    s >> i;
    t = static_cast<qbs::Internal::Artifact::ArtifactType>(i);
    return s;
}

static QDataStream &operator <<(QDataStream &s, const qbs::Internal::Artifact::ArtifactType &t)
{
    return s << (int)t;
}

QT_END_NAMESPACE

namespace qbs {
namespace Internal {

Artifact::Artifact()
{
    initialize();
}

Artifact::~Artifact()
{
    foreach (Artifact *p, parentArtifacts())
        p->childrenAddedByScanner.remove(this);
}

void Artifact::accept(BuildGraphVisitor *visitor)
{
    if (visitor->visit(this))
        acceptChildren(visitor);
    visitor->endVisit(this);
}

QString Artifact::toString() const
{
    return QLatin1String("ARTIFACT ") + filePath();
}

void Artifact::addFileTag(const FileTag &t)
{
    m_fileTags += t;
    if (!product.isNull() && product->buildData)
        product->buildData->artifactsByFileTag[t] += this;
}

void Artifact::removeFileTag(const FileTag &t)
{
    m_fileTags -= t;
    if (!product.isNull() && product->buildData)
        removeArtifactFromSetByFileTag(this, t, product->buildData->artifactsByFileTag);
}

void Artifact::setFileTags(const FileTags &newFileTags)
{
    if (product.isNull() || !product->buildData) {
        m_fileTags = newFileTags;
        return;
    }
    foreach (const FileTag &t, m_fileTags)
        removeArtifactFromSetByFileTag(this, t, product->buildData->artifactsByFileTag);
    m_fileTags = newFileTags;
    addArtifactToSet(this, product->buildData->artifactsByFileTag);
}

void Artifact::initialize()
{
    artifactType = Unknown;
    inputsScanned = false;
    timestampRetrieved = false;
    alwaysUpdated = true;
    oldDataPossiblyPresent = true;
}

ArtifactSet Artifact::parentArtifacts() const
{
    return ArtifactSet::fromNodeSet(parents);
}

ArtifactSet Artifact::childArtifacts() const
{
    return ArtifactSet::fromNodeSet(children);
}

void Artifact::onChildDisconnected(BuildGraphNode *child)
{
    Artifact *childArtifact = dynamic_cast<Artifact *>(child);
    if (!childArtifact)
        return;
    childrenAddedByScanner.remove(childArtifact);
}

void Artifact::load(PersistentPool &pool)
{
    FileResourceBase::load(pool);
    BuildGraphNode::load(pool);
    children.load(pool);

    // restore parents of the loaded children
    for (NodeSet::const_iterator it = children.constBegin(); it != children.constEnd(); ++it)
        (*it)->parents.insert(this);

    pool.loadContainer(childrenAddedByScanner);
    pool.loadContainer(fileDependencies);
    properties = pool.idLoadS<PropertyMapInternal>();
    transformer = pool.idLoadS<Transformer>();
    unsigned char c;
    pool.stream()
            >> m_fileTags
            >> artifactType
            >> c;
    alwaysUpdated = c;
    pool.stream() >> c;
    oldDataPossiblyPresent = c;
}

void Artifact::store(PersistentPool &pool) const
{
    FileResourceBase::store(pool);
    BuildGraphNode::store(pool);
    // Do not store parents to avoid recursion.
    children.store(pool);
    pool.storeContainer(childrenAddedByScanner);
    pool.storeContainer(fileDependencies);
    pool.store(properties);
    pool.store(transformer);
    pool.stream()
            << m_fileTags
            << artifactType
            << static_cast<unsigned char>(alwaysUpdated)
            << static_cast<unsigned char>(oldDataPossiblyPresent);
}

} // namespace Internal
} // namespace qbs
