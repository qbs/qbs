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
#include <logging/logger.h>
#include <tools/setupprojectparameters.h>
#include <tools/version.h>

#include <QMap>
#include <QSet>
#include <QStack>
#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QScriptContext;
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {

class CodeLocation;

namespace Internal {

class Evaluator;
class Item;
class ItemReader;
class ProgressObserver;
class QualifiedId;
class ScriptEngine;

struct ModuleLoaderResult
{
    ModuleLoaderResult()
        : itemPool(new ItemPool), root(0)
    {}

    struct ProductInfo
    {
        struct Dependency
        {
            FileTags productTypes;
            QString name;
            QString profile; // "*" <=> Match all profiles.
            bool limitToSubProject = false;
            bool isRequired = true;

            QString uniqueName() const;
        };

        QList<ProbeConstPtr> probes;
        QList<Dependency> usedProducts;
        bool hasError = false;
    };

    QSharedPointer<ItemPool> itemPool;
    Item *root;
    QHash<Item *, ProductInfo> productInfos;
    QSet<QString> qbsFiles;
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
    ModuleLoader(ScriptEngine *engine, Logger &logger);
    ~ModuleLoader();

    void setProgressObserver(ProgressObserver *progressObserver);
    void setSearchPaths(const QStringList &searchPaths);
    void setOldProbes(const QHash<QString, QList<ProbeConstPtr>> &oldProbes);
    Evaluator *evaluator() const { return m_evaluator; }

    ModuleLoaderResult load(const SetupProjectParameters &parameters);

private:
    class ProductSortByDependencies;
    struct ItemCacheValue {
        explicit ItemCacheValue(Item *module = 0, bool enabled = false)
            : module(module), enabled(enabled) {}
        Item *module;
        bool enabled;
    };

    typedef QMap<QPair<QString, QString>, ItemCacheValue> ModuleItemCache;

    class ContextBase
    {
    public:
        ContextBase()
            : item(0), scope(0)
        {}

        Item *item;
        Item *scope;
    };

    class ProjectContext;

    class ProductContext : public ContextBase
    {
    public:
        ProjectContext *project;
        ModuleLoaderResult::ProductInfo info;
        QString name;
        QString profileName;
        QVariantMap moduleProperties;
    };

    class TopLevelProjectContext;

    class ProjectContext : public ContextBase
    {
    public:
        TopLevelProjectContext *topLevelProject;
        ModuleLoaderResult *result;
        QVector<ProductContext> products;
        QStack<QStringList> searchPathsStack;
    };

    struct ProductModuleInfo
    {
        ProductModuleInfo() : exportItem() {}

        Item *exportItem;
        QList<ModuleLoaderResult::ProductInfo::Dependency> productDependencies;
    };

    class TopLevelProjectContext
    {
        Q_DISABLE_COPY(TopLevelProjectContext)
    public:
        TopLevelProjectContext() {}
        ~TopLevelProjectContext() { qDeleteAll(projects); }

        QVector<ProjectContext *> projects;
        QHash<QString, ProductModuleInfo> productModules;
        QString buildDirectory;
    };

    class DependsContext
    {
    public:
        ProductContext *product;
        QList<ModuleLoaderResult::ProductInfo::Dependency> *productDependencies;
    };

    typedef QList<ModuleLoaderResult::ProductInfo::Dependency> ProductDependencyResults;

    void handleTopLevelProject(ModuleLoaderResult *loadResult, Item *projectItem,
            const QString &buildDirectory, const QSet<QString> &referencedFilePaths);
    void handleProject(ModuleLoaderResult *loadResult,
            TopLevelProjectContext *topLevelProjectContext, Item *projectItem,
            const QSet<QString> &referencedFilePaths);
    QList<Item *> multiplexProductItem(ProductContext *dummyContext, Item *productItem);
    void prepareProduct(ProjectContext *projectContext, Item *productItem);
    void setupProductDependencies(ProductContext *productContext);
    void handleProduct(ProductContext *productContext);
    void handleModuleSetupError(ProductContext *productContext, const Item::Module &module,
                                const ErrorInfo &error);
    void initProductProperties(const ProductContext &product);
    void handleSubProject(ProjectContext *projectContext, Item *projectItem,
            const QSet<QString> &referencedFilePaths);
    QList<Item *> loadReferencedFile(const QString &relativePath,
                                     const CodeLocation &referencingLocation,
                                     const QSet<QString> &referencedFilePaths,
                                     ProductContext &dummyContext);
    void handleAllPropertyOptionsItems(Item *item);
    void handlePropertyOptions(Item *optionsItem);

    using ModuleDependencies = QHash<QualifiedId, QualifiedIdSet>;
    void setupReverseModuleDependencies(const Item::Module &module, ModuleDependencies &deps,
                                        QualifiedIdSet &seenModules);
    ModuleDependencies setupReverseModuleDependencies(const Item *product);
    void handleGroup(Item *groupItem, const ModuleDependencies &reverseDepencencies);
    void propagateModulesFromParent(Item *groupItem, const ModuleDependencies &reverseDepencencies);
    void adjustDefiningItemsInGroupModuleInstances(const Item::Module &module,
                                                   const Item::Modules &dependentModules);

    void mergeExportItems(const ProductContext &productContext);
    void resolveDependencies(DependsContext *dependsContext, Item *item);
    class ItemModuleList;
    void resolveDependsItem(DependsContext *dependsContext, Item *parentItem, Item *dependsItem,
                            ItemModuleList *moduleResults, ProductDependencyResults *productResults);
    Item *moduleInstanceItem(Item *containerItem, const QualifiedId &moduleName);
    Item *loadProductModule(ProductContext *productContext, const QString &moduleName);
    Item *loadModule(ProductContext *productContext, Item *item,
            const CodeLocation &dependsItemLocation, const QString &moduleId,
            const QualifiedId &moduleName, bool isRequired, bool *isModuleDependency);
    Item *searchAndLoadModuleFile(ProductContext *productContext,
            const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
            const QStringList &extraSearchPaths, bool isRequired, bool *cacheHit);
    Item *loadModuleFile(ProductContext *productContext, const QString &fullModuleName,
            bool isBaseModule, const QString &filePath, bool *cacheHit, bool *triedToLoad);
    Item::Module loadBaseModule(ProductContext *productContext, Item *item);
    void setupBaseModulePrototype(Item *prototype);
    void instantiateModule(ProductContext *productContext, Item *exportingProductItem,
            Item *instanceScope, Item *moduleInstance, Item *modulePrototype,
            const QualifiedId &moduleName, bool isProduct);
    void createChildInstances(ProductContext *productContext, Item *instance,
                              Item *prototype, QHash<Item *, Item *> *prototypeInstanceMap) const;
    void resolveProbes(ProductContext *productContext, Item *item);
    void resolveProbe(ProductContext *productContext, Item *parent, Item *probe);
    void checkCancelation() const;
    bool checkItemCondition(Item *item);
    QStringList readExtraSearchPaths(Item *item, bool *wasSet = 0);
    void copyProperties(const Item *sourceProject, Item *targetProject);
    Item *wrapInProjectIfNecessary(Item *item);
    static QString findExistingModulePath(const QString &searchPath,
            const QualifiedId &moduleName);
    static void setScopeForDescendants(Item *item, Item *scope);
    void overrideItemProperties(Item *item, const QString &buildConfigKey,
                                const QVariantMap &buildConfig);
    void addTransitiveDependencies(ProductContext *ctx);
    Item *createNonPresentModule(const QString &name, const QString &reason, Item *module);
    void copyGroupsFromModuleToProduct(const ProductContext &productContext,
                                       const Item *modulePrototype);
    void copyGroupsFromModulesToProduct(const ProductContext &productContext);
    bool checkExportItemCondition(Item *exportItem, const ProductContext &productContext);
    ProbeConstPtr findOldProbe(const QString &product, bool condition,
                               const QVariantMap &initialProperties,
                               const QString &sourceCode) const;
    ProbeConstPtr findCurrentProbe(const CodeLocation &location, bool condition,
                                   const QVariantMap &initialProperties) const;
    void printProfilingInfo();

    void handleProductError(const ErrorInfo &error, ProductContext *productContext);

    bool isSomeModulePropertySet(const Item *item);

    ScriptEngine *m_engine;
    ItemPool *m_pool;
    Logger &m_logger;
    ProgressObserver *m_progressObserver;
    ItemReader *m_reader;
    Evaluator *m_evaluator;
    QStringList m_moduleSearchPaths;
    QMap<QString, QStringList> m_moduleDirListCache;
    QHash<QString, Item *> m_productModuleCache;
    ModuleItemCache m_modulePrototypeItemCache;
    QHash<Item *, QSet<QString> > m_validItemPropertyNamesPerItem;
    QSet<Item *> m_disabledItems;
    QStack<bool> m_requiredChain;

    using DependsChainEntry = QPair<QualifiedId, CodeLocation>;
    class DependsChainManager;
    QStack<DependsChainEntry> m_dependsChain;

    QHash<QString, QList<ProbeConstPtr>> m_oldProbes;
    QHash<CodeLocation, QList<ProbeConstPtr>> m_currentProbes;
    SetupProjectParameters m_parameters;
    Version m_qbsVersion;
    Item *m_tempScopeItem = nullptr;
    qint64 m_elapsedTimeProbes;
};

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo::Dependency, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_MODULELOADER_H
