/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#include "requestedartifacts.h"

#include "productbuilddata.h"
#include <language/filetags.h>
#include <language/language.h>
#include <logging/categories.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>

#include <algorithm>

namespace qbs {
namespace Internal {

bool RequestedArtifacts::isUpToDate(const TopLevelProject *project) const
{
    if (m_requestedArtifactsPerProduct.empty())
        return true;
    const std::vector<ResolvedProductPtr> &allProducts = project->allProducts();
    for (const auto &kv : m_requestedArtifactsPerProduct) {
        const QString &productName = kv.first;
        const auto findProduct = [productName](const ResolvedProductConstPtr &p) {
            return p->uniqueName() == productName;
        };
        const auto productIt = std::find_if(allProducts.begin(), allProducts.end(), findProduct);
        if (productIt == allProducts.end()) {
            qCDebug(lcBuildGraph) << "artifacts map not up to date: product" << productName
                                  << "does not exist anymore";
            return false;
        }
        if (!kv.second.isUpToDate(productIt->get()))
            return false;
    }
    return true;
}

void RequestedArtifacts::setAllArtifactTags(const ResolvedProduct *product, bool forceUpdate)
{
    RequestedArtifactsPerProduct &ra = m_requestedArtifactsPerProduct[product->uniqueName()];
    if (!ra.allTags.empty() && !forceUpdate)
        return;
    ra.allTags.clear();
    const ArtifactSetByFileTag artifactsMap = product->buildData->artifactsByFileTag();
    for (auto it = artifactsMap.begin(); it != artifactsMap.end(); ++it)
        ra.allTags.insert(it.key().toString());
}

void RequestedArtifacts::setArtifactsForTag(const ResolvedProduct *product,
                                            const FileTag &tag)
{
    RequestedArtifactsPerProduct &ra = m_requestedArtifactsPerProduct[product->uniqueName()];
    QBS_ASSERT(!ra.allTags.empty(), ;);
    Set<QString> &filePaths = ra.requestedTags[tag.toString()];
    for (const Artifact * const a : product->buildData->artifactsByFileTag().value(tag))
        filePaths.insert(a->filePath());
}

void RequestedArtifacts::setNonExistingTagRequested(const ResolvedProduct *product,
                                                    const QString &tag)
{
    RequestedArtifactsPerProduct &ra = m_requestedArtifactsPerProduct[product->uniqueName()];
    QBS_ASSERT(!ra.allTags.empty(), ;);
    Set<QString> &filePaths = ra.requestedTags[tag];
    QBS_CHECK(filePaths.empty());
}

void RequestedArtifacts::setArtifactsEnumerated(const ResolvedProduct *product)
{
    m_requestedArtifactsPerProduct[product->uniqueName()].artifactsEnumerated = true;
}

void RequestedArtifacts::unite(const RequestedArtifacts &other)
{
    for (const auto &kv : other.m_requestedArtifactsPerProduct)
        m_requestedArtifactsPerProduct[kv.first].unite(kv.second);
}

void RequestedArtifacts::doSanityChecks() const
{
    for (const auto &kv : m_requestedArtifactsPerProduct)
        kv.second.doSanityChecks();
}

void RequestedArtifacts::load(PersistentPool &pool)
{
    serializationOp<PersistentPool::Load>(pool);
}

void RequestedArtifacts::store(PersistentPool &pool)
{
    serializationOp<PersistentPool::Store>(pool);
}

bool RequestedArtifacts::RequestedArtifactsPerProduct::isUpToDate(
        const ResolvedProduct *product) const
{
    if (!product->buildData) {
        qCDebug(lcBuildGraph) << "artifacts map not up to date: product" << product->uniqueName()
                              << "is now disabled";
        return false;
    }

    if (!artifactsEnumerated && requestedTags.empty())
        return true;

    const ArtifactSetByFileTag currentArtifacts = product->buildData->artifactsByFileTag();
    for (const auto &kv : requestedTags) {
        const FileTag tag = FileTag(kv.first.toUtf8());
        const auto currentIt = currentArtifacts.constFind(tag);
        Set<QString> currentFilePathsForTag;
        if (currentIt != currentArtifacts.constEnd()) {
            for (const Artifact * const a : currentIt.value())
                currentFilePathsForTag.insert(a->filePath());
        }
        if (currentFilePathsForTag != kv.second) {
            qCDebug(lcBuildGraph) << "artifacts map not up to date: requested artifact set for "
                                     "file tag" << kv.first << "in product"
                                  << product->uniqueName() << "differs from the current one";
            return false;
        }
    }

    if (!artifactsEnumerated)
        return true;

    Set<QString> currentTags;
    for (auto it = currentArtifacts.begin(); it != currentArtifacts.end(); ++it)
        currentTags.insert(it.key().toString());
    if (currentTags != allTags) {
        qCDebug(lcBuildGraph) << "artifacts map not up to date: overall file tags differ for "
                              << "product" << product->uniqueName();
        return false;
    }
    return true;
}

void RequestedArtifacts::RequestedArtifactsPerProduct::unite(
        const RequestedArtifactsPerProduct &other)
{
    if (allTags.empty()) {
        *this = other;
        return;
    }
    allTags = other.allTags;
    for (const auto &kv : other.requestedTags)
        requestedTags[kv.first] = kv.second;
}

void RequestedArtifacts::RequestedArtifactsPerProduct::doSanityChecks() const
{
    for (const auto &kv : requestedTags)
        QBS_CHECK(allTags.contains(kv.first) || kv.second.empty());
}

void RequestedArtifacts::RequestedArtifactsPerProduct::load(PersistentPool &pool)
{
    serializationOp<PersistentPool::Load>(pool);
}

void RequestedArtifacts::RequestedArtifactsPerProduct::store(PersistentPool &pool)
{
    serializationOp<PersistentPool::Store>(pool);
}

} // namespace Internal
} // namespace qbs
