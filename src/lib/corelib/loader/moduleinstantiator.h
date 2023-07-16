/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#pragma once

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs::Internal {
class Item;
class LoaderState;
class ProductContext;
class QualifiedId;

class InstantiationContext {
public:
    ProductContext &product;
    Item * const loadingItem;
    const QString &loadingName;
    Item * const module;
    Item * const moduleWithSameName;
    Item * const exportingProduct;
    const QualifiedId &moduleName;
    const QString &id;
    const bool alreadyLoaded;
};

// This function is responsible for setting up a proper module instance from a bunch of items:
//    - Set the item type to ItemType::ModuleInstance (from Module or Export).
//    - Apply possible command-line overrides for module properties.
//    - Replace a possible module instance placeholder in the loading item with the actual instance
//      and merge their values employing the ModulePropertyMerger.
//    - Setting up the module instance scope.
void instantiateModule(const InstantiationContext &context, LoaderState &loaderState);

// Helper functions for retrieving/setting module instance items for special purposes.
// Note that these will also create the respective item value if it does not exist yet.
Item *retrieveModuleInstanceItem(Item *containerItem, const QualifiedId &name,
                                 LoaderState &loaderState);
Item *retrieveQbsItem(Item *containerItem, LoaderState &loaderState);

} // namespace qbs::Internal

