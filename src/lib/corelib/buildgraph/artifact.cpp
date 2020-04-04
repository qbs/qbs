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

#include "artifact.h"

#include "transformer.h"
#include "buildgraphvisitor.h"
#include "productbuilddata.h"
#include "rulenode.h"
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <tools/persistence.h>
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

Artifact::Artifact() :
    artifactType(ArtifactType::Unknown),
    inputsScanned(false),
    timestampRetrieved(false),
    alwaysUpdated(false),
    oldDataPossiblyPresent(true)
{
}

Artifact::~Artifact()
{
    for (Artifact *p : parentArtifacts())
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
    return QLatin1String("ARTIFACT ") + filePath() + QLatin1String(" [")
            + (!product.expired() ? product->name : QLatin1String("<null>")) + QLatin1Char(']');
}

void Artifact::addFileTag(const FileTag &t)
{
    m_fileTags += t;
    if (!product.expired() && product->buildData) {
        product->buildData->addFileTagToArtifact(this, t);
        if (product->fileTags.contains(t))
            product->buildData->addRootNode(this);
    }
}

void Artifact::removeFileTag(const FileTag &t)
{
    m_fileTags -= t;
    if (!product.expired() && product->buildData) {
        product->buildData->removeArtifactFromSetByFileTag(this, t);
        if (product->fileTags.contains(t) && !product->fileTags.intersects(m_fileTags))
            product->buildData->removeFromRootNodes(this);
    }
}

void Artifact::setFileTags(const FileTags &newFileTags)
{
    if (product.expired() || !product->buildData) {
        m_fileTags = newFileTags;
        return;
    }
    if (m_fileTags == newFileTags)
        return;
    const Set<FileTag> addedTags = newFileTags - m_fileTags;
    for (const FileTag &t : addedTags)
        addFileTag(t);
    const Set<FileTag> removedTags = m_fileTags - newFileTags;
    for (const FileTag &t : removedTags)
        removeFileTag(t);
}

RuleNode *Artifact::producer() const
{
    if (artifactType == SourceFile)
        return nullptr;
    const auto ruleNodes = filterByType<RuleNode>(children);
    QBS_CHECK(ruleNodes.begin() != ruleNodes.end());
    return *ruleNodes.begin();
}

const TypeFilter<Artifact> Artifact::parentArtifacts() const
{
    return TypeFilter<Artifact>(parents);
}

const TypeFilter<Artifact> Artifact::childArtifacts() const
{
    return TypeFilter<Artifact>(children);
}

void Artifact::onChildDisconnected(BuildGraphNode *child)
{
    if (child->type() != BuildGraphNode::ArtifactNodeType)
        return;
    childrenAddedByScanner.remove(static_cast<Artifact *>(child));
}

void Artifact::load(PersistentPool &pool)
{
    FileResourceBase::load(pool);
    BuildGraphNode::load(pool);
    children.load(pool);

    // restore parents of the loaded children
    for (auto it = children.constBegin(); it != children.constEnd(); ++it)
        (*it)->parents.insert(this);

    pool.load(childrenAddedByScanner);
    pool.load(fileDependencies);
    pool.load(properties);
    pool.load(targetOfModule);
    pool.load(transformer);
    pool.load(m_fileTags);
    pool.load(pureFileTags);
    pool.load(pureProperties);
    artifactType = static_cast<ArtifactType>(pool.load<quint8>());
    alwaysUpdated = pool.load<bool>();
    oldDataPossiblyPresent = pool.load<bool>();
}

void Artifact::store(PersistentPool &pool)
{
    FileResourceBase::store(pool);
    BuildGraphNode::store(pool);
    // Do not store parents to avoid recursion.
    children.store(pool);
    pool.store(childrenAddedByScanner);
    pool.store(fileDependencies);
    pool.store(properties);
    pool.store(targetOfModule);
    pool.store(transformer);
    pool.store(m_fileTags);
    pool.store(pureFileTags);
    pool.store(pureProperties);
    pool.store(static_cast<quint8>(artifactType));
    pool.store(alwaysUpdated);
    pool.store(oldDataPossiblyPresent);
}

} // namespace Internal
} // namespace qbs
