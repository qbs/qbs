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

// This module comprises functions for collecting values attached to module properties
// in different contexts.
// For example, in the Qt.core module you will find a property binding such as this:
//   cpp.defines: "QT_CORE_LIB"
// while in the Qt.widgets module, it will look like this:
//   cpp.defines: "QT_WIDGETS_LIB"
// A product with a dependency on both these modules will end up with a value of
// ["QT_WIDGETS_LIB", "QT_CORE_LIB"], plus potentially other defines set elsewhere.
// Each of these values is assigned a priority that roughly corresponds to the "level" at which
// the module containing the property binding resides in the dependency hierarchy.
// For list properties, the priorities determine the order of the respecive values in the
// final array, for scalar values they determine which one survives. Different scalar values
// with the same priority trigger a warning message.
// Since the right-hand side of a binding can refer to properties of the surrounding context,
// each such value gets its own scope.

// This function is called when a module is loaded via a Depends item.
// loadingItem is the product or module containing the Depends item.
// loadingName is the name of that module. It is used as a tie-breaker for list property values
//             with equal priority.
// localInstance is the module instance placeholder in the ItemValue of a property binding,
//               i.e. the "cpp" in "cpp.defines".
// globalInstance is the actual module into which the properties from localInstance get merged.
void mergeFromLocalInstance(ProductContext &product, Item *loadingItem,
                            const QString &loadingName, const Item *localInstance,
                            Item *globalInstance, LoaderState &loaderState);

// This function is called after all dependencies have been resolved. It uses its global
// knowledge of module priorities to potentially adjust the order of list values or
// favor different scalar values. It can also remove previously merged-in values again;
// this can happen if a module fails to load after it already merged some values, or
// if it fails validation in the end.
void doFinalMerge(ProductContext &product, LoaderState &loaderState);

} // namespace qbs::Internal
