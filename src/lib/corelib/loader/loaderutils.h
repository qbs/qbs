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

#include <language/filetags.h>
#include <language/forward_decls.h>
#include <language/qualifiedid.h>
#include <tools/pimpl.h>
#include <tools/set.h>
#include <tools/version.h>

#include <QStringList>
#include <QVariant>

#include <vector>

namespace qbs {
class SetupProjectParameters;
namespace Internal {
class DependenciesResolver;
class Evaluator;
class Item;
class ItemPool;
class ItemReader;
class LocalProfiles;
class Logger;
class ModuleInstantiator;
class ModulePropertyMerger;
class ProbesResolver;
class ProductContext;
class ProductItemMultiplexer;
class ProgressObserver;
class ProjectContext;

using ModulePropertiesPerGroup = std::unordered_map<const Item *, QualifiedIdSet>;
using ShadowProductInfo = std::pair<bool, QString>;

enum class FallbackMode { Enabled, Disabled };
enum class Deferral { Allowed, NotAllowed };

class ProductInfo
{
public:
    std::vector<ProbeConstPtr> probes;
    ModulePropertiesPerGroup modulePropertiesSetInGroups;
    ErrorInfo delayedError;
};

class ProductContext
{
public:
    QString uniqueName() const;
    QString displayName() const;
    void handleError(const ErrorInfo &error);

    QString name;
    Item *item = nullptr;
    Item *scope = nullptr;
    ProjectContext *project = nullptr;
    Item *mergedExportItem = nullptr;
    ProductInfo info;
    QString profileName;
    QString multiplexConfigurationId;
    QVariantMap profileModuleProperties; // Tree-ified module properties from profile.
    QVariantMap moduleProperties;        // Tree-ified module properties from profile + overridden values.
    QVariantMap defaultParameters; // In Export item.
    QStringList searchPaths;

    bool dependenciesResolved = false;
};

class TopLevelProjectContext
{
public:
    TopLevelProjectContext() = default;
    TopLevelProjectContext(const TopLevelProjectContext &) = delete;
    TopLevelProjectContext &operator=(const TopLevelProjectContext &) = delete;
    ~TopLevelProjectContext() { qDeleteAll(projects); }

    bool checkItemCondition(Item *item, Evaluator &evaluator);
    void checkCancelation(const SetupProjectParameters &parameters);

    std::vector<ProjectContext *> projects;
    std::list<std::pair<ProductContext *, int>> productsToHandle;
    std::multimap<QString, ProductContext *> productsByName;
    std::unordered_map<Item *, ProductInfo> productInfos;
    Set<QString> projectNamesUsedInOverrides;
    Set<QString> productNamesUsedInOverrides;
    Set<Item *> disabledItems;
    Set<QString> erroneousProducts;
    std::vector<ProbeConstPtr> probes;
    QString buildDirectory;
    QVariantMap profileConfigs;
    ProgressObserver *progressObserver = nullptr;

    // For fast look-up when resolving Depends.productTypes.
    // The contract is that it contains fully handled, error-free, enabled products.
    std::multimap<FileTag, ProductContext *> productsByType;
};

class ProjectContext
{
public:
    QString name;
    Item *item = nullptr;
    Item *scope = nullptr;
    TopLevelProjectContext *topLevelProject = nullptr;
    std::vector<ProductContext> products;
    std::vector<QStringList> searchPathsStack;
};

class LoaderState
{
public:
    LoaderState(const SetupProjectParameters &parameters, ItemPool &itemPool, Evaluator &evaluator,
                Logger &logger);
    ~LoaderState();

    DependenciesResolver &dependenciesResolver();
    Evaluator &evaluator();
    ItemPool &itemPool();
    ItemReader &itemReader();
    LocalProfiles &localProfiles();
    Logger &logger();
    ModuleInstantiator &moduleInstantiator();
    ProductItemMultiplexer &multiplexer();
    const SetupProjectParameters &parameters() const;
    ProbesResolver &probesResolver();
    ModulePropertyMerger &propertyMerger();
    TopLevelProjectContext &topLevelProject();

private:
    class Private;
    Pimpl<Private> d;
};

void mergeParameters(QVariantMap &dst, const QVariantMap &src);
ShadowProductInfo getShadowProductInfo(const ProductContext &product);
void adjustParametersScopes(Item *item, Item *scope);

} // namespace Internal
} // namespace qbs
