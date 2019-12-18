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
#include "requesteddependencies.h"

#include <language/language.h>
#include <logging/categories.h>
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

static Set<QString> depNamesForProduct(const ResolvedProduct *p)
{
    Set<QString> names;
    for (const auto &dep : p->dependencies)
        names.insert(dep->uniqueName());
    for (const auto &m : p->modules) {
        if (!m->isProduct)
            names.insert(m->name);
    }
    return names;
}

void RequestedDependencies::set(const Set<const ResolvedProduct *> &products)
{
    m_depsPerProduct.clear();
    add(products);
}

void RequestedDependencies::add(const Set<const ResolvedProduct *> &products)
{
    for (const ResolvedProduct * const p : products)
        m_depsPerProduct[p->uniqueName()] = depNamesForProduct(p);
}

bool RequestedDependencies::isUpToDate(const TopLevelProject *project) const
{
    if (m_depsPerProduct.empty())
        return true;
    for (const auto &product : project->allProducts()) {
        const auto it = m_depsPerProduct.find(product->uniqueName());
        if (it == m_depsPerProduct.cend())
            continue;
        const Set<QString> newDepNames = depNamesForProduct(product.get());
        if (newDepNames != it->second) {
            qCDebug(lcBuildGraph) << "dependencies list was accessed for product"
                                  << product->fullDisplayName() << "and dependencies have changed.";
            return false;
        }
    }
    return true;
}

} // namespace Internal
} // namespace qbs
