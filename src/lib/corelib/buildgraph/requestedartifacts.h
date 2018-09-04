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

#ifndef QBS_REQUESTEDARTIFACTS_H
#define QBS_REQUESTEDARTIFACTS_H

#include <language/forward_decls.h>
#include <tools/qttools.h>
#include <tools/set.h>

#include <QtCore/qstring.h>

#include <unordered_map>

namespace qbs {
namespace Internal {
class FileTag;
class PersistentPool;

class RequestedArtifacts
{
public:
    bool isUpToDate(const TopLevelProject *project) const;

    void clear() { m_requestedArtifactsPerProduct.clear(); }
    void setAllArtifactTags(const ResolvedProduct *product, bool forceUpdate);
    void setArtifactsForTag(const ResolvedProduct *product, const FileTag &tag);
    void setNonExistingTagRequested(const ResolvedProduct *product, const QString &tag);
    void setArtifactsEnumerated(const ResolvedProduct *product);
    void unite(const RequestedArtifacts &other);

    void doSanityChecks() const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool);

    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_requestedArtifactsPerProduct);
    }

private:
    struct RequestedArtifactsPerProduct
    {
        Set<QString> allTags;
        std::unordered_map<QString, Set<QString>> requestedTags;
        bool artifactsEnumerated = false;

        bool isUpToDate(const ResolvedProduct *product) const;

        void unite(const RequestedArtifactsPerProduct &other);

        void doSanityChecks() const;

        template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
        {
            pool.serializationOp<opType>(allTags, requestedTags, artifactsEnumerated);
        }

        void load(PersistentPool &pool);
        void store(PersistentPool &pool);
    };

    std::unordered_map<QString, RequestedArtifactsPerProduct> m_requestedArtifactsPerProduct;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
