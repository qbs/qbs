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

#include "moduleloader.h"
#include "moduleproviderinfo.h"

#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

class ModuleProviderLoader
{
public:
    using ProductContext = ModuleLoader::ProductContext;
    using FallbackMode = ModuleLoader::FallbackMode;
    explicit ModuleProviderLoader(ItemReader *itemReader, Evaluator *evaluator);

    enum class ModuleProviderLookup { Regular, Fallback };
    struct ModuleProviderResult
    {
        ModuleProviderResult() = default;
        ModuleProviderResult(bool ran, bool added)
            : providerFound(ran), providerAddedSearchPaths(added) {}
        bool providerFound = false;
        bool providerAddedSearchPaths = false;
    };

    const ModuleProviderInfoList &moduleProviderInfo() const { return m_moduleProviderInfo; }
    void setModuleProviderInfo(ModuleProviderInfoList moduleProviderInfo)
    {
        m_moduleProviderInfo = std::move(moduleProviderInfo);
    }

    void setProjectParameters(SetupProjectParameters parameters)
    {
        m_parameters = std::move(parameters);
    }

    void setupKnownModuleProviders(ProductContext &product);
    ModuleProviderResult executeModuleProvider(
            ProductContext &productContext,
            const CodeLocation &dependsItemLocation,
            const QualifiedId &moduleName,
            FallbackMode fallbackMode);
    ModuleProviderResult findModuleProvider(
            const QualifiedId &name,
            ProductContext &product,
            ModuleProviderLookup lookupType,
            const CodeLocation &dependsItemLocation);
    QVariantMap moduleProviderConfig(ProductContext &product);

    const Set<QString> &tempQbsFiles() const { return m_tempQbsFiles; }

private:
    ItemReader *const m_reader{nullptr};
    Evaluator *const m_evaluator{nullptr};

    SetupProjectParameters m_parameters;
    ModuleProviderInfoList m_moduleProviderInfo;
    Set<QString> m_tempQbsFiles;
};

} // namespace Internal
} // namespace qbs

#endif // MODULEPROVIDERLOADER_H
