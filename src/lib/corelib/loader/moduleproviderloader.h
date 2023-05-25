/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

#ifndef MODULEPROVIDERLOADER_H
#define MODULEPROVIDERLOADER_H

#include "loaderutils.h"

#include <language/forward_decls.h>
#include <language/moduleproviderinfo.h>

#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>

#include <optional>
#include <vector>

namespace qbs::Internal {
class Item;
class LoaderState;

class ModuleProviderLoader
{
public:
    explicit ModuleProviderLoader(LoaderState &loaderState);

    enum class ModuleProviderLookup { Scoped, Named, Fallback };

    struct Provider
    {
        QualifiedId name;
        ModuleProviderLookup lookup;
    };

    struct ModuleProviderResult
    {
        std::vector<ProbeConstPtr> probes;
        QVariantMap providerConfig;
        bool providerFound = false;
        std::optional<QStringList> searchPaths;
    };

    const StoredModuleProviderInfo &storedModuleProviderInfo() const
    {
        return m_storedModuleProviderInfo;
    }

    void setStoredModuleProviderInfo(StoredModuleProviderInfo moduleProviderInfo)
    {
        m_storedModuleProviderInfo = std::move(moduleProviderInfo);
    }

    const Set<QString> &tempQbsFiles() const { return m_tempQbsFiles; }

    struct ProductContext {
        Item * const productItem;
        const Item * const projectItem;
        const QString &name;
        const QString &uniqueName;
        const QVariantMap &moduleProperties;
        const std::optional<QVariantMap> providerConfig;
    };
    ModuleProviderResult executeModuleProviders(
            const ProductContext &productContext,
            const CodeLocation &dependsItemLocation,
            const QualifiedId &moduleName,
            FallbackMode fallbackMode);

private:
    ModuleProviderResult executeModuleProvidersHelper(
            const ProductContext &product,
            const CodeLocation &dependsItemLocation,
            const std::vector<Provider> &providers);
    QVariantMap getModuleProviderConfig(const ProductContext &product);

    std::optional<std::vector<QualifiedId>> getModuleProviders(Item *item);

    QString findModuleProviderFile(const QualifiedId &name, ModuleProviderLookup lookupType);
    QVariantMap evaluateQbsModule(const ProductContext &product) const;
    Item *createProviderScope(const ProductContext &product, const QVariantMap &qbsModule);
    std::pair<QStringList, std::vector<ProbeConstPtr>> evaluateModuleProvider(
            const ProductContext &product,
            const CodeLocation &location,
            const QualifiedId &name,
            const QString &providerFile,
            const QVariantMap &moduleConfig,
            const QVariantMap &qbsModule);

private:
    LoaderState &m_loaderState;
    StoredModuleProviderInfo m_storedModuleProviderInfo;
    Set<QString> m_tempQbsFiles;
};

} // namespace qbs::Internal

#endif // MODULEPROVIDERLOADER_H
