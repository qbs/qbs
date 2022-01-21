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

#ifndef QBS_MODULELOADER_H
#define QBS_MODULELOADER_H

#include "filetags.h"
#include "forward_decls.h"
#include "item.h"
#include "itempool.h"
#include "moduleproviderinfo.h"
#include <logging/logger.h>
#include <tools/filetime.h>
#include <tools/qttools.h>
#include <tools/set.h>
#include <tools/setupprojectparameters.h>
#include <tools/version.h>

#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qbs {

class CodeLocation;
class Settings;

namespace Internal {

class Evaluator;
class Item;
class ItemReader;
class ModuleProviderLoader;
class ProbesResolver;
class ProgressObserver;
class QualifiedId;
class SearchPathsManager;

using ModulePropertiesPerGroup = std::unordered_map<const Item *, QualifiedIdSet>;

struct ModuleLoaderResult
{
    ModuleLoaderResult()
        : itemPool(new ItemPool), root(nullptr)
    {}

    struct ProductInfo
    {
        struct Dependency
        {
            QString name;
            QString profile; // "*" <=> Match all profiles.
            QString multiplexConfigurationId;
            QVariantMap parameters;
            bool limitToSubProject = false;
            bool isRequired = true;

            QString uniqueName() const;
        };

        std::vector<ProbeConstPtr> probes;
        std::vector<Dependency> usedProducts;
        ModulePropertiesPerGroup modulePropertiesSetInGroups;
        ErrorInfo delayedError;
    };

    std::shared_ptr<ItemPool> itemPool;
    Item *root;
    std::unordered_map<Item *, ProductInfo> productInfos;
    std::vector<ProbeConstPtr> projectProbes;
    StoredModuleProviderInfo storedModuleProviderInfo;
    Set<QString> qbsFiles;
    QVariantMap profileConfigs;
};

/*
 * Loader stage II. Responsible for
 *      - loading modules and module dependencies,
 *      - project references,
 *      - Probe items.
 */
class ModuleLoader
{
public:
    ModuleLoader(Evaluator *evaluator, Logger &logger);
    ~ModuleLoader();

    void setProgressObserver(ProgressObserver *progressObserver);
    void setSearchPaths(const QStringList &searchPaths);
    void setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes);
    void setOldProductProbes(const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes);
    void setLastResolveTime(const FileTime &time) { m_lastResolveTime = time; }
    void setStoredProfiles(const QVariantMap &profiles);
    void setStoredModuleProviderInfo(const StoredModuleProviderInfo &moduleProviderInfo);
    Evaluator *evaluator() const { return m_evaluator; }

    ModuleLoaderResult load(const SetupProjectParameters &parameters);

private:
    friend class ModuleProviderLoader;
    friend class ProbesResolver;
    class ProductSortByDependencies;

    class ContextBase
    {
    public:
        ContextBase()
            : item(nullptr), scope(nullptr)
        {}

        Item *item;
        Item *scope;
        QString name;
    };

    class ProjectContext;

    using ProductDependencies = std::vector<ModuleLoaderResult::ProductInfo::Dependency>;

    // This is the data we need to store at the point where a dependency is deferred
    // in order to properly resolve the dependency in pass 2.
    struct DeferredDependsContext {
        DeferredDependsContext(Item *exportingProduct, Item *parent)
            : exportingProductItem(exportingProduct), parentItem(parent) {}
        Item *exportingProductItem = nullptr;
        Item *parentItem = nullptr;
        bool operator==(const DeferredDependsContext &other) const
        {
            return exportingProductItem == other.exportingProductItem
                    && parentItem == other.parentItem;
        }
        bool operator<(const DeferredDependsContext &other) const
        {
            return parentItem < other.parentItem;
        }
    };

    class ProductContext : public ContextBase
    {
    public:
        ProjectContext *project = nullptr;
        ModuleLoaderResult::ProductInfo info;
        QString profileName;
        QString multiplexConfigurationId;
        QVariantMap moduleProperties;
        std::map<QString, ProductDependencies> productModuleDependencies;
        std::unordered_map<const Item *, std::vector<ErrorInfo>> unknownProfilePropertyErrors;
        QStringList searchPaths;

        std::optional<QVariantMap> theModuleProviderConfig;

        // The key corresponds to DeferredDependsContext.exportingProductItem, which is the
        // only value from that data structure that we still need here.
        std::unordered_map<Item *, std::vector<Item *>> deferredDependsItems;

        QString uniqueName() const;
    };

    class TopLevelProjectContext;

    class ProjectContext : public ContextBase
    {
    public:
        TopLevelProjectContext *topLevelProject = nullptr;
        ModuleLoaderResult *result = nullptr;
        std::vector<ProductContext> products;
        std::vector<QStringList> searchPathsStack;
    };

    struct ProductModuleInfo
    {
        Item *exportItem = nullptr;
        QString multiplexId;
        QVariantMap defaultParameters;
    };

    class TopLevelProjectContext
    {
        Q_DISABLE_COPY(TopLevelProjectContext)
    public:
        TopLevelProjectContext() = default;
        ~TopLevelProjectContext() { qDeleteAll(projects); }

        std::vector<ProjectContext *> projects;
        QMultiHash<QString, ProductModuleInfo> productModules;
        std::vector<ProbeConstPtr> probes;
        QString buildDirectory;
    };

    class DependsContext
    {
    public:
        ProductContext *product = nullptr;
        Item *exportingProductItem = nullptr;
        ProductDependencies *productDependencies = nullptr;
    };

    void handleTopLevelProject(ModuleLoaderResult *loadResult, Item *projectItem,
            const QString &buildDirectory, const Set<QString> &referencedFilePaths);
    void handleProject(ModuleLoaderResult *loadResult,
            TopLevelProjectContext *topLevelProjectContext, Item *projectItem,
            const Set<QString> &referencedFilePaths);

    using MultiplexRow = std::vector<VariantValuePtr>;
    using MultiplexTable = std::vector<MultiplexRow>;

    struct MultiplexInfo
    {
        std::vector<QString> properties;
        MultiplexTable table;
        bool aggregate = false;
        VariantValuePtr multiplexedType;

        QString toIdString(size_t row) const;
        static QVariantMap multiplexIdToVariantMap(const QString &multiplexId);
    };

    void dump(const MultiplexInfo &mpi);
    static MultiplexTable combine(const MultiplexTable &table, const MultiplexRow &values);
    MultiplexInfo extractMultiplexInfo(Item *productItem, Item *qbsModuleItem);
    QList<Item *> multiplexProductItem(ProductContext *dummyContext, Item *productItem);
    void normalizeDependencies(ProductContext *product,
                               const DeferredDependsContext &dependsContext);
    void adjustDependenciesForMultiplexing(const TopLevelProjectContext &tlp);
    void adjustDependenciesForMultiplexing(const ProductContext &product);
    void adjustDependenciesForMultiplexing(const ProductContext &product, Item *dependsItem);

    void prepareProduct(ProjectContext *projectContext, Item *productItem);
    void setupProductDependencies(ProductContext *productContext,
                                  const Set<DeferredDependsContext> &deferredDependsContext);
    void handleProduct(ProductContext *productContext);
    void checkDependencyParameterDeclarations(const ProductContext *productContext) const;
    void handleModuleSetupError(ProductContext *productContext, const Item::Module &module,
                                const ErrorInfo &error);
    void initProductProperties(const ProductContext &product);
    void handleSubProject(ProjectContext *projectContext, Item *projectItem,
            const Set<QString> &referencedFilePaths);
    QList<Item *> loadReferencedFile(const QString &relativePath,
                                     const CodeLocation &referencingLocation,
                                     const Set<QString> &referencedFilePaths,
                                     ProductContext &dummyContext);
    void handleAllPropertyOptionsItems(Item *item);
    void handlePropertyOptions(Item *optionsItem);

    using ModuleDependencies = QHash<QualifiedId, QualifiedIdSet>;
    void setupReverseModuleDependencies(const Item::Module &module, ModuleDependencies &deps,
                                        QualifiedIdSet &seenModules);
    ModuleDependencies setupReverseModuleDependencies(const Item *product);
    void handleGroup(ProductContext *productContext, Item *groupItem,
                     const ModuleDependencies &reverseDepencencies);
    void propagateModulesFromParent(ProductContext *productContext, Item *groupItem,
                                    const ModuleDependencies &reverseDepencencies);
    void adjustDefiningItemsInGroupModuleInstances(const Item::Module &module,
                                                   const Item::Modules &dependentModules);

    bool mergeExportItems(const ProductContext &productContext);
    void resolveDependencies(DependsContext *dependsContext, Item *item,
                             ProductContext *productContext = nullptr);
    class ItemModuleList;
    void resolveDependsItem(DependsContext *dependsContext, Item *parentItem, Item *dependsItem,
                            ItemModuleList *moduleResults, ProductDependencies *productResults);
    void forwardParameterDeclarations(const Item *dependsItem, const ItemModuleList &modules);
    void forwardParameterDeclarations(const QualifiedId &moduleName, Item *item,
                                      const ItemModuleList &modules);
    void resolveParameterDeclarations(const Item *module);
    QVariantMap extractParameters(Item *dependsItem) const;
    Item *moduleInstanceItem(Item *containerItem, const QualifiedId &moduleName);
    static ProductModuleInfo *productModule(ProductContext *productContext, const QString &name,
                                            const QString &multiplexId, bool &productNameMatch);
    static ProductContext *product(ProjectContext *projectContext, const QString &name);
    static ProductContext *product(TopLevelProjectContext *tlpContext, const QString &name);

    enum class FallbackMode { Enabled, Disabled };
    Item *loadModule(ProductContext *productContext, Item *exportingProductItem, Item *item,
            const CodeLocation &dependsItemLocation, const QString &moduleId,
            const QualifiedId &moduleName, const QString &multiplexId, FallbackMode fallbackMode,
            bool isRequired, bool *isProductDependency, QVariantMap *defaultParameters);
    Item *searchAndLoadModuleFile(ProductContext *productContext,
            const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
            FallbackMode fallbackMode, bool isRequired, Item *moduleInstance);
    QStringList &getModuleFileNames(const QString &dirPath);
    std::pair<Item *, bool> loadModuleFile(
            ProductContext *productContext, const QString &fullModuleName,
            const QString &filePath, Item *moduleInstance);
    std::pair<Item *, bool> getModulePrototype(ProductContext *productContext,
            const QString &fullModuleName, const QString &filePath);
    Item::Module loadBaseModule(ProductContext *productContext, Item *item);
    void setupBaseModulePrototype(Item *prototype);
    template <typename T, typename F>
    T callWithTemporaryBaseModule(ProductContext *productContext, const F &func);
    void instantiateModule(ProductContext *productContext, Item *exportingProductItem,
            Item *instanceScope, Item *moduleInstance, Item *modulePrototype,
            const QualifiedId &moduleName, ProductModuleInfo *productModuleInfo);
    void createChildInstances(Item *instance, Item *prototype,
                              QHash<Item *, Item *> *prototypeInstanceMap) const;
    void checkCancelation() const;
    bool checkItemCondition(Item *item, Item *itemToDisable = nullptr);
    QStringList readExtraSearchPaths(Item *item, bool *wasSet = nullptr);
    void copyProperties(const Item *sourceProject, Item *targetProject);
    Item *wrapInProjectIfNecessary(Item *item);
    QString findExistingModulePath(const QString &searchPath, const QualifiedId &moduleName);
    QStringList findExistingModulePaths(
            const QStringList &searchPaths, const QualifiedId &moduleName);

    static void setScopeForDescendants(Item *item, Item *scope);
    void overrideItemProperties(Item *item, const QString &buildConfigKey,
                                const QVariantMap &buildConfig);
    void addProductModuleDependencies(ProductContext *ctx, const QString &name);
    void addProductModuleDependencies(ProductContext *ctx);
    void addTransitiveDependencies(ProductContext *ctx);
    Item *createNonPresentModule(const QString &name, const QString &reason, Item *module);
    void copyGroupsFromModuleToProduct(const ProductContext &productContext,
                                       const Item::Module &module, const Item *modulePrototype);
    void copyGroupsFromModulesToProduct(const ProductContext &productContext);
    void markModuleTargetGroups(Item *group, const Item::Module &module);
    bool checkExportItemCondition(Item *exportItem, const ProductContext &productContext);

    void printProfilingInfo();
    void handleProductError(const ErrorInfo &error, ProductContext *productContext);
    QualifiedIdSet gatherModulePropertiesSetInGroup(const Item *group);
    Item *loadItemFromFile(const QString &filePath, const CodeLocation &referencingLocation);
    void collectProductsByName(const TopLevelProjectContext &topLevelProject);
    void collectProductsByType(const TopLevelProjectContext &topLevelProject);

    void handleProfileItems(Item *item, ProjectContext *projectContext);
    std::vector<Item *> collectProfileItems(Item *item, ProjectContext *projectContext);
    void evaluateProfileValues(const QualifiedId &namePrefix, Item *item, Item *profileItem,
                               QVariantMap &values);
    void handleProfile(Item *profileItem);
    void collectNameFromOverride(const QString &overrideString);
    void checkProjectNamesInOverrides(const TopLevelProjectContext &tlp);
    void checkProductNamesInOverrides();
    void setSearchPathsForProduct(ProductContext *product);

    Item::Modules modulesSortedByDependency(const Item *productItem);
    void createSortedModuleList(const Item::Module &parentModule, Item::Modules &modules);
    void collectAllModules(Item *item, std::vector<Item::Module> *modules);
    std::vector<Item::Module> allModules(Item *item);
    bool moduleRepresentsDisabledProduct(const Item::Module &module);

    using ShadowProductInfo = std::pair<bool, QString>;
    ShadowProductInfo getShadowProductInfo(const ProductContext &product) const;

    ItemPool *m_pool;
    Logger &m_logger;
    ProgressObserver *m_progressObserver;
    const std::unique_ptr<ItemReader> m_reader;
    Evaluator *m_evaluator;
    const std::unique_ptr<ProbesResolver> m_probesResolver;
    const std::unique_ptr<ModuleProviderLoader> m_moduleProviderLoader;
    QMap<QString, QStringList> m_moduleDirListCache;
    QHash<std::pair<QString, QualifiedId>, std::optional<QString>> m_existingModulePathCache;

    // The keys are file paths, the values are module prototype items accompanied by a profile.
    std::unordered_map<QString, std::vector<std::pair<Item *, QString>>> m_modulePrototypes;

    // The keys are module prototypes and products, the values specify whether the module's
    // condition is true for that product.
    QHash<std::pair<Item *, ProductContext *>, bool> m_modulePrototypeEnabledInfo;

    QHash<const Item *, Item::PropertyDeclarationMap> m_parameterDeclarations;
    Set<Item *> m_disabledItems;
    std::vector<bool> m_requiredChain;

    struct DependsChainEntry
    {
        DependsChainEntry(QualifiedId name, const CodeLocation &location)
            : name(std::move(name)), location(location)
        {
        }

        QualifiedId name;
        CodeLocation location;
        bool isProduct = false;
    };
    class DependsChainManager;
    std::vector<DependsChainEntry> m_dependsChain;

    FileTime m_lastResolveTime;
    QVariantMap m_storedProfiles;
    QVariantMap m_localProfiles;
    std::multimap<QString, const ProductContext *> m_productsByName;
    std::multimap<FileTag, const ProductContext *> m_productsByType;

    std::unordered_map<ProductContext *, Set<DeferredDependsContext>> m_productsWithDeferredDependsItems;
    Set<Item *> m_exportsWithDeferredDependsItems;

    SetupProjectParameters m_parameters;
    std::unique_ptr<Settings> m_settings;
    Version m_qbsVersion;
    Item *m_tempScopeItem = nullptr;

    qint64 m_elapsedTimeProbes = 0;
    qint64 m_elapsedTimePrepareProducts = 0;
    qint64 m_elapsedTimeProductDependencies = 0;
    qint64 m_elapsedTimeModuleProviders = 0;
    qint64 m_elapsedTimeTransitiveDependencies = 0;
    qint64 m_elapsedTimeHandleProducts = 0;
    qint64 m_elapsedTimePropertyChecking = 0;
    Set<QString> m_projectNamesUsedInOverrides;
    Set<QString> m_productNamesUsedInOverrides;
    Set<QString> m_disabledProjects;
    Set<QString> m_erroneousProducts;

    int m_dependencyResolvingPass = 0;
};

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo::Dependency, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_MODULELOADER_H
