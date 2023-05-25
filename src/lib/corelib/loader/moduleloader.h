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


#include <language/forward_decls.h>
#include <language/item.h>
#include <tools/pimpl.h>
#include <tools/set.h>

#include <QString>
#include <QVariantMap>

#include <vector>

namespace qbs {
class CodeLocation;
namespace Internal {
enum class FallbackMode;
class LoaderState;
class StoredModuleProviderInfo;

class ModuleLoader
{
public:
    ModuleLoader(LoaderState &loaderState);
    ~ModuleLoader();

    struct ProductContext {
        Item * const productItem;
        const Item * const projectItem;
        const QString &name;
        const QString &uniqueName;
        const QString &profile;
        const QString &multiplexId;
        const QVariantMap &moduleProperties;
        const QVariantMap &profileModuleProperties;
    };
    struct Result {
        Item *moduleItem = nullptr;
        std::vector<ProbeConstPtr> providerProbes;
    };
    Result searchAndLoadModuleFile(const ProductContext &productContext,
                                   const CodeLocation &dependsItemLocation,
                                   const QualifiedId &moduleName,
                                   FallbackMode fallbackMode, bool isRequired);

    void setStoredModuleProviderInfo(const StoredModuleProviderInfo &moduleProviderInfo);
    StoredModuleProviderInfo storedModuleProviderInfo() const;
    const Set<QString> &tempQbsFiles() const;

    void checkDependencyParameterDeclarations(const Item *productItem,
                                              const QString &productName) const;
    void forwardParameterDeclarations(const Item *dependsItem, const Item::Modules &modules);
    void printProfilingInfo(int indent);

private:
    class Private;
    Pimpl<Private> d;
};

} // namespace Internal
} // namespace qbs

