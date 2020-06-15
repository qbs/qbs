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

#include "moduleloader.h"

#include "builtindeclarations.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "itemreader.h"
#include "language.h"
#include "modulemerger.h"
#include "qualifiedid.h"
#include "scriptengine.h"
#include "value.h"

#include <api/languageinfo.h>
#include <language/language.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/preferences.h>
#include <tools/profile.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/scripttools.h>
#include <tools/settings.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qglobalstatic.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qthreadstorage.h>
#include <QtScript/qscriptvalueiterator.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace qbs {
namespace Internal {

using MultiplexConfigurationByIdTable = QThreadStorage<QHash<QString, QVariantMap> >;
Q_GLOBAL_STATIC(MultiplexConfigurationByIdTable, multiplexConfigurationsById);

static void handlePropertyError(const ErrorInfo &error, const SetupProjectParameters &params,
                                Logger &logger)
{
    if (params.propertyCheckingMode() == ErrorHandlingMode::Strict)
        throw error;
    logger.printWarning(error);
}

static bool multiplexConfigurationIntersects(const QVariantMap &lhs, const QVariantMap &rhs)
{
    QBS_CHECK(!lhs.isEmpty() && !rhs.isEmpty());

    for (auto lhsProperty = lhs.constBegin(); lhsProperty != lhs.constEnd(); lhsProperty++) {
        const auto rhsProperty = rhs.find(lhsProperty.key());
        const bool isCommonProperty = rhsProperty != rhs.constEnd();
        if (isCommonProperty && lhsProperty.value() != rhsProperty.value())
            return false;
    }

    return true;
}

class ModuleLoader::ItemModuleList : public QList<Item::Module> {};

static QString probeGlobalId(Item *probe)
{
    QString id;

    for (Item *obj = probe; obj; obj = obj->prototype()) {
        if (!obj->id().isEmpty()) {
            id = obj->id();
            break;
        }
    }

    if (id.isEmpty())
        return {};

    QBS_CHECK(probe->file());
    return id + QLatin1Char('_') + probe->file()->filePath();
}

class ModuleLoader::ProductSortByDependencies
{
public:
    ProductSortByDependencies(TopLevelProjectContext &tlp) : m_tlp(tlp)
    {
    }

    void apply()
    {
        QHash<QString, std::vector<ProductContext *>> productsMap;
        QList<ProductContext *> allProducts;
        for (ProjectContext * const projectContext : qAsConst(m_tlp.projects)) {
            for (auto &product : projectContext->products) {
                allProducts.push_back(&product);
                productsMap[product.name].push_back(&product);
            }
        }
        Set<ProductContext *> allDependencies;
        for (auto productContext : qAsConst(allProducts)) {
            auto &productDependencies = m_dependencyMap[productContext];
            for (const auto &dep : qAsConst(productContext->info.usedProducts)) {
                QBS_CHECK(!dep.name.isEmpty());
                const auto &deps = productsMap.value(dep.name);
                if (dep.profile == StringConstants::star()) {
                    QBS_CHECK(!deps.empty());
                    for (ProductContext *depProduct : deps) {
                        if (depProduct == productContext)
                            continue;
                        productDependencies.push_back(depProduct);
                        allDependencies << depProduct;
                    }
                } else {
                    auto it = std::find_if(deps.begin(), deps.end(), [&dep] (ProductContext *p) {
                            return p->multiplexConfigurationId == dep.multiplexConfigurationId;
                        });
                    if (it == deps.end()) {
                        QBS_CHECK(!productContext->multiplexConfigurationId.isEmpty());
                        const QString productName = ResolvedProduct::fullDisplayName(
                                    productContext->name, productContext->multiplexConfigurationId);
                        const QString depName = ResolvedProduct::fullDisplayName(
                                    dep.name, dep.multiplexConfigurationId);
                        throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' not "
                                               "fulfilled.").arg(productName, depName),
                                        productContext->item->location());
                    }
                    productDependencies.push_back(*it);
                    allDependencies << *it;
                }
            }
        }
        const Set<ProductContext *> rootProducts
                = Set<ProductContext *>::fromList(allProducts) - allDependencies;
        for (ProductContext * const rootProduct : rootProducts)
            traverse(rootProduct);
        if (m_sortedProducts.size() < allProducts.size()) {
            for (auto const product : qAsConst(allProducts)) {
                QList<ModuleLoader::ProductContext *> path;
                findCycle(product, path);
            }
        }
        QBS_CHECK(m_sortedProducts.size() == allProducts.size());
    }

    // No product at position i has dependencies to a product at position j > i.
    const QList<ProductContext *> &sortedProducts() const
    {
        return m_sortedProducts;
    }

private:
    void traverse(ModuleLoader::ProductContext *product)
    {
        if (!m_seenProducts.insert(product).second)
            return;
        for (const auto &dependency : m_dependencyMap.value(product))
            traverse(dependency);
        m_sortedProducts << product;
    }

    void findCycle(ModuleLoader::ProductContext *product,
                   QList<ModuleLoader::ProductContext *> &path)
    {
        if (path.contains(product)) {
            ErrorInfo error(Tr::tr("Cyclic dependencies detected."));
            for (const auto * const p : path)
                error.append(p->name, p->item->location());
            error.append(product->name, product->item->location());
            throw error;
        }
        path << product;
        for (auto const dep : m_dependencyMap.value(product))
            findCycle(dep, path);
        path.removeLast();
    }

    TopLevelProjectContext &m_tlp;
    QHash<ProductContext *, std::vector<ProductContext *>> m_dependencyMap;
    Set<ProductContext *> m_seenProducts;
    QList<ProductContext *> m_sortedProducts;
};

class SearchPathsManager {
public:
    SearchPathsManager(ItemReader *itemReader, const QStringList &extraSearchPaths)
        : m_itemReader(itemReader)
    {
        m_itemReader->pushExtraSearchPaths(extraSearchPaths);
    }
    ~SearchPathsManager() { m_itemReader->popExtraSearchPaths(); }

private:
    ItemReader * const m_itemReader;
};

ModuleLoader::ModuleLoader(Evaluator *evaluator, Logger &logger)
    : m_pool(nullptr)
    , m_logger(logger)
    , m_progressObserver(nullptr)
    , m_reader(new ItemReader(logger))
    , m_evaluator(evaluator)
{
}

ModuleLoader::~ModuleLoader()
{
    delete m_reader;
}

void ModuleLoader::setProgressObserver(ProgressObserver *progressObserver)
{
    m_progressObserver = progressObserver;
}

void ModuleLoader::setSearchPaths(const QStringList &searchPaths)
{
    m_reader->setSearchPaths(searchPaths);
    qCDebug(lcModuleLoader) << "initial search paths:" << searchPaths;
}

void ModuleLoader::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    m_oldProjectProbes.clear();
    for (const ProbeConstPtr& probe : oldProbes)
        m_oldProjectProbes[probe->globalId()] << probe;
}

void ModuleLoader::setOldProductProbes(const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    m_oldProductProbes = oldProbes;
}

void ModuleLoader::setStoredProfiles(const QVariantMap &profiles)
{
    m_storedProfiles = profiles;
}

void ModuleLoader::setStoredModuleProviderInfo(const ModuleProviderInfoList &moduleProviderInfo)
{
    m_moduleProviderInfo = moduleProviderInfo;
}

ModuleLoaderResult ModuleLoader::load(const SetupProjectParameters &parameters)
{
    TimedActivityLogger moduleLoaderTimer(m_logger, Tr::tr("ModuleLoader"),
                                          parameters.logElapsedTime());
    qCDebug(lcModuleLoader) << "load" << parameters.projectFilePath();
    m_parameters = parameters;
    m_modulePrototypes.clear();
    m_modulePrototypeEnabledInfo.clear();
    m_parameterDeclarations.clear();
    m_disabledItems.clear();
    m_reader->clearExtraSearchPathsStack();
    m_reader->setEnableTiming(parameters.logElapsedTime());
    m_elapsedTimeProbes = m_elapsedTimePrepareProducts = m_elapsedTimeHandleProducts
            = m_elapsedTimeProductDependencies = m_elapsedTimeTransitiveDependencies
            = m_elapsedTimePropertyChecking = 0;
    m_elapsedTimeProbes = 0;
    m_probesEncountered = m_probesRun = m_probesCachedCurrent = m_probesCachedOld = 0;
    m_settings = std::make_unique<Settings>(parameters.settingsDirectory());

    const auto keys = m_parameters.overriddenValues().keys();
    for (const QString &key : keys) {
        static const QStringList prefixes({ StringConstants::projectPrefix(),
                                            QStringLiteral("projects"),
                                            QStringLiteral("products"), QStringLiteral("modules"),
                                            StringConstants::moduleProviders(),
                                            StringConstants::qbsModule()});
        bool ok = false;
        for (const auto &prefix : prefixes) {
            if (key.startsWith(prefix + QLatin1Char('.'))) {
                ok = true;
                break;
            }
        }
        if (ok) {
            collectNameFromOverride(key);
            continue;
        }
        ErrorInfo e(Tr::tr("Property override key '%1' not understood.").arg(key));
        e.append(Tr::tr("Please use one of the following:"));
        e.append(QLatin1Char('\t') + Tr::tr("projects.<project-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("modules.<module-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<module-name>."
                                            "<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("moduleProviders.<provider-name>."
                                            "<property-name>:value"));
        handlePropertyError(e, m_parameters, m_logger);
    }

    ModuleLoaderResult result;
    result.profileConfigs = m_storedProfiles;
    m_pool = result.itemPool.get();
    m_reader->setPool(m_pool);

    const QStringList topLevelSearchPaths = parameters.finalBuildConfigurationTree()
            .value(StringConstants::projectPrefix()).toMap()
            .value(StringConstants::qbsSearchPathsProperty()).toStringList();
    Item *root;
    {
        SearchPathsManager searchPathsManager(m_reader, topLevelSearchPaths);
        root = loadItemFromFile(parameters.projectFilePath(), CodeLocation());
        if (!root)
            return ModuleLoaderResult();
    }

    switch (root->type()) {
    case ItemType::Product:
        root = wrapInProjectIfNecessary(root);
        break;
    case ItemType::Project:
        break;
    default:
        throw ErrorInfo(Tr::tr("The top-level item must be of type 'Project' or 'Product', but it"
                               " is of type '%1'.").arg(root->typeName()), root->location());
    }

    const QString buildDirectory = TopLevelProject::deriveBuildDirectory(parameters.buildRoot(),
            TopLevelProject::deriveId(parameters.finalBuildConfigurationTree()));
    root->setProperty(StringConstants::sourceDirectoryProperty(),
                      VariantValue::create(QFileInfo(root->file()->filePath()).absolutePath()));
    root->setProperty(StringConstants::buildDirectoryProperty(),
                      VariantValue::create(buildDirectory));
    root->setProperty(StringConstants::profileProperty(),
                      VariantValue::create(m_parameters.topLevelProfile()));
    handleTopLevelProject(&result, root, buildDirectory,
                  Set<QString>() << QDir::cleanPath(parameters.projectFilePath()));
    result.root = root;
    result.qbsFiles = m_reader->filesRead() - m_tempQbsFiles;
    for (auto it = m_localProfiles.cbegin(); it != m_localProfiles.cend(); ++it)
        result.profileConfigs.remove(it.key());
    printProfilingInfo();
    return result;
}

class PropertyDeclarationCheck : public ValueHandler
{
    const Set<Item *> &m_disabledItems;
    Set<Item *> m_handledItems;
    std::vector<Item *> m_parentItems;
    Item *m_currentModuleInstance = nullptr;
    QualifiedId m_currentModuleName;
    QString m_currentName;
    SetupProjectParameters m_params;
    Logger &m_logger;
public:
    PropertyDeclarationCheck(const Set<Item *> &disabledItems,
                             SetupProjectParameters params, Logger &logger)
        : m_disabledItems(disabledItems)
        , m_params(std::move(params))
        , m_logger(logger)
    {
    }

    void operator()(Item *item)
    {
        handleItem(item);
    }

private:
    void handle(JSSourceValue *value) override
    {
        if (!value->createdByPropertiesBlock()) {
            const ErrorInfo error(Tr::tr("Property '%1' is not declared.")
                                  .arg(m_currentName), value->location());
            handlePropertyError(error, m_params, m_logger);
        }
    }

    void handle(ItemValue *value) override
    {
        if (checkItemValue(value))
            handleItem(value->item());
    }

    bool checkItemValue(ItemValue *value)
    {
        // TODO: Remove once QBS-1030 is fixed.
        if (parentItem()->type() == ItemType::Artifact)
            return false;

        if (parentItem()->type() == ItemType::Properties)
            return false;

        if (parentItem()->isOfTypeOrhasParentOfType(ItemType::Export)) {
            // Export item prototypes do not have instantiated modules.
            // The module instances are where the Export is used.
            QBS_ASSERT(m_currentModuleInstance, return false);
            auto hasCurrentModuleName = [this](const Item::Module &m) {
                return m.name == m_currentModuleName;
            };
            if (any_of(m_currentModuleInstance->modules(), hasCurrentModuleName))
                return true;
        }

        // TODO: We really should have a dedicated item type for "pre-instantiated" item values
        //       and only use ModuleInstance for actual module instances.
        const bool itemIsModuleInstance = value->item()->type() == ItemType::ModuleInstance
                && value->item()->hasProperty(StringConstants::presentProperty());

        if (!itemIsModuleInstance
                && value->item()->type() != ItemType::ModulePrefix
                && (!parentItem()->file() || !parentItem()->file()->idScope()
                    || !parentItem()->file()->idScope()->hasProperty(m_currentName))
                && !value->createdByPropertiesBlock()) {
            CodeLocation location = value->location();
            for (int i = int(m_parentItems.size() - 1); !location.isValid() && i >= 0; --i)
                location = m_parentItems.at(i)->location();
            const ErrorInfo error(Tr::tr("Item '%1' is not declared. "
                                         "Did you forget to add a Depends item?")
                                  .arg(m_currentModuleName.toString()), location);
            handlePropertyError(error, m_params, m_logger);
            return false;
        }

        return true;
    }

    void handleItem(Item *item)
    {
        if (!m_handledItems.insert(item).second)
            return;
        if (m_disabledItems.contains(item)
                || (item->type() == ItemType::ModuleInstance && !item->isPresentModule())
                || item->type() == ItemType::Properties

                // The Properties child of a SubProject item is not a regular item.
                || item->type() == ItemType::PropertiesInSubProject) {
            return;
        }

        // If a module was found but its validate script failed, only the canonical
        // module instance will have the "non-present" flag set, so we need to locate it.
        if (item->type() == ItemType::ModuleInstance) {
            const Item *productItem = nullptr;
            for (auto it = m_parentItems.rbegin(); it != m_parentItems.rend(); ++it) {
                if ((*it)->type() == ItemType::Product) {
                    productItem = *it;
                    break;
                }
            }
            if (productItem) {
                for (const Item::Module &m : productItem->modules()) {
                    if (m.name == m_currentModuleName) {
                        if (!m.item->isPresentModule())
                            return;
                        break;
                    }
                }
            }
        }

        m_parentItems.push_back(item);
        for (Item::PropertyMap::const_iterator it = item->properties().constBegin();
                it != item->properties().constEnd(); ++it) {
            if (item->type() == ItemType::Product && it.key() == StringConstants::moduleProviders()
                    && it.value()->type() == Value::ItemValueType)
                continue;
            const PropertyDeclaration decl = item->propertyDeclaration(it.key());
            if (decl.isValid()) {
                if (!decl.isDeprecated())
                    continue;
                const DeprecationInfo &di = decl.deprecationInfo();
                QString message;
                bool warningOnly;
                if (decl.isExpired()) {
                    message = Tr::tr("The property '%1' can no longer be used. "
                                     "It was removed in Qbs %2.")
                            .arg(decl.name(), di.removalVersion().toString());
                    warningOnly = false;
                } else {
                    message = Tr::tr("The property '%1' is deprecated and will be removed "
                                     "in Qbs %2.").arg(decl.name(), di.removalVersion().toString());
                    warningOnly = true;
                }
                ErrorInfo error(message, it.value()->location());
                if (!di.additionalUserInfo().isEmpty())
                    error.append(di.additionalUserInfo());
                if (warningOnly)
                    m_logger.printWarning(error);
                else
                    handlePropertyError(error, m_params, m_logger);
                continue;
            }
            m_currentName = it.key();
            const QualifiedId oldModuleName = m_currentModuleName;
            if (parentItem()->type() != ItemType::ModulePrefix)
                m_currentModuleName.clear();
            m_currentModuleName.push_back(m_currentName);
            it.value()->apply(this);
            m_currentModuleName = oldModuleName;
        }
        m_parentItems.pop_back();
        for (Item * const child : item->children()) {
            switch (child->type()) {
            case ItemType::Export:
            case ItemType::Depends:
            case ItemType::Parameter:
            case ItemType::Parameters:
                break;
            case ItemType::Group:
                if (item->type() == ItemType::Module || item->type() == ItemType::ModuleInstance)
                    break;
                Q_FALLTHROUGH();
            default:
                handleItem(child);
            }
        }

        // Properties that don't refer to an existing module with a matching Depends item
        // only exist in the prototype of an Export item, not in the instance.
        // Example 1 - setting a property of an unknown module: Export { abc.def: true }
        // Example 2 - setting a non-existing Export property: Export { blubb: true }
        if (item->type() == ItemType::ModuleInstance && item->prototype()) {
            Item *oldInstance = m_currentModuleInstance;
            m_currentModuleInstance = item;
            handleItem(item->prototype());
            m_currentModuleInstance = oldInstance;
        }
    }

    void handle(VariantValue *) override { /* only created internally - no need to check */ }

    Item *parentItem() const { return m_parentItems.back(); }
};

void ModuleLoader::handleTopLevelProject(ModuleLoaderResult *loadResult, Item *projectItem,
        const QString &buildDirectory, const Set<QString> &referencedFilePaths)
{
    TopLevelProjectContext tlp;
    tlp.buildDirectory = buildDirectory;
    handleProject(loadResult, &tlp, projectItem, referencedFilePaths);
    checkProjectNamesInOverrides(tlp);
    collectProductsByName(tlp);
    checkProductNamesInOverrides();

    adjustDependenciesForMultiplexing(tlp);

    m_dependencyResolvingPass = 1;
    for (ProjectContext * const projectContext : qAsConst(tlp.projects)) {
        m_reader->setExtraSearchPathsStack(projectContext->searchPathsStack);
        for (ProductContext &productContext : projectContext->products) {
            try {
                setupProductDependencies(&productContext, Set<DeferredDependsContext>());
            } catch (const ErrorInfo &err) {
                if (productContext.name.isEmpty())
                    throw err;
                handleProductError(err, &productContext);
            }
            for (std::size_t i = 0; i < productContext.newlyAddedModuleProviderSearchPaths.size(); ++i)
                m_reader->popExtraSearchPaths();
            productContext.newlyAddedModuleProviderSearchPaths.clear();
        }
    }
    if (!m_productsWithDeferredDependsItems.empty() || !m_exportsWithDeferredDependsItems.empty()) {
        collectProductsByType(tlp);
        m_dependencyResolvingPass = 2;

        // Doing the normalization for the Export items themselves (as opposed to doing it only
        // for the corresponding module instances) serves two purposes:
        // (1) It makes recursive use of Depends.productTypes via Export items work; otherwise,
        //     we'd need an additional dependency resolving pass for every export level.
        // (2) The "expanded" Depends items are available to the Exporter.qbs module.
        for (Item * const exportItem : m_exportsWithDeferredDependsItems)
            normalizeDependencies(nullptr, DeferredDependsContext(nullptr, exportItem));

        for (const auto &deferredDependsData : m_productsWithDeferredDependsItems) {
            ProductContext * const productContext = deferredDependsData.first;
            m_reader->setExtraSearchPathsStack(productContext->project->searchPathsStack);
            try {
                setupProductDependencies(productContext, deferredDependsData.second);
            } catch (const ErrorInfo &err) {
                handleProductError(err, productContext);
            }
        }
    }

    ProductSortByDependencies productSorter(tlp);
    productSorter.apply();
    for (ProductContext * const p : productSorter.sortedProducts()) {
        try {
            handleProduct(p);
            if (p->name.startsWith(StringConstants::shadowProductPrefix()))
                tlp.probes << p->info.probes;
        } catch (const ErrorInfo &err) {
            handleProductError(err, p);
        }
    }

    loadResult->projectProbes = tlp.probes;
    loadResult->moduleProviderInfo = m_moduleProviderInfo;

    m_reader->clearExtraSearchPathsStack();
    AccumulatingTimer timer(m_parameters.logElapsedTime()
                            ? &m_elapsedTimePropertyChecking : nullptr);
    PropertyDeclarationCheck check(m_disabledItems, m_parameters, m_logger);
    check(projectItem);
}

void ModuleLoader::handleProject(ModuleLoaderResult *loadResult,
        TopLevelProjectContext *topLevelProjectContext, Item *projectItem,
        const Set<QString> &referencedFilePaths)
{
    QScopedPointer<ProjectContext> p(new ProjectContext);
    auto &projectContext = *p;
    projectContext.topLevelProject = topLevelProjectContext;
    projectContext.result = loadResult;
    ItemValuePtr itemValue = ItemValue::create(projectItem);
    projectContext.scope = Item::create(m_pool, ItemType::Scope);
    projectContext.scope->setFile(projectItem->file());
    projectContext.scope->setProperty(StringConstants::projectVar(), itemValue);
    ProductContext dummyProductContext;
    dummyProductContext.project = &projectContext;
    dummyProductContext.moduleProperties = m_parameters.finalBuildConfigurationTree();
    projectItem->addModule(loadBaseModule(&dummyProductContext, projectItem));
    overrideItemProperties(projectItem, StringConstants::projectPrefix(),
                           m_parameters.overriddenValuesTree());
    projectContext.name = m_evaluator->stringValue(projectItem,
                                                   StringConstants::nameProperty());
    if (projectContext.name.isEmpty()) {
        projectContext.name = FileInfo::baseName(projectItem->location().filePath());
        projectItem->setProperty(StringConstants::nameProperty(),
                                 VariantValue::create(projectContext.name));
    }
    overrideItemProperties(projectItem,
                           StringConstants::projectsOverridePrefix() + projectContext.name,
                           m_parameters.overriddenValuesTree());
    if (!checkItemCondition(projectItem)) {
        m_disabledProjects.insert(projectContext.name);
        return;
    }
    p.take();
    topLevelProjectContext->projects.push_back(&projectContext);
    m_reader->pushExtraSearchPaths(readExtraSearchPaths(projectItem)
                                   << projectItem->file()->dirPath());
    projectContext.searchPathsStack = m_reader->extraSearchPathsStack();
    projectContext.item = projectItem;

    const QString minVersionStr
            = m_evaluator->stringValue(projectItem, StringConstants::minimumQbsVersionProperty(),
                                       QStringLiteral("1.3.0"));
    const Version minVersion = Version::fromString(minVersionStr);
    if (!minVersion.isValid()) {
        throw ErrorInfo(Tr::tr("The value '%1' of Project.minimumQbsVersion "
                "is not a valid version string.").arg(minVersionStr), projectItem->location());
    }
    if (!m_qbsVersion.isValid())
        m_qbsVersion = Version::fromString(QLatin1String(QBS_VERSION));
    if (m_qbsVersion < minVersion) {
        throw ErrorInfo(Tr::tr("The project requires at least qbs version %1, but "
                               "this is qbs version %2.").arg(minVersion.toString(),
                                                              m_qbsVersion.toString()));
    }

    for (Item * const child : projectItem->children())
        child->setScope(projectContext.scope);

    resolveProbes(&dummyProductContext, projectItem);
    projectContext.topLevelProject->probes << dummyProductContext.info.probes;

    handleProfileItems(projectItem, &projectContext);

    QList<Item *> multiplexedProducts;
    for (Item * const child : projectItem->children()) {
        if (child->type() == ItemType::Product)
            multiplexedProducts << multiplexProductItem(&dummyProductContext, child);
    }
    for (Item * const additionalProductItem : qAsConst(multiplexedProducts))
        Item::addChild(projectItem, additionalProductItem);

    const QList<Item *> originalChildren = projectItem->children();
    for (Item * const child : originalChildren) {
        switch (child->type()) {
        case ItemType::Product:
            prepareProduct(&projectContext, child);
            break;
        case ItemType::SubProject:
            handleSubProject(&projectContext, child, referencedFilePaths);
            break;
        case ItemType::Project:
            copyProperties(projectItem, child);
            handleProject(loadResult, topLevelProjectContext, child, referencedFilePaths);
            break;
        default:
            break;
        }
    }

    const QStringList refs = m_evaluator->stringListValue(
                projectItem, StringConstants::referencesProperty());
    const CodeLocation referencingLocation
            = projectItem->property(StringConstants::referencesProperty())->location();
    QList<Item *> additionalProjectChildren;
    for (const QString &filePath : refs) {
        try {
            additionalProjectChildren << loadReferencedFile(filePath, referencingLocation,
                    referencedFilePaths, dummyProductContext);
        } catch (const ErrorInfo &error) {
            if (m_parameters.productErrorMode() == ErrorHandlingMode::Strict)
                throw;
            m_logger.printWarning(error);
        }
    }
    for (Item * const subItem : qAsConst(additionalProjectChildren)) {
        Item::addChild(projectContext.item, subItem);
        switch (subItem->type()) {
        case ItemType::Product:
            prepareProduct(&projectContext, subItem);
            break;
        case ItemType::Project:
            copyProperties(projectItem, subItem);
            handleProject(loadResult, topLevelProjectContext, subItem,
                          Set<QString>(referencedFilePaths) << subItem->file()->filePath());
            break;
        default:
            break;
        }
    }
    m_reader->popExtraSearchPaths();
}

QString ModuleLoader::MultiplexInfo::toIdString(size_t row) const
{
    const auto &mprow = table.at(row);
    QVariantMap multiplexConfiguration;
    for (size_t column = 0; column < mprow.size(); ++column) {
        const QString &propertyName = properties.at(column);
        const VariantValuePtr &mpvalue = mprow.at(column);
        multiplexConfiguration.insert(propertyName, mpvalue->value());
    }
    QString id = QString::fromUtf8(QJsonDocument::fromVariant(multiplexConfiguration)
                                   .toJson(QJsonDocument::Compact)
                                   .toBase64());
    // Cache for later use in:multiplexIdToVariantMap()
    multiplexConfigurationsById->localData().insert(id, multiplexConfiguration);
    return id;
}

QVariantMap ModuleLoader::MultiplexInfo::multiplexIdToVariantMap(const QString &multiplexId)
{
    if (multiplexId.isEmpty())
        return QVariantMap();

    QVariantMap result = multiplexConfigurationsById->localData().value(multiplexId);
    // We assume that MultiplexInfo::toIdString() has been called for this
    // particular multiplex configuration.
    QBS_CHECK(!result.isEmpty());
    return result;
}

void qbs::Internal::ModuleLoader::ModuleLoader::dump(const ModuleLoader::MultiplexInfo &mpi)
{
    QStringList header;
    for (const auto &str : mpi.properties)
        header << str;
    qDebug() << header;

    for (const auto &row : mpi.table) {
        QVariantList values;
        for (const auto &elem : row) {
            values << elem->value();
        }
        qDebug() << values;
    }
}

ModuleLoader::MultiplexTable ModuleLoader::combine(const MultiplexTable &table,
                                                   const MultiplexRow &values)
{
    MultiplexTable result;
    if (table.empty()) {
        result.resize(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            MultiplexRow row;
            row.resize(1);
            row[0] = values.at(i);
            result[i] = row;
        }
    } else {
        for (const auto &row : table) {
            for (const auto &value : values) {
                MultiplexRow newRow = row;
                newRow.push_back(value);
                result.push_back(newRow);
            }
        }
    }
    return result;
}

ModuleLoader::MultiplexInfo ModuleLoader::extractMultiplexInfo(Item *productItem,
                                                               Item *qbsModuleItem)
{
    static const QString mpmKey = QStringLiteral("multiplexMap");

    const QScriptValue multiplexMap = m_evaluator->value(qbsModuleItem, mpmKey);
    const QStringList multiplexByQbsProperties = m_evaluator->stringListValue(
                productItem, StringConstants::multiplexByQbsPropertiesProperty());

    MultiplexInfo multiplexInfo;
    multiplexInfo.aggregate = m_evaluator->boolValue(
                productItem, StringConstants::aggregateProperty());

    const QString multiplexedType = m_evaluator->stringValue(
                productItem, StringConstants::multiplexedTypeProperty());
    if (!multiplexedType.isEmpty())
        multiplexInfo.multiplexedType = VariantValue::create(multiplexedType);

    Set<QString> uniqueMultiplexByQbsProperties;
    for (const QString &key : multiplexByQbsProperties) {
        const QString mappedKey = multiplexMap.property(key).toString();
        if (mappedKey.isEmpty())
            throw ErrorInfo(Tr::tr("There is no entry for '%1' in 'qbs.multiplexMap'.").arg(key));

        if (!uniqueMultiplexByQbsProperties.insert(mappedKey).second) {
            throw ErrorInfo(Tr::tr("Duplicate entry '%1' in Product.%2.")
                            .arg(mappedKey, StringConstants::multiplexByQbsPropertiesProperty()),
                            productItem->location());
        }

        const QScriptValue arr = m_evaluator->value(qbsModuleItem, key);
        if (arr.isUndefined())
            continue;
        if (!arr.isArray())
            throw ErrorInfo(Tr::tr("Property '%1' must be an array.").arg(key));

        const quint32 arrlen = arr.property(StringConstants::lengthProperty()).toUInt32();
        if (arrlen == 0)
            continue;

        MultiplexRow mprow;
        mprow.resize(arrlen);
        QVariantList entriesForKey;
        for (quint32 i = 0; i < arrlen; ++i) {
            const QVariant value = arr.property(i).toVariant();
            if (entriesForKey.contains(value)) {
                throw ErrorInfo(Tr::tr("Duplicate entry '%1' in qbs.%2.")
                                .arg(value.toString(), key), productItem->location());
            }
            entriesForKey << value;
            mprow[i] = VariantValue::create(value);
        }
        multiplexInfo.table = combine(multiplexInfo.table, mprow);
        multiplexInfo.properties.push_back(mappedKey);
    }
    return multiplexInfo;
}

template <typename T, typename F>
T ModuleLoader::callWithTemporaryBaseModule(ProductContext *productContext, const F &func)
{
    // Temporarily attach the qbs module here, in case we need to access one of its properties
    // to evaluate properties.
    const QString &qbsKey = StringConstants::qbsModule();
    Item *productItem = productContext->item;
    ValuePtr qbsValue = productItem->property(qbsKey); // Retrieve now to restore later.
    if (qbsValue)
        qbsValue = qbsValue->clone();
    const Item::Module qbsModule = loadBaseModule(productContext, productItem);
    productItem->addModule(qbsModule);

    auto &&result = func(qbsModule);

    // "Unload" the qbs module again.
    if (qbsValue)
        productItem->setProperty(qbsKey, qbsValue);
    else
        productItem->removeProperty(qbsKey);
    productItem->removeModules();

    return std::forward<T>(result);
}

QList<Item *> ModuleLoader::multiplexProductItem(ProductContext *dummyContext, Item *productItem)
{
    QString productName;
    dummyContext->item = productItem;
    auto extractMultiplexInfoFromProduct
            = [this, productItem, &productName](const Item::Module &qbsModule) {
        // Overriding the product item properties must be done here already, because multiplexing
        // properties might depend on product properties.
        const QString &nameKey = StringConstants::nameProperty();
        productName = m_evaluator->stringValue(productItem, nameKey);
        if (productName.isEmpty()) {
            productName = FileInfo::completeBaseName(productItem->file()->filePath());
            productItem->setProperty(nameKey, VariantValue::create(productName));
        }
        overrideItemProperties(productItem, StringConstants::productsOverridePrefix() + productName,
                               m_parameters.overriddenValuesTree());

        return extractMultiplexInfo(productItem, qbsModule.item);
    };
    const MultiplexInfo multiplexInfo
            = callWithTemporaryBaseModule<const MultiplexInfo>(dummyContext,
                                                               extractMultiplexInfoFromProduct);

    if (multiplexInfo.table.size() > 1)
        productItem->setProperty(StringConstants::multiplexedProperty(), VariantValue::trueValue());

    VariantValuePtr productNameValue = VariantValue::create(productName);

    Item *aggregator = multiplexInfo.aggregate ? productItem->clone() : nullptr;
    QList<Item *> additionalProductItems;
    std::vector<VariantValuePtr> multiplexConfigurationIdValues;
    for (size_t row = 0; row < multiplexInfo.table.size(); ++row) {
        Item *item = productItem;
        const auto &mprow = multiplexInfo.table.at(row);
        QBS_CHECK(mprow.size() == multiplexInfo.properties.size());
        if (row > 0) {
            item = productItem->clone();
            additionalProductItems.push_back(item);
        }
        const QString multiplexConfigurationId = multiplexInfo.toIdString(row);
        const VariantValuePtr multiplexConfigurationIdValue
            = VariantValue::create(multiplexConfigurationId);
        if (multiplexInfo.table.size() > 1 || aggregator) {
            multiplexConfigurationIdValues.push_back(multiplexConfigurationIdValue);
            item->setProperty(StringConstants::multiplexConfigurationIdProperty(),
                              multiplexConfigurationIdValue);
        }
        if (multiplexInfo.multiplexedType)
            item->setProperty(StringConstants::typeProperty(), multiplexInfo.multiplexedType);
        for (size_t column = 0; column < mprow.size(); ++column) {
            Item *qbsItem = moduleInstanceItem(item, StringConstants::qbsModule());
            const QString &propertyName = multiplexInfo.properties.at(column);
            const VariantValuePtr &mpvalue = mprow.at(column);
            qbsItem->setProperty(propertyName, mpvalue);
        }
    }

    if (aggregator) {
        additionalProductItems << aggregator;

        // Add dependencies to all multiplexed instances.
        for (const auto &v : multiplexConfigurationIdValues) {
            Item *dependsItem = Item::create(aggregator->pool(), ItemType::Depends);
            dependsItem->setProperty(StringConstants::nameProperty(), productNameValue);
            dependsItem->setProperty(StringConstants::multiplexConfigurationIdProperty(), v);
            dependsItem->setProperty(StringConstants::profilesProperty(),
                                     VariantValue::create(QStringList()));
            Item::addChild(aggregator, dependsItem);
        }
    }

    return additionalProductItems;
}

void ModuleLoader::normalizeDependencies(ProductContext *product,
                                         const DeferredDependsContext &dependsContext)
{
    std::vector<Item *> dependsItemsToAdd;
    std::vector<Item *> dependsItemsToRemove;
    std::vector<Item *> deferredDependsItems;
    for (Item *dependsItem : dependsContext.parentItem->children()) {
        if (dependsItem->type() != ItemType::Depends)
            continue;
        bool productTypesIsSet;
        const FileTags productTypes = m_evaluator->fileTagsValue(dependsItem,
                StringConstants::productTypesProperty(), &productTypesIsSet);
        if (productTypesIsSet) {
            bool nameIsSet;
            m_evaluator->stringValue(dependsItem, StringConstants::nameProperty(), QString(),
                                     &nameIsSet);

            // The second condition is for the case where the dependency comes from an Export item
            // that has itself been normalized in the mean time.
            if (nameIsSet && !dependsItem->variantProperty(StringConstants::nameProperty())) {
                throw ErrorInfo(Tr::tr("The 'productTypes' and 'name' properties are mutually "
                                       "exclusive."), dependsItem->location());
            }

            bool submodulesPropertySet;
            m_evaluator->stringListValue( dependsItem, StringConstants::submodulesProperty(),
                                          &submodulesPropertySet);
            if (submodulesPropertySet) {
                throw ErrorInfo(Tr::tr("The 'productTypes' and 'subModules' properties are "
                                       "mutually exclusive."), dependsItem->location());
            }

            // We ignore the "limitToSubProject" property for dependencies from Export items,
            // because we cannot make it work consistently, as the importing product is not
            // yet known when normalizing via an Export item.
            const bool limitToSubProject = dependsContext.parentItem->type() == ItemType::Product
                    && m_evaluator->boolValue(dependsItem,
                                              StringConstants::limitToSubProjectProperty());
            static const auto hasSameSubProject
                    = [](const ProductContext &product, const ProductContext &other) {
                for (const Item *otherParent = other.item->parent(); otherParent;
                     otherParent = otherParent->parent()) {
                    if (otherParent == product.item->parent())
                        return true;
                }
                return false;
            };
            std::vector<const ProductContext *> matchingProducts;
            for (const FileTag &typeTag : productTypes) {
                const auto range = m_productsByType.equal_range(typeTag);
                for (auto it = range.first; it != range.second; ++it) {
                    if (it->second != product
                            && (!product || it->second->name != product->name)
                            && (!limitToSubProject || hasSameSubProject(*product, *it->second))) {
                        matchingProducts.push_back(it->second);
                    }
                }
            }
            if (matchingProducts.empty()) {
                qCDebug(lcModuleLoader) << "Depends.productTypes does not match anything."
                                        << dependsItem->location();
                dependsItemsToRemove.push_back(dependsItem);
                continue;
            }
            if (dependsContext.parentItem->type() != ItemType::Export)
                deferredDependsItems.push_back(dependsItem);
            for (std::size_t i = 1; i < matchingProducts.size(); ++i) {
                Item * const dependsClone = dependsItem->clone();
                dependsClone->setProperty(StringConstants::nameProperty(),
                                         VariantValue::create(matchingProducts.at(i)->name));
                dependsItemsToAdd.push_back(dependsClone);
                if (dependsContext.parentItem->type() != ItemType::Export)
                    deferredDependsItems.push_back(dependsClone);

            }
            dependsItem->setProperty(StringConstants::nameProperty(),
                                     VariantValue::create(matchingProducts.front()->name));
        }
    }
    for (Item * const newDependsItem : dependsItemsToAdd)
        Item::addChild(dependsContext.parentItem, newDependsItem);
    for (Item * const dependsItem : dependsItemsToRemove)
        Item::removeChild(dependsContext.parentItem, dependsItem);
    if (!deferredDependsItems.empty()) {
        auto &allDeferredDependsItems
                = product->deferredDependsItems[dependsContext.exportingProductItem];
        allDeferredDependsItems.insert(allDeferredDependsItems.end(), deferredDependsItems.cbegin(),
                                       deferredDependsItems.cend());
    }
}

void ModuleLoader::adjustDependenciesForMultiplexing(const TopLevelProjectContext &tlp)
{
    for (const ProjectContext * const project : tlp.projects) {
        for (const ProductContext &product : project->products)
            adjustDependenciesForMultiplexing(product);
    }
}

void ModuleLoader::adjustDependenciesForMultiplexing(const ModuleLoader::ProductContext &product)
{
    for (Item *dependsItem : product.item->children()) {
        if (dependsItem->type() == ItemType::Depends)
            adjustDependenciesForMultiplexing(product, dependsItem);
    }
}

void ModuleLoader::adjustDependenciesForMultiplexing(const ProductContext &product,
                                                     Item *dependsItem)
{
    const QString name = m_evaluator->stringValue(dependsItem, StringConstants::nameProperty());
    const bool productIsMultiplexed = !product.multiplexConfigurationId.isEmpty();
    if (name == product.name) {
        QBS_CHECK(!productIsMultiplexed); // This product must be an aggregator.
        return;
    }

    bool profilesPropertyIsSet;
    const QStringList profiles = m_evaluator->stringListValue(dependsItem,
            StringConstants::profilesProperty(), &profilesPropertyIsSet);

    const auto productRange = m_productsByName.equal_range(name);
    if (productRange.first == productRange.second) {
        // Dependency is a module. Nothing to adjust.
        return;
    }

    std::vector<const ProductContext *> multiplexedDependencies;
    bool hasNonMultiplexedDependency = false;
    for (auto it = productRange.first; it != productRange.second; ++it) {
        if (!it->second->multiplexConfigurationId.isEmpty())
            multiplexedDependencies.push_back(it->second);
        else
            hasNonMultiplexedDependency = true;
    }
    bool hasMultiplexedDependencies = !multiplexedDependencies.empty();

    // These are the allowed cases:
    // (1) Normal dependency with no multiplexing whatsoever.
    // (2) Both product and dependency are multiplexed.
    //     (2a) The profiles property is not set, we want to depend on the best
    //          matching variant.
    //     (2b) The profiles property is set, we want to depend on all variants
    //          with a matching profile.
    // (3) The product is not multiplexed, but the dependency is.
    //     (3a) The profiles property is not set, the dependency has an aggregator.
    //          We want to depend on the aggregator.
    //     (3b) The profiles property is not set, the dependency does not have an
    //          aggregator. We want to depend on all the multiplexed variants.
    //     (3c) The profiles property is set, we want to depend on all variants
    //          with a matching profile regardless of whether an aggregator exists or not.
    // (4) The product is multiplexed, but the dependency is not. We don't have to adapt
    //     any Depends items.
    // (5) The product is a "shadow product". In that case, we know which product
    //     it should have a dependency on, and we make sure we depend on that.

    // (1) and (4)
    if (!hasMultiplexedDependencies)
        return;

    // (3a)
    if (!productIsMultiplexed && hasNonMultiplexedDependency && !profilesPropertyIsSet)
        return;

    QStringList multiplexIds;
    const ShadowProductInfo shadowProductInfo = getShadowProductInfo(product);
    const bool isShadowProduct = shadowProductInfo.first && shadowProductInfo.second == name;
    const auto productMultiplexConfig =
            MultiplexInfo::multiplexIdToVariantMap(product.multiplexConfigurationId);

    for (const ProductContext *dependency : multiplexedDependencies) {
        const bool depMatchesShadowProduct = isShadowProduct
                && dependency->item == product.item->parent();
        const QString depMultiplexId = dependency->multiplexConfigurationId;
        if (depMatchesShadowProduct) { // (5)
            dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                                     VariantValue::create(depMultiplexId));
            return;
        }
        if (productIsMultiplexed && !profilesPropertyIsSet) { // 2a
            if (dependency->multiplexConfigurationId == product.multiplexConfigurationId) {
                const ValuePtr &multiplexId = product.item->property(
                            StringConstants::multiplexConfigurationIdProperty());
                dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                                         multiplexId);
                return;

            } else {
                // Otherwise collect partial matches and decide later
                const auto dependencyMultiplexConfig =
                        MultiplexInfo::multiplexIdToVariantMap(dependency->multiplexConfigurationId);

                if (multiplexConfigurationIntersects(dependencyMultiplexConfig, productMultiplexConfig))
                    multiplexIds << dependency->multiplexConfigurationId;
            }
        } else {
            // (2b), (3b) or (3c)
            const bool profileMatch = !profilesPropertyIsSet || profiles.empty()
                    || profiles.contains(dependency->profileName);
            if (profileMatch)
                multiplexIds << depMultiplexId;
        }
    }
    if (multiplexIds.empty()) {
        const QString productName = ResolvedProduct::fullDisplayName(
                    product.name, product.multiplexConfigurationId);
        throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' not fulfilled. "
                               "There are no eligible multiplex candidates.").arg(productName,
                                                                                  name),
                        dependsItem->location());
    }

    // In case of (2a), at most 1 match is allowed
    if (productIsMultiplexed && !profilesPropertyIsSet && multiplexIds.size() > 1) {
        const QString productName = ResolvedProduct::fullDisplayName(
                    product.name, product.multiplexConfigurationId);
        QStringList candidateNames;
        for (const auto &id : qAsConst(multiplexIds))
            candidateNames << ResolvedProduct::fullDisplayName(name, id);
        throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' is ambiguous. "
                               "Eligible multiplex candidates: %3.").arg(
                            productName, name, candidateNames.join(QLatin1String(", "))),
                        dependsItem->location());
    }

    dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                             VariantValue::create(multiplexIds));
}

void ModuleLoader::prepareProduct(ProjectContext *projectContext, Item *productItem)
{
    AccumulatingTimer timer(m_parameters.logElapsedTime()
                            ? &m_elapsedTimePrepareProducts : nullptr);
    checkCancelation();
    qCDebug(lcModuleLoader) << "prepareProduct" << productItem->file()->filePath();

    ProductContext productContext;
    productContext.item = productItem;
    productContext.project = projectContext;
    productContext.name = m_evaluator->stringValue(productItem, StringConstants::nameProperty());
    QBS_CHECK(!productContext.name.isEmpty());
    const ItemValueConstPtr qbsItemValue = productItem->itemProperty(StringConstants::qbsModule());
    if (!!qbsItemValue && qbsItemValue->item()->hasProperty(StringConstants::profileProperty())) {
        qbsItemValue->item()->setProperty(StringConstants::nameProperty(),
                                          VariantValue::create(StringConstants::nameProperty()));
        auto evaluateQbsProfileProperty = [this](const Item::Module &qbsModule) {
            return m_evaluator->stringValue(qbsModule.item,
                                            StringConstants::profileProperty(), QString());
        };
        productContext.profileName
                = callWithTemporaryBaseModule<QString>(&productContext,
                                                       evaluateQbsProfileProperty);
    } else {
        productContext.profileName = m_parameters.topLevelProfile();
    }
    productContext.multiplexConfigurationId = m_evaluator->stringValue(
                productItem, StringConstants::multiplexConfigurationIdProperty());
    QBS_CHECK(!productContext.profileName.isEmpty());
    const auto it = projectContext->result->profileConfigs.constFind(productContext.profileName);
    if (it == projectContext->result->profileConfigs.constEnd()) {
        const Profile profile(productContext.profileName, m_settings.get(), m_localProfiles);
        if (!profile.exists()) {
            ErrorInfo error(Tr::tr("Profile '%1' does not exist.").arg(profile.name()),
                             productItem->location());
            handleProductError(error, &productContext);
            return;
        }
        const QVariantMap buildConfig = SetupProjectParameters::expandedBuildConfiguration(
                    profile, m_parameters.configurationName());
        productContext.moduleProperties = SetupProjectParameters::finalBuildConfigurationTree(
                    buildConfig, m_parameters.overriddenValues());
        projectContext->result->profileConfigs.insert(productContext.profileName,
                                                      productContext.moduleProperties);
    } else {
        productContext.moduleProperties = it.value().toMap();
    }
    initProductProperties(productContext);

    ItemValuePtr itemValue = ItemValue::create(productItem);
    productContext.scope = Item::create(m_pool, ItemType::Scope);
    productContext.scope->setProperty(StringConstants::productVar(), itemValue);
    productContext.scope->setFile(productItem->file());
    productContext.scope->setScope(productContext.project->scope);

    const bool hasExportItems = mergeExportItems(productContext);

    setScopeForDescendants(productItem, productContext.scope);

    projectContext->products.push_back(productContext);

    if (!hasExportItems || getShadowProductInfo(productContext).first)
        return;

    // This "shadow product" exists only to pull in a dependency on the actual product
    // and nothing else, thus providing us with the pure environment that we need to
    // evaluate the product's exported properties in isolation in the project resolver.
    Item * const importer = Item::create(productItem->pool(), ItemType::Product);
    importer->setProperty(QStringLiteral("name"),
                          VariantValue::create(StringConstants::shadowProductPrefix()
                                               + productContext.name));
    importer->setFile(productItem->file());
    importer->setLocation(productItem->location());
    importer->setScope(projectContext->scope);
    importer->setupForBuiltinType(m_logger);
    Item * const dependsItem = Item::create(productItem->pool(), ItemType::Depends);
    dependsItem->setProperty(QStringLiteral("name"), VariantValue::create(productContext.name));
    dependsItem->setProperty(QStringLiteral("required"), VariantValue::create(false));
    dependsItem->setFile(importer->file());
    dependsItem->setLocation(importer->location());
    dependsItem->setupForBuiltinType(m_logger);
    Item::addChild(importer, dependsItem);
    Item::addChild(productItem, importer);
    prepareProduct(projectContext, importer);
}

void ModuleLoader::setupProductDependencies(ProductContext *productContext,
        const Set<DeferredDependsContext> &deferredDependsContext)
{
    if (m_dependencyResolvingPass == 2) {
        for (const DeferredDependsContext &ctx : deferredDependsContext)
            normalizeDependencies(productContext, ctx);
        for (const auto &deferralData : productContext->deferredDependsItems) {
            for (Item * const deferredDependsItem : deferralData.second) {

                // Dependencies from Export items are handled in addProductModuleDependencies().
                if (deferredDependsItem->parent() == productContext->item)
                    adjustDependenciesForMultiplexing(*productContext, deferredDependsItem);
            }
        }
    }
    AccumulatingTimer timer(m_parameters.logElapsedTime()
                            ? &m_elapsedTimeProductDependencies : nullptr);
    checkCancelation();
    Item *item = productContext->item;
    qCDebug(lcModuleLoader) << "setupProductDependencies" << productContext->name
                            << productContext->item->location();

    if (m_dependencyResolvingPass == 1)
        setSearchPathsForProduct(productContext);
    SearchPathsManager searchPathsManager(m_reader, productContext->searchPaths);

    DependsContext dependsContext;
    dependsContext.product = productContext;
    dependsContext.productDependencies = &productContext->info.usedProducts;
    resolveDependencies(&dependsContext, item, productContext);
    if (m_dependencyResolvingPass == 2
            || !containsKey(m_productsWithDeferredDependsItems, productContext)) {
        addProductModuleDependencies(productContext);
    }
    productContext->project->result->productInfos.insert(item, productContext->info);
}

// Leaf modules first.
// TODO: Can this be merged with addTransitiveDependencies? Looks suspiciously similar.
void ModuleLoader::createSortedModuleList(const Item::Module &parentModule, Item::Modules &modules)
{
    if (std::find_if(modules.cbegin(), modules.cend(),
                     [parentModule](const Item::Module &m) { return m.name == parentModule.name;})
            != modules.cend()) {
        return;
    }
    for (const Item::Module &dep : parentModule.item->modules())
        createSortedModuleList(dep, modules);
    modules.push_back(parentModule);
}

Item::Modules ModuleLoader::modulesSortedByDependency(const Item *productItem)
{
    QBS_CHECK(productItem->type() == ItemType::Product);
    Item::Modules sortedModules;
    const Item::Modules &unsortedModules = productItem->modules();
    for (const Item::Module &module : unsortedModules)
        createSortedModuleList(module, sortedModules);
    QBS_CHECK(sortedModules.size() == unsortedModules.size());

    // Make sure the top-level items stay the same.
    for (Item::Module &s : sortedModules) {
        for (const Item::Module &u : unsortedModules) {
            if (s.name == u.name) {
                s.item = u.item;
                break;
            }
        }
    }
    return sortedModules;
}


template<typename T> bool insertIntoSet(Set<T> &set, const T &value)
{
    const auto insertionResult = set.insert(value);
    return insertionResult.second;
}

void ModuleLoader::setupReverseModuleDependencies(const Item::Module &module,
                                                  ModuleDependencies &deps,
                                                  QualifiedIdSet &seenModules)
{
    if (!insertIntoSet(seenModules, module.name))
        return;
    for (const Item::Module &m : module.item->modules()) {
        deps[m.name].insert(module.name);
        setupReverseModuleDependencies(m, deps, seenModules);
    }
}

ModuleLoader::ModuleDependencies ModuleLoader::setupReverseModuleDependencies(const Item *product)
{
    ModuleDependencies deps;
    QualifiedIdSet seenModules;
    for (const Item::Module &m : product->modules())
        setupReverseModuleDependencies(m, deps, seenModules);
    return deps;
}

void ModuleLoader::handleProduct(ModuleLoader::ProductContext *productContext)
{
    AccumulatingTimer timer(m_parameters.logElapsedTime() ? &m_elapsedTimeHandleProducts : nullptr);
    if (productContext->info.delayedError.hasError())
        return;

    Item * const item = productContext->item;

    m_reader->setExtraSearchPathsStack(productContext->project->searchPathsStack);
    SearchPathsManager searchPathsManager(m_reader, productContext->searchPaths);
    addTransitiveDependencies(productContext);

    // It is important that dependent modules are merged after their dependency, because
    // the dependent module's merger potentially needs to replace module items that were
    // set by the dependency module's merger (namely, scopes of defining items; see
    // ModuleMerger::replaceItemInScopes()).
    Item::Modules topSortedModules = modulesSortedByDependency(item);
    ModuleMerger::merge(m_logger, item, productContext->name, &topSortedModules);

    // Re-sort the modules by name. This is more stable; see QBS-818.
    // The list of modules in the product now has the same order as before,
    // only the items have been replaced by their merged counterparts.
    Item::Modules lexicographicallySortedModules = topSortedModules;
    std::sort(lexicographicallySortedModules.begin(), lexicographicallySortedModules.end());
    item->setModules(lexicographicallySortedModules);

    for (const Item::Module &module : topSortedModules) {
        if (!module.item->isPresentModule())
            continue;
        try {
            resolveProbes(productContext, module.item);
            if (module.versionRange.minimum.isValid()
                    || module.versionRange.maximum.isValid()) {
                if (module.versionRange.maximum.isValid()
                        && module.versionRange.minimum >= module.versionRange.maximum) {
                    throw ErrorInfo(Tr::tr("Impossible version constraint [%1,%2) set for module "
                                           "'%3'").arg(module.versionRange.minimum.toString(),
                                                       module.versionRange.maximum.toString(),
                                                       module.name.toString()));
                }
                const Version moduleVersion = Version::fromString(
                            m_evaluator->stringValue(module.item,
                                                     StringConstants::versionProperty()));
                if (moduleVersion < module.versionRange.minimum) {
                    throw ErrorInfo(Tr::tr("Module '%1' has version %2, but it needs to be "
                            "at least %3.").arg(module.name.toString(),
                                                moduleVersion.toString(),
                                                module.versionRange.minimum.toString()));
                }
                if (module.versionRange.maximum.isValid()
                        && moduleVersion >= module.versionRange.maximum) {
                    throw ErrorInfo(Tr::tr("Module '%1' has version %2, but it needs to be "
                            "lower than %3.").arg(module.name.toString(),
                                               moduleVersion.toString(),
                                               module.versionRange.maximum.toString()));
                }
            }
        } catch (const ErrorInfo &error) {
            handleModuleSetupError(productContext, module, error);
            if (productContext->info.delayedError.hasError())
                return;
        }
    }

    resolveProbes(productContext, item);

    // Module validation must happen in an extra pass, after all Probes have been resolved.
    EvalCacheEnabler cacheEnabler(m_evaluator);
    for (const Item::Module &module : topSortedModules) {
        if (!module.item->isPresentModule())
            continue;
        try {
            m_evaluator->boolValue(module.item, StringConstants::validateProperty());
        } catch (const ErrorInfo &error) {
            handleModuleSetupError(productContext, module, error);
            if (productContext->info.delayedError.hasError())
                return;
        }
    }

    if (!checkItemCondition(item)) {
        const auto &exportsData = productContext->project->topLevelProject->productModules;
        for (auto it = exportsData.find(productContext->name);
             it != exportsData.end() && it.key() == productContext->name; ++it) {
            if (it.value().multiplexId == productContext->multiplexConfigurationId) {
                createNonPresentModule(productContext->name, QStringLiteral("disabled"),
                                       it.value().exportItem);
                break;
            }
        }
    }

    checkDependencyParameterDeclarations(productContext);
    copyGroupsFromModulesToProduct(*productContext);

    ModuleDependencies reverseModuleDeps;
    for (Item * const child : item->children()) {
        if (child->type() == ItemType::Group) {
            if (reverseModuleDeps.empty())
                reverseModuleDeps = setupReverseModuleDependencies(item);
            handleGroup(productContext, child, reverseModuleDeps);
        }
    }
    productContext->project->result->productInfos.insert(item, productContext->info);
}

static Item *rootPrototype(Item *item)
{
    Item *modulePrototype = item;
    while (modulePrototype->prototype())
        modulePrototype = modulePrototype->prototype();
    return modulePrototype;
}

class DependencyParameterDeclarationCheck
{
public:
    DependencyParameterDeclarationCheck(const QString &productName, const Item *productItem,
            const QHash<const Item *, Item::PropertyDeclarationMap> &decls)
        : m_productName(productName), m_productItem(productItem), m_parameterDeclarations(decls)
    {
    }

    void operator()(const QVariantMap &parameters) const
    {
        check(parameters, QualifiedId());
    }

private:
    void check(const QVariantMap &parameters, const QualifiedId &moduleName) const
    {
        for (auto it = parameters.begin(); it != parameters.end(); ++it) {
            if (it.value().type() == QVariant::Map) {
                check(it.value().toMap(), QualifiedId(moduleName) << it.key());
            } else {
                const auto &deps = m_productItem->modules();
                auto m = std::find_if(deps.begin(), deps.end(),
                                      [&moduleName] (const Item::Module &module) {
                    return module.name == moduleName;
                });

                if (m == deps.end()) {
                    const QualifiedId fullName = QualifiedId(moduleName) << it.key();
                    throw ErrorInfo(Tr::tr("Cannot set parameter '%1', "
                                           "because '%2' does not have a dependency on '%3'.")
                                    .arg(fullName.toString(), m_productName, moduleName.toString()),
                                    m_productItem->location());
                }

                auto decls = m_parameterDeclarations.value(rootPrototype(m->item));

                if (!decls.contains(it.key())) {
                    const QualifiedId fullName = QualifiedId(moduleName) << it.key();
                    throw ErrorInfo(Tr::tr("Parameter '%1' is not declared.")
                                    .arg(fullName.toString()), m_productItem->location());
                }
            }
        }
    }

    bool moduleExists(const QualifiedId &name) const
    {
        const auto &deps = m_productItem->modules();
        return any_of(deps, [&name](const Item::Module &module) {
            return module.name == name;
        });
    }

    const QString &m_productName;
    const Item *m_productItem;
    const QHash<const Item *, Item::PropertyDeclarationMap> &m_parameterDeclarations;
};

void ModuleLoader::checkDependencyParameterDeclarations(const ProductContext *productContext) const
{
    DependencyParameterDeclarationCheck dpdc(productContext->name, productContext->item,
                                             m_parameterDeclarations);
    for (const Item::Module &dep : productContext->item->modules()) {
        if (!dep.parameters.empty())
            dpdc(dep.parameters);
    }
}

void ModuleLoader::handleModuleSetupError(ModuleLoader::ProductContext *productContext,
                                          const Item::Module &module, const ErrorInfo &error)
{
    if (module.required) {
        handleProductError(error, productContext);
    } else {
        qCDebug(lcModuleLoader()) << "non-required module" << module.name.toString()
                                  << "found, but not usable in product" << productContext->name
                                  << error.toString();
        createNonPresentModule(module.name.toString(), QStringLiteral("failed validation"),
                               module.item);
    }
}

void ModuleLoader::initProductProperties(const ProductContext &product)
{
    QString buildDir = ResolvedProduct::deriveBuildDirectoryName(product.name,
                                                                 product.multiplexConfigurationId);
    buildDir = FileInfo::resolvePath(product.project->topLevelProject->buildDirectory, buildDir);
    product.item->setProperty(StringConstants::buildDirectoryProperty(),
                              VariantValue::create(buildDir));
    const QString sourceDir = QFileInfo(product.item->file()->filePath()).absolutePath();
    product.item->setProperty(StringConstants::sourceDirectoryProperty(),
                              VariantValue::create(sourceDir));
}

void ModuleLoader::handleSubProject(ModuleLoader::ProjectContext *projectContext, Item *projectItem,
        const Set<QString> &referencedFilePaths)
{
    qCDebug(lcModuleLoader) << "handleSubProject" << projectItem->file()->filePath();

    Item * const propertiesItem = projectItem->child(ItemType::PropertiesInSubProject);
    if (!checkItemCondition(projectItem))
        return;
    if (propertiesItem) {
        propertiesItem->setScope(projectItem);
        if (!checkItemCondition(propertiesItem))
            return;
    }

    Item *loadedItem;
    QString subProjectFilePath;
    try {
        const QString projectFileDirPath = FileInfo::path(projectItem->file()->filePath());
        const QString relativeFilePath
                = m_evaluator->stringValue(projectItem, StringConstants::filePathProperty());
        subProjectFilePath = FileInfo::resolvePath(projectFileDirPath, relativeFilePath);
        if (referencedFilePaths.contains(subProjectFilePath))
            throw ErrorInfo(Tr::tr("Cycle detected while loading subproject file '%1'.")
                            .arg(relativeFilePath), projectItem->location());
        loadedItem = loadItemFromFile(subProjectFilePath, projectItem->location());
    } catch (const ErrorInfo &error) {
        if (m_parameters.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        m_logger.printWarning(error);
        return;
    }

    loadedItem = wrapInProjectIfNecessary(loadedItem);
    const bool inheritProperties = m_evaluator->boolValue(
                projectItem, StringConstants::inheritPropertiesProperty());

    if (inheritProperties)
        copyProperties(projectItem->parent(), loadedItem);
    if (propertiesItem) {
        const Item::PropertyMap &overriddenProperties = propertiesItem->properties();
        for (Item::PropertyMap::ConstIterator it = overriddenProperties.constBegin();
             it != overriddenProperties.constEnd(); ++it) {
            loadedItem->setProperty(it.key(), it.value());
        }
    }

    Item::addChild(projectItem, loadedItem);
    projectItem->setScope(projectContext->scope);
    handleProject(projectContext->result, projectContext->topLevelProject, loadedItem,
                  Set<QString>(referencedFilePaths) << subProjectFilePath);
}

QList<Item *> ModuleLoader::loadReferencedFile(const QString &relativePath,
                                               const CodeLocation &referencingLocation,
                                               const Set<QString> &referencedFilePaths,
                                               ModuleLoader::ProductContext &dummyContext)
{
    QString absReferencePath = FileInfo::resolvePath(FileInfo::path(referencingLocation.filePath()),
                                                     relativePath);
    if (FileInfo(absReferencePath).isDir()) {
        QString qbsFilePath;

        QDirIterator dit(absReferencePath, StringConstants::qbsFileWildcards());
        while (dit.hasNext()) {
            if (!qbsFilePath.isEmpty()) {
                throw ErrorInfo(Tr::tr("Referenced directory '%1' contains more than one "
                                       "qbs file.").arg(absReferencePath), referencingLocation);
            }
            qbsFilePath = dit.next();
        }
        if (qbsFilePath.isEmpty()) {
            throw ErrorInfo(Tr::tr("Referenced directory '%1' does not contain a qbs file.")
                            .arg(absReferencePath), referencingLocation);
        }
        absReferencePath = qbsFilePath;
    }
    if (referencedFilePaths.contains(absReferencePath))
        throw ErrorInfo(Tr::tr("Cycle detected while referencing file '%1'.").arg(relativePath),
                        referencingLocation);
    Item * const subItem = loadItemFromFile(absReferencePath, referencingLocation);
    if (subItem->type() != ItemType::Project && subItem->type() != ItemType::Product) {
        ErrorInfo error(Tr::tr("Item type should be 'Product' or 'Project', but is '%1'.")
                        .arg(subItem->typeName()));
        error.append(Tr::tr("Item is defined here."), subItem->location());
        error.append(Tr::tr("File is referenced here."), referencingLocation);
        throw  error;
    }
    subItem->setScope(dummyContext.project->scope);
    subItem->setParent(dummyContext.project->item);
    QList<Item *> loadedItems;
    loadedItems << subItem;
    if (subItem->type() == ItemType::Product) {
        handleProfileItems(subItem, dummyContext.project);
        loadedItems << multiplexProductItem(&dummyContext, subItem);
    }
    return loadedItems;
}

void ModuleLoader::handleGroup(ProductContext *productContext, Item *groupItem,
                               const ModuleDependencies &reverseDepencencies)
{
    checkCancelation();
    propagateModulesFromParent(productContext, groupItem, reverseDepencencies);
    checkItemCondition(groupItem);
    for (Item * const child : groupItem->children()) {
        if (child->type() == ItemType::Group)
            handleGroup(productContext, child, reverseDepencencies);
    }
}

void ModuleLoader::handleAllPropertyOptionsItems(Item *item)
{
    QList<Item *> childItems = item->children();
    auto childIt = childItems.begin();
    while (childIt != childItems.end()) {
        Item * const child = *childIt;
        if (child->type() == ItemType::PropertyOptions) {
            handlePropertyOptions(child);
            childIt = childItems.erase(childIt);
        } else {
            handleAllPropertyOptionsItems(child);
            ++childIt;
        }
    }
    item->setChildren(childItems);
}

void ModuleLoader::handlePropertyOptions(Item *optionsItem)
{
    const QString name = m_evaluator->stringValue(optionsItem, StringConstants::nameProperty());
    if (name.isEmpty()) {
        throw ErrorInfo(Tr::tr("PropertyOptions item needs a name property"),
                        optionsItem->location());
    }
    const QString description = m_evaluator->stringValue(
                optionsItem, StringConstants::descriptionProperty());
    const auto removalVersion = Version::fromString(m_evaluator->stringValue(optionsItem,
            StringConstants::removalVersionProperty()));
    PropertyDeclaration decl = optionsItem->parent()->propertyDeclaration(name);
    if (!decl.isValid()) {
        decl.setName(name);
        decl.setType(PropertyDeclaration::Variant);
    }
    decl.setDescription(description);
    if (removalVersion.isValid()) {
        DeprecationInfo di(removalVersion, description);
        decl.setDeprecationInfo(di);
    }
    const ValuePtr property = optionsItem->parent()->property(name);
    if (!property && !decl.isExpired()) {
        throw ErrorInfo(Tr::tr("PropertyOptions item refers to non-existing property '%1'")
                        .arg(name), optionsItem->location());
    }
    if (property && decl.isExpired()) {
        ErrorInfo e(Tr::tr("Property '%1' was scheduled for removal in version %2, but "
                           "is still present.")
                    .arg(name).arg(removalVersion.toString()),
                    property->location());
        e.append(Tr::tr("Removal version for '%1' specified here.").arg(name),
                 optionsItem->location());
        m_logger.printWarning(e);
    }
    optionsItem->parent()->setPropertyDeclaration(name, decl);
}

static void mergeProperty(Item *dst, const QString &name, const ValuePtr &value)
{
    if (value->type() == Value::ItemValueType) {
        const ItemValueConstPtr itemValue = std::static_pointer_cast<ItemValue>(value);
        const Item * const valueItem = itemValue->item();
        Item * const subItem = dst->itemProperty(name, itemValue)->item();
        for (QMap<QString, ValuePtr>::const_iterator it = valueItem->properties().constBegin();
                it != valueItem->properties().constEnd(); ++it)
            mergeProperty(subItem, it.key(), it.value());
    } else {
        // If the property already exists set up the base value.
        if (value->type() == Value::JSSourceValueType) {
            const auto jsValue = static_cast<JSSourceValue *>(value.get());
            if (jsValue->isBuiltinDefaultValue())
                return;
            const ValuePtr baseValue = dst->property(name);
            if (baseValue) {
                QBS_CHECK(baseValue->type() == Value::JSSourceValueType);
                const JSSourceValuePtr jsBaseValue = std::static_pointer_cast<JSSourceValue>(
                            baseValue->clone());
                jsValue->setBaseValue(jsBaseValue);
                std::vector<JSSourceValue::Alternative> alternatives = jsValue->alternatives();
                jsValue->clearAlternatives();
                for (JSSourceValue::Alternative &a : alternatives) {
                    a.value->setBaseValue(jsBaseValue);
                    jsValue->addAlternative(a);
                }
            }
        }
        dst->setProperty(name, value);
    }
}

bool ModuleLoader::checkExportItemCondition(Item *exportItem, const ProductContext &productContext)
{
    class ScopeHandler {
    public:
        ScopeHandler(Item *exportItem, const ProductContext &productContext, Item **cachedScopeItem)
            : m_exportItem(exportItem)
        {
            if (!*cachedScopeItem)
                *cachedScopeItem = Item::create(exportItem->pool(), ItemType::Scope);
            Item * const scope = *cachedScopeItem;
            QBS_CHECK(productContext.item->file());
            scope->setFile(productContext.item->file());
            scope->setScope(productContext.item);
            productContext.project->scope->copyProperty(StringConstants::projectVar(), scope);
            productContext.scope->copyProperty(StringConstants::productVar(), scope);
            QBS_CHECK(!exportItem->scope());
            exportItem->setScope(scope);
        }
        ~ScopeHandler() { m_exportItem->setScope(nullptr); }

    private:
        Item * const m_exportItem;
    } scopeHandler(exportItem, productContext, &m_tempScopeItem);
    return checkItemCondition(exportItem);
}

ProbeConstPtr ModuleLoader::findOldProjectProbe(
        const QString &globalId,
        bool condition,
        const QVariantMap &initialProperties,
        const QString &sourceCode) const
{
    if (m_parameters.forceProbeExecution())
        return {};

    for (const ProbeConstPtr &oldProbe : m_oldProjectProbes.value(globalId)) {
        if (probeMatches(oldProbe, condition, initialProperties, sourceCode, CompareScript::Yes))
            return oldProbe;
    }

    return {};
}

ProbeConstPtr ModuleLoader::findOldProductProbe(
        const QString &productName,
        bool condition,
        const QVariantMap &initialProperties,
        const QString &sourceCode) const
{
    if (m_parameters.forceProbeExecution())
        return {};

    for (const ProbeConstPtr &oldProbe : m_oldProductProbes.value(productName)) {
        if (probeMatches(oldProbe, condition, initialProperties, sourceCode, CompareScript::Yes))
            return oldProbe;
    }

    return {};
}

ProbeConstPtr ModuleLoader::findCurrentProbe(
        const CodeLocation &location,
        bool condition,
        const QVariantMap &initialProperties) const
{
    const std::vector<ProbeConstPtr> &cachedProbes = m_currentProbes.value(location);
    for (const ProbeConstPtr &probe : cachedProbes) {
        if (probeMatches(probe, condition, initialProperties, QString(), CompareScript::No))
            return probe;
    }
    return {};
}

bool ModuleLoader::probeMatches(const ProbeConstPtr &probe, bool condition,
        const QVariantMap &initialProperties, const QString &configureScript,
        CompareScript compareScript) const
{
    return probe->condition() == condition
            && probe->initialProperties() == initialProperties
            && (compareScript == CompareScript::No
                || (probe->configureScript() == configureScript
                    && !probe->needsReconfigure(m_lastResolveTime)));
}

void ModuleLoader::printProfilingInfo()
{
    if (!m_parameters.logElapsedTime())
        return;
    m_logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Project file loading and parsing took %1.")
                                         .arg(elapsedTimeString(m_reader->elapsedTime()));
    m_logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Preparing products took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimePrepareProducts));
    m_logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Setting up product dependencies took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimeProductDependencies));
    m_logger.qbsLog(LoggerInfo, true) << "\t\t"
            << Tr::tr("Setting up transitive product dependencies took %1.")
               .arg(elapsedTimeString(m_elapsedTimeTransitiveDependencies));
    m_logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Handling products took %1.")
                                      .arg(elapsedTimeString(m_elapsedTimeHandleProducts));
    m_logger.qbsLog(LoggerInfo, true) << "\t\t"
                                      << Tr::tr("Running Probes took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimeProbes));
    m_logger.qbsLog(LoggerInfo, true) << "\t\t"
            << Tr::tr("%1 probes encountered, %2 configure scripts executed, "
                      "%3 re-used from current run, %4 re-used from earlier run.")
               .arg(m_probesEncountered).arg(m_probesRun).arg(m_probesCachedCurrent)
               .arg(m_probesCachedOld);
    m_logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Property checking took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimePropertyChecking));
}

static void mergeParameters(QVariantMap &dst, const QVariantMap &src)
{
    for (auto it = src.begin(); it != src.end(); ++it) {
        if (it.value().type() == QVariant::Map) {
            QVariant &vdst = dst[it.key()];
            QVariantMap mdst = vdst.toMap();
            mergeParameters(mdst, it.value().toMap());
            vdst = mdst;
        } else {
            dst[it.key()] = it.value();
        }
    }
}

static void adjustParametersScopes(Item *item, Item *scope)
{
    if (item->type() == ItemType::ModuleParameters) {
        item->setScope(scope);
        return;
    }

    for (const auto &value : item->properties()) {
        if (value->type() != Value::ItemValueType)
            continue;
        adjustParametersScopes(std::static_pointer_cast<ItemValue>(value)->item(), scope);
    }
}

bool ModuleLoader::mergeExportItems(const ProductContext &productContext)
{
    std::vector<Item *> exportItems;
    QList<Item *> children = productContext.item->children();
    for (int i = 0; i < children.size();) {
        Item * const child = children.at(i);
        if (child->type() == ItemType::Export) {
            exportItems.push_back(child);
            children.removeAt(i);
        } else {
            ++i;
        }
    }

    // Note that we do not return if there are no Export items: The "merged" item becomes the
    // "product module", which always needs to exist, regardless of whether the product sources
    // actually contain an Export item or not.
    if (!exportItems.empty())
        productContext.item->setChildren(children);

    Item *merged = Item::create(productContext.item->pool(), ItemType::Export);
    const QString &nameKey = StringConstants::nameProperty();
    const ValuePtr nameValue = VariantValue::create(productContext.name);
    merged->setProperty(nameKey, nameValue);
    Set<FileContextConstPtr> filesWithExportItem;
    ProductModuleInfo pmi;
    bool hasDependenciesOnProductType = false;
    for (Item * const exportItem : qAsConst(exportItems)) {
        checkCancelation();
        if (Q_UNLIKELY(filesWithExportItem.contains(exportItem->file())))
            throw ErrorInfo(Tr::tr("Multiple Export items in one product are prohibited."),
                        exportItem->location());
        exportItem->setProperty(nameKey, nameValue);
        if (!checkExportItemCondition(exportItem, productContext))
            continue;
        filesWithExportItem += exportItem->file();
        for (Item * const child : exportItem->children()) {
            if (child->type() == ItemType::Parameters) {
                adjustParametersScopes(child, child);
                mergeParameters(pmi.defaultParameters,
                                m_evaluator->scriptValue(child).toVariant().toMap());
            } else {
                if (child->type() == ItemType::Depends) {
                    bool productTypesIsSet;
                    m_evaluator->stringValue(child, StringConstants::productTypesProperty(),
                                             QString(), &productTypesIsSet);
                    if (productTypesIsSet)
                        hasDependenciesOnProductType = true;
                }
                Item::addChild(merged, child);
            }
        }
        const Item::PropertyDeclarationMap &decls = exportItem->propertyDeclarations();
        for (auto it = decls.constBegin(); it != decls.constEnd(); ++it) {
            const PropertyDeclaration &newDecl = it.value();
            const PropertyDeclaration &existingDecl = merged->propertyDeclaration(it.key());
            if (existingDecl.isValid() && existingDecl.type() != newDecl.type()) {
                ErrorInfo error(Tr::tr("Export item in inherited item redeclares property "
                        "'%1' with different type.").arg(it.key()), exportItem->location());
                handlePropertyError(error, m_parameters, m_logger);
            }
            merged->setPropertyDeclaration(newDecl.name(), newDecl);
        }
        for (QMap<QString, ValuePtr>::const_iterator it = exportItem->properties().constBegin();
                it != exportItem->properties().constEnd(); ++it) {
            mergeProperty(merged, it.key(), it.value());
        }
    }
    merged->setFile(exportItems.empty()
            ? productContext.item->file() : exportItems.back()->file());
    merged->setLocation(exportItems.empty()
            ? productContext.item->location() : exportItems.back()->location());
    Item::addChild(productContext.item, merged);
    merged->setupForBuiltinType(m_logger);
    pmi.exportItem = merged;
    pmi.multiplexId = productContext.multiplexConfigurationId;
    productContext.project->topLevelProject->productModules.insert(productContext.name, pmi);
    if (hasDependenciesOnProductType)
        m_exportsWithDeferredDependsItems.insert(merged);
    return !exportItems.empty();
}

Item *ModuleLoader::loadItemFromFile(const QString &filePath,
                                     const CodeLocation &referencingLocation)
{
    Item *item;
    try {
        item = m_reader->readFile(filePath);
    } catch (const ErrorInfo &e) {
        if (e.hasLocation())
            throw;
        throw ErrorInfo(e.toString(), referencingLocation);
    }
    handleAllPropertyOptionsItems(item);
    return item;
}

void ModuleLoader::handleProfileItems(Item *item, ProjectContext *projectContext)
{
    const std::vector<Item *> profileItems = collectProfileItems(item, projectContext);
    for (Item * const profileItem : profileItems) {
        try {
            handleProfile(profileItem);
        } catch (const ErrorInfo &e) {
            handlePropertyError(e, m_parameters, m_logger);
        }
    }
}

std::vector<Item *> ModuleLoader::collectProfileItems(Item *item, ProjectContext *projectContext)
{
    QList<Item *> childItems = item->children();
    std::vector<Item *> profileItems;
    Item * scope = item->type() == ItemType::Project ? projectContext->scope : nullptr;
    for (auto it = childItems.begin(); it != childItems.end();) {
        Item * const childItem = *it;
        if (childItem->type() == ItemType::Profile) {
            if (!scope) {
                const ItemValuePtr itemValue = ItemValue::create(item);
                scope = Item::create(m_pool, ItemType::Scope);
                scope->setProperty(StringConstants::productVar(), itemValue);
                scope->setFile(item->file());
                scope->setScope(projectContext->scope);
            }
            childItem->setScope(scope);
            profileItems.push_back(childItem);
            it = childItems.erase(it);
        } else {
            if (childItem->type() == ItemType::Product) {
                for (Item * const profileItem : collectProfileItems(childItem, projectContext))
                    profileItems.push_back(profileItem);
            }
            ++it;
        }
    }
    if (!profileItems.empty())
        item->setChildren(childItems);
    return profileItems;
}

void ModuleLoader::evaluateProfileValues(const QualifiedId &namePrefix, Item *item,
                                         Item *profileItem, QVariantMap &values)
{
    const Item::PropertyMap &props = item->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        QualifiedId name = namePrefix;
        name << it.key();
        switch (it.value()->type()) {
        case Value::ItemValueType:
            evaluateProfileValues(name, std::static_pointer_cast<ItemValue>(it.value())->item(),
                                  profileItem, values);
            break;
        case Value::VariantValueType:
            values.insert(name.join(QLatin1Char('.')),
                          std::static_pointer_cast<VariantValue>(it.value())->value());
            break;
        case Value::JSSourceValueType:
            item->setType(ItemType::ModulePrefix); // TODO: Introduce new item type
            if (item != profileItem)
                item->setScope(profileItem);
            values.insert(name.join(QLatin1Char('.')),
                          m_evaluator->value(item, it.key()).toVariant());
            break;
        }
    }
}

void ModuleLoader::handleProfile(Item *profileItem)
{
    QVariantMap values;
    evaluateProfileValues(QualifiedId(), profileItem, profileItem, values);
    const bool condition = values.take(StringConstants::conditionProperty()).toBool();
    if (!condition)
        return;
    const QString profileName = values.take(StringConstants::nameProperty()).toString();
    if (profileName.isEmpty())
        throw ErrorInfo(Tr::tr("Every Profile item must have a name"), profileItem->location());
    if (profileName == Profile::fallbackName()) {
        throw ErrorInfo(Tr::tr("Reserved name '%1' cannot be used for an actual profile.")
                        .arg(profileName), profileItem->location());
    }
    if (m_localProfiles.contains(profileName)) {
        throw ErrorInfo(Tr::tr("Local profile '%1' redefined.").arg(profileName),
                        profileItem->location());
    }
    m_localProfiles.insert(profileName, values);
}

void ModuleLoader::collectNameFromOverride(const QString &overrideString)
{
    static const auto extract = [](const QString &prefix, const QString &overrideString) {
        if (!overrideString.startsWith(prefix))
            return QString();
        const int startPos = prefix.length();
        const int endPos = overrideString.lastIndexOf(StringConstants::dot());
        if (endPos == -1)
            return QString();
        return overrideString.mid(startPos, endPos - startPos);
    };
    const QString &projectName = extract(StringConstants::projectsOverridePrefix(), overrideString);
    if (!projectName.isEmpty()) {
        m_projectNamesUsedInOverrides.insert(projectName);
        return;
    }
    const QString &productName = extract(StringConstants::productsOverridePrefix(), overrideString);
    if (!productName.isEmpty()) {
        m_productNamesUsedInOverrides.insert(productName.left(
                                                 productName.indexOf(StringConstants::dot())));
        return;
    }
}

void ModuleLoader::checkProjectNamesInOverrides(const ModuleLoader::TopLevelProjectContext &tlp)
{
    for (const QString &projectNameInOverride : m_projectNamesUsedInOverrides) {
        if (m_disabledProjects.contains(projectNameInOverride))
            continue;
        bool found = false;
        for (const ProjectContext * const p : tlp.projects) {
            if (p->name == projectNameInOverride) {
                found = true;
                break;
            }
        }
        if (!found) {
            handlePropertyError(Tr::tr("Unknown project '%1' in property override.")
                                .arg(projectNameInOverride), m_parameters, m_logger);
        }
    }
}

void ModuleLoader::checkProductNamesInOverrides()
{
    for (const QString &productNameInOverride : m_productNamesUsedInOverrides) {
        if (m_erroneousProducts.contains(productNameInOverride))
            continue;
        bool found = false;
        for (const auto &kv : m_productsByName) {
            // In an override string such as "a.b.c:d, we cannot tell whether we have a product
            // "a" and a module "b.c" or a product "a.b" and a module "c", so we need to take
            // care not to emit false positives here.
            if (kv.first == productNameInOverride
                    || kv.first.startsWith(productNameInOverride + StringConstants::dot())) {
                found = true;
                break;
            }
        }
        if (!found) {
            handlePropertyError(Tr::tr("Unknown product '%1' in property override.")
                                .arg(productNameInOverride), m_parameters, m_logger);
        }
    }
}

void ModuleLoader::setSearchPathsForProduct(ModuleLoader::ProductContext *product)
{
    product->searchPaths = readExtraSearchPaths(product->item);
    Settings settings(m_parameters.settingsDirectory());
    const QVariantMap profileContents = product->project->result->profileConfigs
            .value(product->profileName).toMap();
    const QStringList prefsSearchPaths = Preferences(&settings, profileContents).searchPaths();
    const QStringList &currentSearchPaths = m_reader->allSearchPaths();
    for (const QString &p : prefsSearchPaths) {
        if (!currentSearchPaths.contains(p) && FileInfo(p).exists())
            product->searchPaths << p;
    }

    // Existing module provider search paths are re-used if and only if the provider configuration
    // at setup time was the same as the current one for the respective module provider.
    if (!m_moduleProviderInfo.empty()) {
        const QVariantMap configForProduct = moduleProviderConfig(*product);
        for (const ModuleProviderInfo &c : m_moduleProviderInfo) {
            if (configForProduct.value(c.name.toString()).toMap() == c.config) {
                qCDebug(lcModuleLoader) << "re-using search paths" << c.searchPaths
                                        << "from module provider" << c.name
                                        << "for product" << product->name;
                product->knownModuleProviders.insert(c.name);
                product->searchPaths << c.searchPaths;
            }
        }
    }
}

ModuleLoader::ShadowProductInfo ModuleLoader::getShadowProductInfo(
        const ModuleLoader::ProductContext &product) const
{
    const bool isShadowProduct = product.name.startsWith(StringConstants::shadowProductPrefix());
    return std::make_pair(isShadowProduct, isShadowProduct
                          ? product.name.mid(StringConstants::shadowProductPrefix().size())
                          : QString());
}

void ModuleLoader::collectProductsByName(const TopLevelProjectContext &topLevelProject)
{
    for (ProjectContext * const project : topLevelProject.projects) {
        for (ProductContext &product : project->products)
            m_productsByName.insert({ product.name, &product });
    }
}

void ModuleLoader::collectProductsByType(const ModuleLoader::TopLevelProjectContext &topLevelProject)
{
    for (ProjectContext * const project : topLevelProject.projects) {
        for (ProductContext &product : project->products) {
            try {
                const FileTags productTags
                        = m_evaluator->fileTagsValue(product.item, StringConstants::typeProperty());
                for (const FileTag &tag : productTags)
                    m_productsByType.insert({ tag, &product});
            } catch (const ErrorInfo &) {
                qCDebug(lcModuleLoader) << "product" << product.name << "has complex type "
                                           " and won't get an entry in the type map";
            }
        }
    }
}

void ModuleLoader::propagateModulesFromParent(ProductContext *productContext, Item *groupItem,
                                              const ModuleDependencies &reverseDepencencies)
{
    QBS_CHECK(groupItem->type() == ItemType::Group);
    QHash<QualifiedId, Item *> moduleInstancesForGroup;

    // Step 1: Instantiate the product's modules for the group.
    for (Item::Module m : groupItem->parent()->modules()) {
        Item *targetItem = moduleInstanceItem(groupItem, m.name);
        targetItem->setPrototype(m.item);

        Item * const moduleScope = Item::create(targetItem->pool(), ItemType::Scope);
        moduleScope->setFile(groupItem->file());
        moduleScope->setProperties(m.item->scope()->properties()); // "project", "product", ids
        moduleScope->setScope(groupItem);
        targetItem->setScope(moduleScope);

        targetItem->setFile(m.item->file());

        // "parent" should point to the group/artifact parent
        targetItem->setParent(groupItem->parent());

        targetItem->setOuterItem(m.item);

        m.item = targetItem;
        groupItem->addModule(m);
        moduleInstancesForGroup.insert(m.name, targetItem);
    }

    // Step 2: Make the inter-module references point to the instances created in step 1.
    for (const Item::Module &module : groupItem->modules()) {
        Item::Modules adaptedModules;
        const Item::Modules &oldModules = module.item->prototype()->modules();
        for (Item::Module depMod : oldModules) {
            depMod.item = moduleInstancesForGroup.value(depMod.name);
            adaptedModules << depMod;
            if (depMod.name.front() == module.name.front())
                continue;
            const ItemValuePtr &modulePrefix = groupItem->itemProperty(depMod.name.front());
            QBS_CHECK(modulePrefix);
            module.item->setProperty(depMod.name.front(), modulePrefix);
        }
        module.item->setModules(adaptedModules);
    }

    const QualifiedIdSet &propsSetInGroup = gatherModulePropertiesSetInGroup(groupItem);
    if (propsSetInGroup.empty())
        return;
    productContext->info.modulePropertiesSetInGroups
            .insert(std::make_pair(groupItem, propsSetInGroup));

    // Step 3: Adapt defining items in values. This is potentially necessary if module properties
    //         get assigned on the group level.
    for (const Item::Module &module : groupItem->modules()) {
        const QualifiedIdSet &dependents = reverseDepencencies.value(module.name);
        Item::Modules dependentModules;
        dependentModules.reserve(int(dependents.size()));
        for (const QualifiedId &depName : dependents) {
            Item * const itemOfDependent = moduleInstancesForGroup.value(depName);
            QBS_CHECK(itemOfDependent);
            Item::Module depMod;
            depMod.name = depName;
            depMod.item = itemOfDependent;
            dependentModules << depMod;
        }
        adjustDefiningItemsInGroupModuleInstances(module, dependentModules);
    }
}

static Item *createReplacementForDefiningItem(const Item *definingItem, ItemType type)
{
    Item *replacement = Item::create(definingItem->pool(), type);
    replacement->setLocation(definingItem->location());
    definingItem->copyProperty(StringConstants::nameProperty(), replacement);
    return replacement;
}

void ModuleLoader::adjustDefiningItemsInGroupModuleInstances(const Item::Module &module,
        const Item::Modules &dependentModules)
{
    if (!module.item->isPresentModule())
        return;

    // There are three cases:
    //     a) The defining item is the "main" module instance, i.e. the one instantiated in the
    //        product directly (or a parent group).
    //     b) The defining item refers to the module prototype (or the replacement of it
    //        created in the module merger [for products] or in this function [for parent groups]).
    //     c) The defining item is a different instance of the module, i.e. it was instantiated
    //        in some other module.

    QHash<Item *, Item *> definingItemReplacements;

    Item *modulePrototype = rootPrototype(module.item->prototype());
    QBS_CHECK(modulePrototype->type() == ItemType::Module
              || modulePrototype->type() == ItemType::Export);

    const Item::PropertyDeclarationMap &propDecls = modulePrototype->propertyDeclarations();
    for (const auto &decl : propDecls) {
        const QString &propName = decl.name();

        // Module properties assigned in the group are not relevant here, as nothing
        // gets inherited in that case. In particular, setting a list property
        // overwrites the value from the product's (or parent group's) instance completely,
        // rather than appending to it (concatenation happens via outer.concat()).
        ValueConstPtr propValue = module.item->ownProperty(propName);
        if (propValue)
            continue;

        // Find the nearest prototype instance that has the value assigned.
        // The result is either an instance of a parent group (or the parent group's
        // parent group and so on) or the instance of the product or the module prototype.
        // In the latter case, we don't have to do anything.
        const Item *instanceWithProperty = module.item;
        int prototypeChainLen = 0;
        do {
            instanceWithProperty = instanceWithProperty->prototype();
            QBS_CHECK(instanceWithProperty);
            ++prototypeChainLen;
            propValue = instanceWithProperty->ownProperty(propName);
        } while (!propValue);
        QBS_CHECK(propValue);

        if (propValue->type() != Value::JSSourceValueType)
            continue;

        bool hasDefiningItem = false;
        for (ValueConstPtr v = propValue; v && !hasDefiningItem; v = v->next())
            hasDefiningItem = v->definingItem();
        if (!hasDefiningItem)
            continue;

        const ValuePtr clonedValue = propValue->clone();
        for (ValuePtr v = clonedValue; v; v = v->next()) {
            QBS_CHECK(v->definingItem());

            Item *& replacement = definingItemReplacements[v->definingItem()];
            static const QString caseA = QStringLiteral("__group_case_a");
            if (v->definingItem() == instanceWithProperty
                    || v->definingItem()->variantProperty(caseA)) {
                // Case a)
                // For values whose defining item is the product's (or parent group's) instance,
                // we take its scope and replace references to module instances with those from the
                // group's instance. This handles cases like the following:
                // Product {
                //    name: "theProduct"
                //    aModule.listProp: [name, otherModule.stringProp]
                //    Group { name: "theGroup"; otherModule.stringProp: name }
                //    ...
                // }
                // In the above example, aModule.listProp is set to ["theProduct", "theGroup"]
                // (plus potential values from the prototype and other module instances,
                // which are different Value objects in the "next chain").
                if (!replacement) {
                    replacement = createReplacementForDefiningItem(v->definingItem(),
                                                                   v->definingItem()->type());
                    Item * const scope = Item::create(v->definingItem()->pool(), ItemType::Scope);
                    scope->setProperties(module.item->scope()->properties());
                    Item * const scopeScope
                            = Item::create(v->definingItem()->pool(), ItemType::Scope);
                    scopeScope->setProperties(v->definingItem()->scope()->scope()->properties());
                    scope->setScope(scopeScope);
                    replacement->setScope(scope);
                    const Item::PropertyMap &groupScopeProperties
                            = module.item->scope()->scope()->properties();
                    for (auto propIt = groupScopeProperties.begin();
                         propIt != groupScopeProperties.end(); ++propIt) {
                        if (propIt.value()->type() == Value::ItemValueType)
                            scopeScope->setProperty(propIt.key(), propIt.value());
                    }
                }
                replacement->setPropertyDeclaration(propName, decl);
                replacement->setProperty(propName, v);
                replacement->setProperty(caseA, VariantValue::invalidValue());
            }  else if (v->definingItem()->type() == ItemType::Module) {
                // Case b)
                // For values whose defining item is the module prototype, we change the scope to
                // the group's instance, analogous to what we do in
                // ModuleMerger::appendPrototypeValueToNextChain().
                QBS_CHECK(!decl.isScalar());
                QBS_CHECK(!v->next());
                Item *& replacement = definingItemReplacements[v->definingItem()];
                if (!replacement) {
                    replacement = createReplacementForDefiningItem(v->definingItem(),
                                                                   ItemType::Module);
                    replacement->setScope(module.item);
                }
                QBS_CHECK(!replacement->hasOwnProperty(caseA));
                qCDebug(lcModuleLoader).noquote().nospace()
                        << "replacing defining item for prototype; module is "
                        << module.name.toString() << module.item
                        << ", property is " << propName
                        << ", old defining item was " << v->definingItem()
                        << " with scope" << v->definingItem()->scope()
                        << ", new defining item is" << replacement
                        << " with scope" << replacement->scope();
                if (v->type() == Value::JSSourceValueType) {
                    qCDebug(lcModuleLoader) << "value source code is"
                        << std::static_pointer_cast<JSSourceValue>(v)->sourceCode().toString();
                }
                replacement->setPropertyDeclaration(propName, decl);
                replacement->setProperty(propName, v);
            } else {
                // Look for instance scopes of other module instances in defining items and
                // replace the affected values.
                // This is case c) as introduced above. See ModuleMerger::replaceItemInScopes()
                // for a detailed explanation.

                QBS_CHECK(v->definingItem()->scope() && v->definingItem()->scope()->scope());
                bool found = false;
                for (const Item::Module &depMod : dependentModules) {
                    const Item *depModPrototype = depMod.item->prototype();
                    for (int i = 1; i < prototypeChainLen; ++i)
                        depModPrototype = depModPrototype->prototype();
                    if (v->definingItem()->scope()->scope() != depModPrototype)
                        continue;

                    found = true;
                    Item *& replacement = definingItemReplacements[v->definingItem()];
                    if (!replacement) {
                        replacement = createReplacementForDefiningItem(v->definingItem(),
                                                                       v->definingItem()->type());
                        replacement->setProperties(v->definingItem()->properties());
                        for (const auto &decl : v->definingItem()->propertyDeclarations())
                            replacement->setPropertyDeclaration(decl.name(), decl);
                        replacement->setPrototype(v->definingItem()->prototype());
                        replacement->setScope(Item::create(v->definingItem()->pool(),
                                                           ItemType::Scope));
                        replacement->scope()->setScope(depMod.item);
                    }
                    QBS_CHECK(!replacement->hasOwnProperty(caseA));
                    qCDebug(lcModuleLoader) << "reset instance scope of module"
                            << depMod.name.toString() << "in property"
                            << propName << "of module" << module.name;
                }
                QBS_CHECK(found);
            }
            QBS_CHECK(replacement);
            v->setDefiningItem(replacement);
        }
        module.item->setProperty(propName, clonedValue);
    }
}

void ModuleLoader::resolveDependencies(DependsContext *dependsContext, Item *item,
                                       ProductContext *productContext)
{
    QBS_CHECK(m_dependencyResolvingPass == 1 || m_dependencyResolvingPass == 2);

    if (!productContext || m_dependencyResolvingPass == 1) {
        const Item::Module baseModule = loadBaseModule(dependsContext->product, item);
        item->addModule(baseModule);
    }

    // Resolve all Depends items.
    ItemModuleList loadedModules;
    QList<Item *> dependsItemPerLoadedModule;
    ProductDependencies productDependencies;
    const auto handleDependsItem = [&](Item *child) {
        if (child->type() != ItemType::Depends)
            return;

        int lastModulesCount = loadedModules.size();
        try {
            resolveDependsItem(dependsContext, child->parent(), child, &loadedModules,
                               &productDependencies);
        } catch (const ErrorInfo &e) {
            if (!productContext)
                throw;
            handleProductError(e, productContext);
        }
        for (int i = lastModulesCount; i < loadedModules.size(); ++i)
            dependsItemPerLoadedModule.push_back(child);

    };
    if (productContext && m_dependencyResolvingPass == 2) {
        for (const auto &deferData : productContext->deferredDependsItems) {
            dependsContext->exportingProductItem = deferData.first;
            for (Item * const dependsItem : deferData.second)
                handleDependsItem(dependsItem);
        }
    } else {
        for (Item * const child : item->children())
            handleDependsItem(child);
    }
    QBS_CHECK(loadedModules.size() == dependsItemPerLoadedModule.size());

    Item *lastDependsItem = nullptr;
    for (Item * const dependsItem : dependsItemPerLoadedModule) {
        if (dependsItem == lastDependsItem)
            continue;
        adjustParametersScopes(dependsItem, dependsItem);
        forwardParameterDeclarations(dependsItem, loadedModules);
        lastDependsItem = dependsItem;
    }

    for (int i = 0; i < loadedModules.size(); ++i) {
        Item::Module &module = loadedModules[i];
        mergeParameters(module.parameters, extractParameters(dependsItemPerLoadedModule.at(i)));
        item->addModule(module);

        const QString moduleName = module.name.toString();
        std::for_each(productDependencies.begin(), productDependencies.end(),
                      [&module, &moduleName] (ModuleLoaderResult::ProductInfo::Dependency &dep) {
            if (dep.name == moduleName)
                dep.parameters = module.parameters;
        });
    }

    dependsContext->productDependencies->insert(
                dependsContext->productDependencies->end(),
                productDependencies.cbegin(), productDependencies.cend());
}

class RequiredChainManager
{
public:
    RequiredChainManager(std::vector<bool> &requiredChain, bool required)
        : m_requiredChain(requiredChain)
    {
        m_requiredChain.push_back(required);
    }

    ~RequiredChainManager() { m_requiredChain.pop_back(); }

private:
    std::vector<bool> &m_requiredChain;
};

void ModuleLoader::resolveDependsItem(DependsContext *dependsContext, Item *parentItem,
        Item *dependsItem, ItemModuleList *moduleResults,
        ProductDependencies *productResults)
{
    checkCancelation();
    if (!checkItemCondition(dependsItem)) {
        qCDebug(lcModuleLoader) << "Depends item disabled, ignoring.";
        return;
    }
    bool nameIsSet;
    const QString name = m_evaluator->stringValue(dependsItem, StringConstants::nameProperty(),
                                                  QString(), &nameIsSet);
    bool submodulesPropertySet;
    const QStringList submodules = m_evaluator->stringListValue(
                dependsItem, StringConstants::submodulesProperty(), &submodulesPropertySet);
    if (submodules.empty() && submodulesPropertySet) {
        qCDebug(lcModuleLoader) << "Ignoring Depends item with empty submodules list.";
        return;
    }
    if (Q_UNLIKELY(submodules.size() > 1 && !dependsItem->id().isEmpty())) {
        QString msg = Tr::tr("A Depends item with more than one module cannot have an id.");
        throw ErrorInfo(msg, dependsItem->location());
    }
    const FallbackMode fallbackMode = m_parameters.fallbackProviderEnabled()
            && m_evaluator->boolValue(dependsItem, StringConstants::enableFallbackProperty())
            ? FallbackMode::Enabled : FallbackMode::Disabled;

    QList<QualifiedId> moduleNames;
    const QualifiedId nameParts = QualifiedId::fromString(name);
    if (submodules.empty()) {
        // Ignore explicit dependencies on the base module, which has already been loaded.
        if (name == StringConstants::qbsModule())
            return;

        moduleNames << nameParts;
    } else {
        for (const QString &submodule : submodules)
            moduleNames << nameParts + QualifiedId::fromString(submodule);
    }

    Item::Module result;
    bool productTypesIsSet;
    m_evaluator->stringValue(dependsItem, StringConstants::productTypesProperty(),
                             QString(), &productTypesIsSet);
    if (m_dependencyResolvingPass == 1 && productTypesIsSet) {
        qCDebug(lcModuleLoader) << "queuing product" << dependsContext->product->name
                                << "for a second dependencies resolving pass";
        m_productsWithDeferredDependsItems[dependsContext->product].insert(
                    DeferredDependsContext(dependsContext->exportingProductItem, parentItem));
        return;
    }

    const bool isRequired = !productTypesIsSet
            && m_evaluator->boolValue(dependsItem, StringConstants::requiredProperty())
            && !contains(m_requiredChain, false);
    const Version minVersion = Version::fromString(
                m_evaluator->stringValue(dependsItem,
                                         StringConstants::versionAtLeastProperty()));
    const Version maxVersion = Version::fromString(
                m_evaluator->stringValue(dependsItem, StringConstants::versionBelowProperty()));
    const VersionRange versionRange(minVersion, maxVersion);
    QStringList multiplexConfigurationIds = m_evaluator->stringListValue(
                dependsItem,
                StringConstants::multiplexConfigurationIdsProperty());
    if (multiplexConfigurationIds.empty())
        multiplexConfigurationIds << QString();

    for (const QualifiedId &moduleName : qAsConst(moduleNames)) {
        // Don't load the same module twice. Duplicate Depends statements can easily
        // happen due to inheritance.
        const auto it = std::find_if(moduleResults->begin(), moduleResults->end(),
                [moduleName](const Item::Module &m) { return m.name == moduleName; });
        if (it != moduleResults->end()) {
            if (isRequired)
                it->required = true;
            it->versionRange.narrowDown(versionRange);
            continue;
        }

        QVariantMap defaultParameters;
        Item *moduleItem = loadModule(dependsContext->product, dependsContext->exportingProductItem,
                                      parentItem, dependsItem->location(), dependsItem->id(),
                                      moduleName, multiplexConfigurationIds.first(), fallbackMode,
                                      isRequired, &result.isProduct, &defaultParameters);
        if (!moduleItem) {
            const QString productName = ResolvedProduct::fullDisplayName(
                        dependsContext->product->name,
                        dependsContext->product->multiplexConfigurationId);
            if (!multiplexConfigurationIds.first().isEmpty()) {
                const QString depName = ResolvedProduct::fullDisplayName(
                            moduleName.toString(), multiplexConfigurationIds.first());
                throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' not "
                                       "fulfilled.").arg(productName, depName));
            }
            ErrorInfo e(Tr::tr("Dependency '%1' not found for product '%2'.")
                        .arg(moduleName.toString(), productName), dependsItem->location());
            throw e;
        }
        if (result.isProduct && !m_dependsChain.empty() && !m_dependsChain.back().isProduct) {
            throw ErrorInfo(Tr::tr("Invalid dependency on product '%1': Modules cannot depend on "
                                   "products. You may want to turn your module into a product and "
                                   "add the dependency in that product's Export item.")
                            .arg(moduleName.toString()), dependsItem->location());
        }
        qCDebug(lcModuleLoader) << "module loaded:" << moduleName.toString();
        result.name = moduleName;
        result.item = moduleItem;
        result.required = isRequired;
        result.parameters = defaultParameters;
        result.versionRange = versionRange;
        moduleResults->push_back(result);
        if (result.isProduct) {
            qCDebug(lcModuleLoader) << "product dependency loaded:" << moduleName.toString();
            bool profilesPropertyWasSet = false;
            QStringList profiles = m_evaluator->stringListValue(dependsItem,
                                                                StringConstants::profilesProperty(),
                                                                &profilesPropertyWasSet);
            if (profiles.empty()) {
                if (profilesPropertyWasSet)
                    profiles.push_back(StringConstants::star());
                else
                    profiles.push_back(QString());
            }
            for (const QString &profile : qAsConst(profiles)) {
                for (const QString &multiplexId : qAsConst(multiplexConfigurationIds)) {
                    ModuleLoaderResult::ProductInfo::Dependency dependency;
                    dependency.name = moduleName.toString();
                    dependency.profile = profile;
                    dependency.multiplexConfigurationId = multiplexId;
                    dependency.isRequired = isRequired;
                    productResults->push_back(dependency);
                }
            }
        }
    }
}

void ModuleLoader::forwardParameterDeclarations(const Item *dependsItem,
                                                const ItemModuleList &modules)
{
    for (auto it = dependsItem->properties().begin(); it != dependsItem->properties().end(); ++it) {
        if (it.value()->type() != Value::ItemValueType)
            continue;
        forwardParameterDeclarations(it.key(),
                                     std::static_pointer_cast<ItemValue>(it.value())->item(),
                                     modules);
    }
}

void ModuleLoader::forwardParameterDeclarations(const QualifiedId &moduleName, Item *item,
                                                const ItemModuleList &modules)
{
    auto it = std::find_if(modules.begin(), modules.end(), [&moduleName] (const Item::Module &m) {
        return m.name == moduleName;
    });
    if (it != modules.end()) {
        item->setPropertyDeclarations(m_parameterDeclarations.value(rootPrototype(it->item)));
    } else {
        for (auto it = item->properties().begin(); it != item->properties().end(); ++it) {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            forwardParameterDeclarations(QualifiedId(moduleName) << it.key(),
                                         std::static_pointer_cast<ItemValue>(it.value())->item(),
                                         modules);
        }
    }
}

void ModuleLoader::resolveParameterDeclarations(const Item *module)
{
    Item::PropertyDeclarationMap decls;
    const auto &moduleChildren = module->children();
    for (Item *param : moduleChildren) {
        if (param->type() != ItemType::Parameter)
            continue;
        const auto &paramDecls = param->propertyDeclarations();
        for (auto it = paramDecls.begin(); it != paramDecls.end(); ++it)
            decls.insert(it.key(), it.value());
    }
    m_parameterDeclarations.insert(module, decls);
}

static bool isItemValue(const ValuePtr &v)
{
    return v->type() == Value::ItemValueType;
}

static Item::PropertyMap filterItemProperties(const Item::PropertyMap &properties)
{
    Item::PropertyMap result;
    auto itEnd = properties.end();
    for (auto it = properties.begin(); it != itEnd; ++it) {
        if (isItemValue(it.value()))
            result.insert(it.key(), it.value());
    }
    return result;
}

static QVariantMap safeToVariant(const QScriptValue &v)
{
    QVariantMap result;
    QScriptValueIterator it(v);
    while (it.hasNext()) {
        it.next();
        QScriptValue u = it.value();
        if (u.isError())
            throw ErrorInfo(u.toString());
        result[it.name()] = (u.isObject() && !u.isArray() && !u.isRegExp())
                ? safeToVariant(u) : it.value().toVariant();
    }
    return result;
}

QVariantMap ModuleLoader::extractParameters(Item *dependsItem) const
{
    QVariantMap result;
    const Item::PropertyMap &itemProperties = filterItemProperties(
                rootPrototype(dependsItem)->properties());
    if (itemProperties.empty())
        return result;

    auto origProperties = dependsItem->properties();
    dependsItem->setProperties(itemProperties);
    QScriptValue sv = m_evaluator->scriptValue(dependsItem);
    try {
        result = safeToVariant(sv);
    } catch (const ErrorInfo &exception) {
        auto ei = exception;
        ei.prepend(Tr::tr("Error in dependency parameter."), dependsItem->location());
        throw ei;
    }
    dependsItem->setProperties(origProperties);
    return result;
}

[[noreturn]] static void throwModuleNamePrefixError(const QualifiedId &shortName,
        const QualifiedId &longName, const CodeLocation &codeLocation)
{
    throw ErrorInfo(Tr::tr("The name of module '%1' is equal to the first component of the "
                           "name of module '%2', which is not allowed")
                    .arg(shortName.toString(), longName.toString()), codeLocation);
}

Item *ModuleLoader::moduleInstanceItem(Item *containerItem, const QualifiedId &moduleName)
{
    QBS_CHECK(!moduleName.empty());
    Item *instance = containerItem;
    for (int i = 0; i < moduleName.size(); ++i) {
        const QString &moduleNameSegment = moduleName.at(i);
        const ValuePtr v = instance->ownProperty(moduleName.at(i));
        if (v && v->type() == Value::ItemValueType) {
            instance = std::static_pointer_cast<ItemValue>(v)->item();
        } else {
            const ItemType itemType = i < moduleName.size() - 1 ? ItemType::ModulePrefix
                                                                : ItemType::ModuleInstance;
            auto newItem = Item::create(m_pool, itemType);
            instance->setProperty(moduleNameSegment, ItemValue::create(newItem));
            instance = newItem;
        }
        if (i < moduleName.size() - 1) {
            if (instance->type() == ItemType::ModuleInstance) {
                QualifiedId conflictingName = QStringList(moduleName.mid(0, i + 1));
                throwModuleNamePrefixError(conflictingName, moduleName, CodeLocation());
            }
            QBS_CHECK(instance->type() == ItemType::ModulePrefix);
        }
    }
    QBS_CHECK(instance != containerItem);
    return instance;
}

ModuleLoader::ProductModuleInfo *ModuleLoader::productModule(ProductContext *productContext,
        const QString &name, const QString &multiplexId, bool &productNameMatch)
{
    auto &exportsData = productContext->project->topLevelProject->productModules;
    const auto firstIt = exportsData.find(name);
    productNameMatch = firstIt != exportsData.end();
    for (auto it = firstIt; it != exportsData.end() && it.key() == name; ++it) {
        if (it.value().multiplexId == multiplexId)
            return &it.value();
    }
    if (multiplexId.isEmpty() && firstIt != exportsData.end())
        return &firstIt.value();
    return nullptr;
}

ModuleLoader::ProductContext *ModuleLoader::product(ProjectContext *projectContext,
                                                    const QString &name)
{
    auto itEnd = projectContext->products.end();
    auto it = std::find_if(projectContext->products.begin(), itEnd,
                           [&name] (const ProductContext &ctx) {
        return ctx.name == name;
    });
    return it == itEnd ? nullptr : &*it;
}

ModuleLoader::ProductContext *ModuleLoader::product(TopLevelProjectContext *tlpContext,
                                                    const QString &name)
{
    ProductContext *result = nullptr;
    for (auto prj : tlpContext->projects) {
        result = product(prj, name);
        if (result)
            break;
    }
    return result;
}

class ModuleLoader::DependsChainManager
{
public:
    DependsChainManager(std::vector<DependsChainEntry> &dependsChain, const QualifiedId &module,
                        const CodeLocation &dependsLocation)
        : m_dependsChain(dependsChain)
    {
        const bool alreadyInChain = std::any_of(dependsChain.cbegin(), dependsChain.cend(),
                                                [&module](const DependsChainEntry &e) {
            return e.name == module;
        });
        if (alreadyInChain) {
            ErrorInfo error;
            error.append(Tr::tr("Cyclic dependencies detected:"));
            for (const DependsChainEntry &e : qAsConst(m_dependsChain))
                error.append(e.name.toString(), e.location);
            error.append(module.toString(), dependsLocation);
            throw error;
        }
        m_dependsChain.emplace_back(module, dependsLocation);
    }

    ~DependsChainManager() { m_dependsChain.pop_back(); }

private:
    std::vector<DependsChainEntry> &m_dependsChain;
};

static bool isBaseModule(const QualifiedId &moduleName)
{
    return moduleName.size() == 1 && moduleName.front() == StringConstants::qbsModule();
}

class DelayedPropertyChanger
{
public:
    ~DelayedPropertyChanger()
    {
        applyNow();
    }

    void setLater(Item *item, const QString &name, const ValuePtr &value)
    {
        QBS_CHECK(m_item == nullptr);
        m_item = item;
        m_name = name;
        m_value = value;
    }

    void removeLater(Item *item, const QString &name)
    {
        QBS_CHECK(m_item == nullptr);
        m_item = item;
        m_name = name;
    }

    void applyNow()
    {
        if (!m_item || m_name.isEmpty())
            return;
        if (m_value)
            m_item->setProperty(m_name, m_value);
        else
            m_item->removeProperty(m_name);
        m_item = nullptr;
        m_name.clear();
        m_value.reset();
    }

private:
    Item *m_item = nullptr;
    QString m_name;
    ValuePtr m_value;
};

Item *ModuleLoader::loadModule(ProductContext *productContext, Item *exportingProductItem,
                               Item *item, const CodeLocation &dependsItemLocation,
                               const QString &moduleId, const QualifiedId &moduleName,
                               const QString &multiplexId, FallbackMode fallbackMode,
                               bool isRequired, bool *isProductDependency,
                               QVariantMap *defaultParameters)
{
    qCDebug(lcModuleLoader) << "loadModule name:" << moduleName.toString() << "id:" << moduleId;

    RequiredChainManager requiredChainManager(m_requiredChain, isRequired);
    DependsChainManager dependsChainManager(m_dependsChain, moduleName, dependsItemLocation);

    Item *moduleInstance = moduleId.isEmpty()
            ? moduleInstanceItem(item, moduleName)
            : moduleInstanceItem(item, QStringList(moduleId));
    if (moduleInstance->scope())
        return moduleInstance; // already handled

    if (Q_UNLIKELY(moduleInstance->type() == ItemType::ModulePrefix)) {
        for (const Item::Module &m : item->modules()) {
            if (m.name.front() == moduleName.front())
                throwModuleNamePrefixError(moduleName, m.name, dependsItemLocation);
        }
    }
    QBS_CHECK(moduleInstance->type() == ItemType::ModuleInstance);

    // Prepare module instance for evaluating Module.condition.
    DelayedPropertyChanger delayedPropertyChanger;
    const QString &qbsModuleName = StringConstants::qbsModule();
    if (!isBaseModule(moduleName)) {
        ItemValuePtr qbsProp = productContext->item->itemProperty(qbsModuleName);
        if (qbsProp) {
            ValuePtr qbsModuleValue = moduleInstance->ownProperty(qbsModuleName);
            if (qbsModuleValue)
                delayedPropertyChanger.setLater(moduleInstance, qbsModuleName, qbsModuleValue);
            else
                delayedPropertyChanger.removeLater(moduleInstance, qbsModuleName);
            moduleInstance->setProperty(qbsModuleName, qbsProp);
        }
    }

    Item *modulePrototype = nullptr;
    ProductModuleInfo * const pmi = productModule(productContext, moduleName.toString(),
                                                  multiplexId, *isProductDependency);
    if (pmi) {
        m_dependsChain.back().isProduct = true;
        modulePrototype = pmi->exportItem;
        if (defaultParameters)
            *defaultParameters = pmi->defaultParameters;
    } else if (!*isProductDependency) {
        modulePrototype = searchAndLoadModuleFile(productContext, dependsItemLocation,
                moduleName, fallbackMode, isRequired, moduleInstance);
    }
    delayedPropertyChanger.applyNow();
    if (!modulePrototype)
        return nullptr;

    instantiateModule(productContext, exportingProductItem, item, moduleInstance, modulePrototype,
                      moduleName, pmi);
    return moduleInstance;
}

struct PrioritizedItem
{
    PrioritizedItem(Item *item, int priority, int searchPathIndex)
        : item(item), priority(priority), searchPathIndex(searchPathIndex)
    {
    }

    Item *item = nullptr;
    int priority = 0;
    int searchPathIndex = 0;
};

static Item *chooseModuleCandidate(const std::vector<PrioritizedItem> &candidates,
                                   const QString &moduleName)
{
    auto maxIt = std::max_element(candidates.begin(), candidates.end(),
            [] (const PrioritizedItem &a, const PrioritizedItem &b) {
        if (a.priority < b.priority)
            return true;
        if (a.priority > b.priority)
            return false;
        return a.searchPathIndex > b.searchPathIndex;
    });

    size_t nmax = std::count_if(candidates.begin(), candidates.end(),
            [maxIt] (const PrioritizedItem &i) {
        return i.priority == maxIt->priority && i.searchPathIndex == maxIt->searchPathIndex;
    });

    if (nmax > 1) {
        ErrorInfo e(Tr::tr("There is more than one equally prioritized candidate for module '%1'.")
                    .arg(moduleName));
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto candidate = candidates.at(i);
            if (candidate.priority == maxIt->priority) {
                //: The %1 denotes the number of the candidate.
                e.append(Tr::tr("candidate %1").arg(i + 1), candidates.at(i).item->location());
            }
        }
        throw e;
    }

    return maxIt->item;
}

Item *ModuleLoader::searchAndLoadModuleFile(ProductContext *productContext,
        const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
        FallbackMode fallbackMode, bool isRequired, Item *moduleInstance)
{
    auto existingPaths = findExistingModulePaths(m_reader->allSearchPaths(), moduleName);

    if (existingPaths.isEmpty()) { // no suitable names found, try to use providers
        bool moduleAlreadyKnown = false;
        ModuleProviderResult result;
        for (QualifiedId providerName = moduleName; !providerName.empty();
             providerName.pop_back()) {
            if (!productContext->knownModuleProviders.insert(providerName).second) {
                moduleAlreadyKnown = true;
                break;
            }
            qCDebug(lcModuleLoader) << "Module" << moduleName.toString()
                                    << "not found, checking for module providers";
            result = findModuleProvider(providerName, *productContext,
                                        ModuleProviderLookup::Regular, dependsItemLocation);
            if (result.providerFound)
                break;
        }
        if (fallbackMode == FallbackMode::Enabled && !result.providerFound
                && !moduleAlreadyKnown) {
            qCDebug(lcModuleLoader) << "Specific module provider not found for"
                                    << moduleName.toString()  << ", setting up fallback.";
            result = findModuleProvider(moduleName, *productContext,
                                        ModuleProviderLookup::Fallback, dependsItemLocation);
        }
        if (result.providerAddedSearchPaths) {
            qCDebug(lcModuleLoader) << "Re-checking for module" << moduleName.toString()
                                    << "with newly added search paths from module provider";
            existingPaths = findExistingModulePaths(m_reader->allSearchPaths(), moduleName);
        }
    }

    const QString fullName = moduleName.toString();
    bool triedToLoadModule = false;
    std::vector<PrioritizedItem> candidates;
    candidates.reserve(size_t(existingPaths.size()));
    for (int i = 0; i < existingPaths.size(); ++i) {
        const QString &dirPath = existingPaths.at(i);
        QStringList &moduleFileNames = getModuleFileNames(dirPath);
        for (auto it = moduleFileNames.begin(); it != moduleFileNames.end(); ) {
            const QString &filePath = *it;
            bool triedToLoad = true;
            Item *module = loadModuleFile(productContext, fullName, isBaseModule(moduleName),
                                          filePath, &triedToLoad, moduleInstance);
            if (module)
                candidates.emplace_back(module, 0, i);
            if (!triedToLoad)
                it = moduleFileNames.erase(it);
            else
                ++it;
            triedToLoadModule = triedToLoadModule || triedToLoad;
        }
    }

    if (candidates.empty()) {
        if (!isRequired)
            return createNonPresentModule(fullName, QStringLiteral("not found"), nullptr);
        if (Q_UNLIKELY(triedToLoadModule)) {
            throw ErrorInfo(Tr::tr("Module %1 could not be loaded.").arg(fullName),
                        dependsItemLocation);
        }
        return nullptr;
    }

    Item *moduleItem;
    if (candidates.size() == 1) {
        moduleItem = candidates.at(0).item;
    } else {
        for (auto &candidate : candidates) {
            candidate.priority = m_evaluator->intValue(candidate.item,
                                                       StringConstants::priorityProperty(),
                                                       candidate.priority);
        }
        moduleItem = chooseModuleCandidate(candidates, fullName);
    }

    const auto it = productContext->unknownProfilePropertyErrors.find(moduleItem);
    if (it != productContext->unknownProfilePropertyErrors.cend()) {
        const QString fullProductName = ResolvedProduct::fullDisplayName
                (productContext->name, productContext->multiplexConfigurationId);
        ErrorInfo error(Tr::tr("Loading module '%1' for product '%2' failed due to invalid values "
                               "in profile '%3':").arg(fullName, fullProductName,
                                                       productContext->profileName));
        for (const ErrorInfo &e : it->second)
            error.append(e.toString());
        handlePropertyError(error, m_parameters, m_logger);
    }
    return moduleItem;
}

QStringList &ModuleLoader::getModuleFileNames(const QString &dirPath)
{
    QStringList &moduleFileNames = m_moduleDirListCache[dirPath];
    if (moduleFileNames.empty()) {
        QDirIterator dirIter(dirPath, StringConstants::qbsFileWildcards());
        while (dirIter.hasNext())
            moduleFileNames += dirIter.next();
    }
    return moduleFileNames;
}

// returns QVariant::Invalid for types that do not need conversion
static QVariant::Type variantType(PropertyDeclaration::Type t)
{
    switch (t) {
    case PropertyDeclaration::UnknownType:
        break;
    case PropertyDeclaration::Boolean:
        return QVariant::Bool;
    case PropertyDeclaration::Integer:
        return QVariant::Int;
    case PropertyDeclaration::Path:
        return QVariant::String;
    case PropertyDeclaration::PathList:
        return QVariant::StringList;
    case PropertyDeclaration::String:
        return QVariant::String;
    case PropertyDeclaration::StringList:
        return QVariant::StringList;
    case PropertyDeclaration::VariantList:
        return QVariant::List;
    case PropertyDeclaration::Variant:
        break;
    }
    return QVariant::Invalid;
}

static QVariant convertToPropertyType(const QVariant &v, PropertyDeclaration::Type t,
    const QStringList &namePrefix, const QString &key)
{
    if (v.isNull() || !v.isValid())
        return v;
    const QVariant::Type vt = variantType(t);
    if (vt == QVariant::Invalid)
        return v;

    // Handle the foo,bar,bla stringlist syntax.
    if (t == PropertyDeclaration::StringList && v.type() == QVariant::String)
        return v.toString().split(QLatin1Char(','));

    QVariant c = v;
    if (!c.convert(vt)) {
        QStringList name = namePrefix;
        name << key;
        throw ErrorInfo(Tr::tr("Value '%1' of property '%2' has incompatible type.")
                        .arg(v.toString(), name.join(QLatin1Char('.'))));
    }
    return c;
}

static Item *findDeepestModuleInstance(Item *instance)
{
    while (instance->prototype() && instance->prototype()->type() == ItemType::ModuleInstance)
        instance = instance->prototype();
    return instance;
}

Item *ModuleLoader::loadModuleFile(ProductContext *productContext, const QString &fullModuleName,
        bool isBaseModule, const QString &filePath, bool *triedToLoad, Item *moduleInstance)
{
    checkCancelation();

    qCDebug(lcModuleLoader) << "loadModuleFile" << fullModuleName << "from" << filePath;

    Item * const module = getModulePrototype(productContext, fullModuleName, filePath, triedToLoad);
    if (!module)
        return nullptr;

    const auto key = std::make_pair(module, productContext);
    const auto it = m_modulePrototypeEnabledInfo.find(key);
    if (it != m_modulePrototypeEnabledInfo.end()) {
        qCDebug(lcModuleLoader) << "prototype cache hit (level 2)";
        return it.value() ? module : nullptr;
    }

    // Set the name before evaluating any properties. EvaluatorScriptClass reads the module name.
    module->setProperty(StringConstants::nameProperty(), VariantValue::create(fullModuleName));

    Item *deepestModuleInstance = findDeepestModuleInstance(moduleInstance);
    Item *origDeepestModuleInstancePrototype = deepestModuleInstance->prototype();
    deepestModuleInstance->setPrototype(module);
    bool enabled = checkItemCondition(moduleInstance, module);
    deepestModuleInstance->setPrototype(origDeepestModuleInstancePrototype);
    if (!enabled) {
        qCDebug(lcModuleLoader) << "condition of module" << fullModuleName << "is false";
        m_modulePrototypeEnabledInfo.insert(key, false);
        return nullptr;
    }

    if (isBaseModule)
        setupBaseModulePrototype(module);
    else
        resolveParameterDeclarations(module);

    m_modulePrototypeEnabledInfo.insert(key, true);
    return module;
}

Item *ModuleLoader::getModulePrototype(ProductContext *productContext,
        const QString &fullModuleName, const QString &filePath, bool *triedToLoad)
{
    auto &prototypeList = m_modulePrototypes[filePath];
    for (const auto &prototype : prototypeList) {
        if (prototype.second == productContext->profileName) {
            qCDebug(lcModuleLoader) << "prototype cache hit (level 1)";
            return prototype.first;
        }
    }
    Item * const module = loadItemFromFile(filePath, CodeLocation());
    prototypeList.emplace_back(module, productContext->profileName);
    if (module->type() != ItemType::Module) {
        qCDebug(lcModuleLoader).nospace()
                            << "Alleged module " << fullModuleName << " has type '"
                            << module->typeName() << "', so it's not a module after all.";
        *triedToLoad = false;
        return nullptr;
    }

    // Module properties that are defined in the profile are used as default values.
    // This is the reason we need to have different items per profile.
    const QVariantMap profileModuleProperties
            = productContext->moduleProperties.value(fullModuleName).toMap();
    for (QVariantMap::const_iterator vmit = profileModuleProperties.begin();
            vmit != profileModuleProperties.end(); ++vmit)
    {
        if (Q_UNLIKELY(!module->hasProperty(vmit.key()))) {
            productContext->unknownProfilePropertyErrors[module].emplace_back
                    (Tr::tr("Unknown property: %1.%2").arg(fullModuleName, vmit.key()));
            continue;
        }
        const PropertyDeclaration decl = module->propertyDeclaration(vmit.key());
        VariantValuePtr v = VariantValue::create(convertToPropertyType(vmit.value(), decl.type(),
                QStringList(fullModuleName), vmit.key()));
        module->setProperty(vmit.key(), v);
    }

    return module;
}

Item::Module ModuleLoader::loadBaseModule(ProductContext *productContext, Item *item)
{
    const QualifiedId baseModuleName(StringConstants::qbsModule());
    Item::Module baseModuleDesc;
    baseModuleDesc.name = baseModuleName;
    baseModuleDesc.item = loadModule(productContext, nullptr, item, CodeLocation(), QString(),
                                     baseModuleName, QString(), FallbackMode::Disabled, true,
                                     &baseModuleDesc.isProduct, nullptr);
    if (productContext->item) {
        const Item * const qbsInstanceItem
                = moduleInstanceItem(productContext->item, baseModuleName);
        const Item::PropertyMap &props = qbsInstanceItem->properties();
        for (auto it = props.cbegin(); it != props.cend(); ++it) {
            if (it.value()->type() == Value::VariantValueType)
                baseModuleDesc.item->setProperty(it.key(), it.value());
        }
    }
    QBS_CHECK(!baseModuleDesc.isProduct);
    if (Q_UNLIKELY(!baseModuleDesc.item))
        throw ErrorInfo(Tr::tr("Cannot load base qbs module."));
    return baseModuleDesc;
}

void ModuleLoader::setupBaseModulePrototype(Item *prototype)
{
    prototype->setProperty(QStringLiteral("hostPlatform"),
                           VariantValue::create(QString::fromStdString(
                                                    HostOsInfo::hostOSIdentifier())));
    prototype->setProperty(QStringLiteral("hostArchitecture"),
                           VariantValue::create(QString::fromStdString(
                                                    HostOsInfo::hostOSArchitecture())));
    prototype->setProperty(QStringLiteral("libexecPath"),
                           VariantValue::create(m_parameters.libexecPath()));

    const Version qbsVersion = LanguageInfo::qbsVersion();
    prototype->setProperty(QStringLiteral("versionMajor"),
                           VariantValue::create(qbsVersion.majorVersion()));
    prototype->setProperty(QStringLiteral("versionMinor"),
                           VariantValue::create(qbsVersion.minorVersion()));
    prototype->setProperty(QStringLiteral("versionPatch"),
                           VariantValue::create(qbsVersion.patchLevel()));
}

static void collectItemsWithId_impl(Item *item, QList<Item *> *result)
{
    if (!item->id().isEmpty())
        result->push_back(item);
    for (Item * const child : item->children())
        collectItemsWithId_impl(child, result);
}

static QList<Item *> collectItemsWithId(Item *item)
{
    QList<Item *> result;
    collectItemsWithId_impl(item, &result);
    return result;
}

static std::vector<std::pair<QualifiedId, ItemValuePtr>> instanceItemProperties(Item *item)
{
    std::vector<std::pair<QualifiedId, ItemValuePtr>> result;
    QualifiedId name;
    std::function<void(Item *)> f = [&] (Item *item) {
        for (auto it = item->properties().begin(); it != item->properties().end(); ++it) {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            ItemValuePtr itemValue = std::static_pointer_cast<ItemValue>(it.value());
            if (!itemValue->item())
                continue;
            name.push_back(it.key());
            if (itemValue->item()->type() == ItemType::ModulePrefix)
                f(itemValue->item());
            else
                result.emplace_back(name, itemValue);
            name.removeLast();
        }
    };
    f(item);
    return result;
}

void ModuleLoader::instantiateModule(ProductContext *productContext, Item *exportingProduct,
        Item *instanceScope, Item *moduleInstance, Item *modulePrototype,
        const QualifiedId &moduleName, ProductModuleInfo *productModuleInfo)
{
    Item *deepestModuleInstance = findDeepestModuleInstance(moduleInstance);
    deepestModuleInstance->setPrototype(modulePrototype);
    const QString fullName = moduleName.toString();
    const QString generalOverrideKey = QStringLiteral("modules.") + fullName;
    const QString perProductOverrideKey = StringConstants::productsOverridePrefix()
            + productContext->name + QLatin1Char('.') + fullName;
    for (Item *instance = moduleInstance; instance; instance = instance->prototype()) {
        overrideItemProperties(instance, generalOverrideKey, m_parameters.overriddenValuesTree());
        if (fullName == QStringLiteral("qbs"))
            overrideItemProperties(instance, fullName, m_parameters.overriddenValuesTree());
        overrideItemProperties(instance, perProductOverrideKey,
                               m_parameters.overriddenValuesTree());
        if (instance == deepestModuleInstance)
            break;
    }

    moduleInstance->setFile(modulePrototype->file());
    moduleInstance->setLocation(modulePrototype->location());
    QBS_CHECK(moduleInstance->type() == ItemType::ModuleInstance);

    // create module scope
    Item *moduleScope = Item::create(m_pool, ItemType::Scope);
    QBS_CHECK(instanceScope->file());
    moduleScope->setFile(instanceScope->file());
    moduleScope->setScope(instanceScope);
    QBS_CHECK(productContext->project->scope);
    productContext->project->scope->copyProperty(StringConstants::projectVar(), moduleScope);
    if (productContext->scope)
        productContext->scope->copyProperty(StringConstants::productVar(), moduleScope);
    else
        QBS_CHECK(fullName == StringConstants::qbsModule()); // Dummy product.

    if (productModuleInfo) {
        exportingProduct = productModuleInfo->exportItem->parent();
        QBS_CHECK(exportingProduct);
        QBS_CHECK(exportingProduct->type() == ItemType::Product);
    }

    if (exportingProduct) {
        // TODO: For consistency with modules, it should be the other way around, i.e.
        //       "exportingProduct" and just "product".
        moduleScope->setProperty(StringConstants::productVar(),
                                 ItemValue::create(exportingProduct));
        moduleScope->setProperty(QStringLiteral("importingProduct"),
                                 ItemValue::create(productContext->item));

        moduleScope->setProperty(StringConstants::projectVar(),
                                 ItemValue::create(exportingProduct->parent()));

        PropertyDeclaration pd(StringConstants::qbsSourceDirPropertyInternal(),
                               PropertyDeclaration::String, QString(),
                               PropertyDeclaration::PropertyNotAvailableInConfig);
        moduleInstance->setPropertyDeclaration(pd.name(), pd);
        ValuePtr v = exportingProduct
                ->property(StringConstants::sourceDirectoryProperty())->clone();
        moduleInstance->setProperty(pd.name(), v);
    }
    moduleInstance->setScope(moduleScope);

    QHash<Item *, Item *> prototypeInstanceMap;
    prototypeInstanceMap[modulePrototype] = moduleInstance;

    // create instances for every child of the prototype
    createChildInstances(moduleInstance, modulePrototype, &prototypeInstanceMap);

    // create ids from from the prototype in the instance
    if (modulePrototype->file()->idScope()) {
        const auto items = collectItemsWithId(modulePrototype);
        for (Item * const itemWithId : items) {
            Item *idProto = itemWithId;
            Item *idInstance = prototypeInstanceMap.value(idProto);
            QBS_ASSERT(idInstance, continue);
            ItemValuePtr idInstanceValue = ItemValue::create(idInstance);
            moduleScope->setProperty(itemWithId->id(), idInstanceValue);
        }
    }

    // For foo.bar in modulePrototype create an item foo in moduleInstance.
    for (const auto &iip : instanceItemProperties(modulePrototype)) {
        if (iip.second->item()->properties().empty())
            continue;
        qCDebug(lcModuleLoader) << "The prototype of " << moduleName
                            << " sets properties on " << iip.first.toString();
        Item *item = moduleInstanceItem(moduleInstance, iip.first);
        item->setPrototype(iip.second->item());
        if (iip.second->createdByPropertiesBlock()) {
            ItemValuePtr itemValue = moduleInstance->itemProperty(iip.first.front());
            for (int i = 1; i < iip.first.size(); ++i)
                itemValue = itemValue->item()->itemProperty(iip.first.at(i));
            itemValue->setCreatedByPropertiesBlock(true);
        }
    }

    // Resolve dependencies of this module instance.
    DependsContext dependsContext;
    dependsContext.product = productContext;
    dependsContext.exportingProductItem = exportingProduct;
    QBS_ASSERT(moduleInstance->modules().empty(), moduleInstance->removeModules());
    if (productModuleInfo) {
        dependsContext.productDependencies = &productContext->productModuleDependencies[fullName];
        resolveDependencies(&dependsContext, moduleInstance);
    } else if (!isBaseModule(moduleName)) {
        dependsContext.productDependencies = &productContext->info.usedProducts;
        resolveDependencies(&dependsContext, moduleInstance);
    }

    // Check readonly properties.
    const auto end = moduleInstance->properties().cend();
    for (auto it = moduleInstance->properties().cbegin(); it != end; ++it) {
        const PropertyDeclaration &pd = moduleInstance->propertyDeclaration(it.key());
        if (!pd.flags().testFlag(PropertyDeclaration::ReadOnlyFlag))
            continue;
        throw ErrorInfo(Tr::tr("Cannot set read-only property '%1'.").arg(pd.name()),
                        moduleInstance->property(pd.name())->location());
    }
}

void ModuleLoader::createChildInstances(Item *instance, Item *prototype,
                                        QHash<Item *, Item *> *prototypeInstanceMap) const
{
    instance->childrenReserve(instance->children().size() + prototype->children().size());

    for (Item * const childPrototype : prototype->children()) {
        Item *childInstance = Item::create(m_pool, childPrototype->type());
        prototypeInstanceMap->insert(childPrototype, childInstance);
        childInstance->setPrototype(childPrototype);
        childInstance->setFile(childPrototype->file());
        childInstance->setId(childPrototype->id());
        childInstance->setLocation(childPrototype->location());
        childInstance->setScope(instance->scope());
        Item::addChild(instance, childInstance);
        createChildInstances(childInstance, childPrototype, prototypeInstanceMap);
    }
}

void ModuleLoader::resolveProbes(ProductContext *productContext, Item *item)
{
    AccumulatingTimer probesTimer(m_parameters.logElapsedTime() ? &m_elapsedTimeProbes : nullptr);
    EvalContextSwitcher evalContextSwitcher(m_evaluator->engine(), EvalContext::ProbeExecution);
    for (Item * const child : item->children())
        if (child->type() == ItemType::Probe)
            resolveProbe(productContext, item, child);
}

void ModuleLoader::resolveProbe(ProductContext *productContext, Item *parent, Item *probe)
{
    qCDebug(lcModuleLoader) << "Resolving Probe at " << probe->location().toString();
    ++m_probesEncountered;
    const QString &probeId = probeGlobalId(probe);
    if (Q_UNLIKELY(probeId.isEmpty()))
        throw ErrorInfo(Tr::tr("Probe.id must be set."), probe->location());
    const JSSourceValueConstPtr configureScript
            = probe->sourceProperty(StringConstants::configureProperty());
    QBS_CHECK(configureScript);
    if (Q_UNLIKELY(configureScript->sourceCode() == StringConstants::undefinedValue()))
        throw ErrorInfo(Tr::tr("Probe.configure must be set."), probe->location());
    using ProbeProperty = std::pair<QString, QScriptValue>;
    std::vector<ProbeProperty> probeBindings;
    QVariantMap initialProperties;
    for (Item *obj = probe; obj; obj = obj->prototype()) {
        const Item::PropertyMap &props = obj->properties();
        for (auto it = props.cbegin(); it != props.cend(); ++it) {
            const QString &name = it.key();
            if (name == StringConstants::configureProperty())
                continue;
            const QScriptValue value = m_evaluator->value(probe, name);
            probeBindings << ProbeProperty(name, value);
            if (name != StringConstants::conditionProperty())
                initialProperties.insert(name, value.toVariant());
        }
    }
    ScriptEngine * const engine = m_evaluator->engine();
    QScriptValue configureScope;
    const bool condition = m_evaluator->boolValue(probe, StringConstants::conditionProperty());
    const QString &sourceCode = configureScript->sourceCode().toString();
    ProbeConstPtr resolvedProbe;
    if (parent->type() == ItemType::Project
            || productContext->name.startsWith(StringConstants::shadowProductPrefix())) {
        resolvedProbe = findOldProjectProbe(probeId, condition, initialProperties, sourceCode);
    } else {
        const QString &uniqueProductName = productContext->uniqueName();
        resolvedProbe
                = findOldProductProbe(uniqueProductName, condition, initialProperties, sourceCode);
    }
    if (!resolvedProbe) {
        resolvedProbe = findCurrentProbe(probe->location(), condition, initialProperties);
        if (resolvedProbe) {
            qCDebug(lcModuleLoader) << "probe results cached from current run";
            ++m_probesCachedCurrent;
        }
    } else {
        qCDebug(lcModuleLoader) << "probe results cached from earlier run";
        ++m_probesCachedOld;
    }
    std::vector<QString> importedFilesUsedInConfigure;
    if (!condition) {
        qCDebug(lcModuleLoader) << "Probe disabled; skipping";
    } else if (!resolvedProbe) {
        ++m_probesRun;
        qCDebug(lcModuleLoader) << "configure script needs to run";
        const Evaluator::FileContextScopes fileCtxScopes
                = m_evaluator->fileContextScopes(configureScript->file());
        engine->currentContext()->pushScope(fileCtxScopes.fileScope);
        engine->currentContext()->pushScope(fileCtxScopes.importScope);
        configureScope = engine->newObject();
        for (const ProbeProperty &b : probeBindings)
            configureScope.setProperty(b.first, b.second);
        engine->currentContext()->pushScope(configureScope);
        engine->clearRequestedProperties();
        QScriptValue sv = engine->evaluate(configureScript->sourceCodeForEvaluation());
        engine->currentContext()->popScope();
        engine->currentContext()->popScope();
        engine->currentContext()->popScope();
        engine->releaseResourcesOfScriptObjects();
        if (Q_UNLIKELY(engine->hasErrorOrException(sv)))
            throw ErrorInfo(engine->lastErrorString(sv), configureScript->location());
        importedFilesUsedInConfigure = engine->importedFilesUsedInScript();
    } else {
        importedFilesUsedInConfigure = resolvedProbe->importedFilesUsed();
    }
    QVariantMap properties;
    for (const ProbeProperty &b : probeBindings) {
        QVariant newValue;
        if (resolvedProbe) {
            newValue = resolvedProbe->properties().value(b.first);
        } else {
            if (condition) {
                QScriptValue v = configureScope.property(b.first);
                m_evaluator->convertToPropertyType(probe->propertyDeclaration(
                                                   b.first), probe->location(), v);
                if (Q_UNLIKELY(engine->hasErrorOrException(v)))
                    throw ErrorInfo(engine->lastError(v));
                newValue = v.toVariant();
            } else {
                newValue = initialProperties.value(b.first);
            }
        }
        if (newValue != b.second.toVariant())
            probe->setProperty(b.first, VariantValue::create(newValue));
        if (!resolvedProbe)
            properties.insert(b.first, newValue);
    }
    if (!resolvedProbe) {
        resolvedProbe = Probe::create(probeId, probe->location(), condition,
                                      sourceCode, properties, initialProperties,
                                      importedFilesUsedInConfigure);
        m_currentProbes[probe->location()] << resolvedProbe;
    }
    productContext->info.probes << resolvedProbe;
}

void ModuleLoader::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled()) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration %1.")
                    .arg(TopLevelProject::deriveId(m_parameters.finalBuildConfigurationTree())));
    }
}

bool ModuleLoader::checkItemCondition(Item *item, Item *itemToDisable)
{
    if (m_evaluator->boolValue(item, StringConstants::conditionProperty()))
        return true;
    m_disabledItems += itemToDisable ? itemToDisable : item;
    return false;
}

QStringList ModuleLoader::readExtraSearchPaths(Item *item, bool *wasSet)
{
    QStringList result;
    const QStringList paths = m_evaluator->stringListValue(
                item, StringConstants::qbsSearchPathsProperty(), wasSet);
    const JSSourceValueConstPtr prop = item->sourceProperty(
                StringConstants::qbsSearchPathsProperty());

    // Value can come from within a project file or as an overridden value from the user
    // (e.g command line).
    const QString basePath = FileInfo::path(prop ? prop->file()->filePath()
                                                 : m_parameters.projectFilePath());
    for (const QString &path : paths)
        result += FileInfo::resolvePath(basePath, path);
    return result;
}

void ModuleLoader::copyProperties(const Item *sourceProject, Item *targetProject)
{
    if (!sourceProject)
        return;
    const QList<PropertyDeclaration> builtinProjectProperties = BuiltinDeclarations::instance()
            .declarationsForType(ItemType::Project).properties();
    Set<QString> builtinProjectPropertyNames;
    for (const PropertyDeclaration &p : builtinProjectProperties)
        builtinProjectPropertyNames << p.name();

    for (Item::PropertyDeclarationMap::ConstIterator it
         = sourceProject->propertyDeclarations().constBegin();
         it != sourceProject->propertyDeclarations().constEnd(); ++it) {

        // We must not inherit built-in properties such as "name",
        // but there are exceptions.
        if (it.key() == StringConstants::qbsSearchPathsProperty()
                || it.key() == StringConstants::profileProperty()
                || it.key() == StringConstants::buildDirectoryProperty()
                || it.key() == StringConstants::sourceDirectoryProperty()
                || it.key() == StringConstants::minimumQbsVersionProperty()) {
            const JSSourceValueConstPtr &v = targetProject->sourceProperty(it.key());
            QBS_ASSERT(v, continue);
            if (v->sourceCode() == StringConstants::undefinedValue())
                sourceProject->copyProperty(it.key(), targetProject);
            continue;
        }

        if (builtinProjectPropertyNames.contains(it.key()))
            continue;

        if (targetProject->hasOwnProperty(it.key()))
            continue; // Ignore stuff the target project already has.

        targetProject->setPropertyDeclaration(it.key(), it.value());
        sourceProject->copyProperty(it.key(), targetProject);
    }
}

Item *ModuleLoader::wrapInProjectIfNecessary(Item *item)
{
    if (item->type() == ItemType::Project)
        return item;
    Item *prj = Item::create(item->pool(), ItemType::Project);
    Item::addChild(prj, item);
    prj->setFile(item->file());
    prj->setLocation(item->location());
    prj->setupForBuiltinType(m_logger);
    return prj;
}

QString ModuleLoader::findExistingModulePath(const QString &searchPath,
        const QualifiedId &moduleName)
{
    QString dirPath = searchPath + QStringLiteral("/modules");

    // isFileCaseCorrect is a very expensive call on macOS, so we cache the value for the
    // modules and search paths we've already processed
    auto &moduleInfo = m_existingModulePathCache[{searchPath, moduleName}];
    if (moduleInfo.first) // poor man's std::optional<QString>
        return moduleInfo.second;

    for (const QString &moduleNamePart : moduleName) {
        dirPath = FileInfo::resolvePath(dirPath, moduleNamePart);
        if (!FileInfo::exists(dirPath) || !FileInfo::isFileCaseCorrect(dirPath)) {
            moduleInfo.first = true;
            return moduleInfo.second = QString();
        }
    }

    moduleInfo.first = true;
    return moduleInfo.second = dirPath;
}

QStringList ModuleLoader::findExistingModulePaths(
        const QStringList &searchPaths, const QualifiedId &moduleName)
{
    QStringList result;
    result.reserve(searchPaths.size());
    for (const auto &path: searchPaths) {
        const QString dirPath = findExistingModulePath(path, moduleName);
        if (!dirPath.isEmpty())
            result.append(dirPath);
    }
    return result;
}

QVariantMap ModuleLoader::moduleProviderConfig(ModuleLoader::ProductContext &product)
{
    if (product.moduleProviderConfigRetrieved)
        return product.theModuleProviderConfig;
    const ItemValueConstPtr configItemValue
            = product.item->itemProperty(StringConstants::moduleProviders());
    if (configItemValue) {
        const std::function<void(const Item *, QualifiedId)> collectMap
                = [this, &product, &collectMap](const Item *item, const QualifiedId &name) {
            const Item::PropertyMap &props = item->properties();
            for (auto it = props.begin(); it != props.end(); ++it) {
                QVariant value;
                switch (it.value()->type()) {
                case Value::ItemValueType: {
                    const auto childItem = static_cast<ItemValue *>(it.value().get())->item();
                    childItem->setScope(item->scope());
                    collectMap(childItem, QualifiedId(name) << it.key());
                    continue;
                }
                case Value::JSSourceValueType:
                    value = m_evaluator->value(item, it.key()).toVariant();
                    break;
                case Value::VariantValueType:
                    value = static_cast<VariantValue *>(it.value().get())->value();
                    break;
                }
                QVariantMap m = product.theModuleProviderConfig.value(name.toString()).toMap();
                m.insert(it.key(), value);
                product.theModuleProviderConfig.insert(name.toString(), m);
            }
        };
        configItemValue->item()->setScope(product.scope);
        collectMap(configItemValue->item(), QualifiedId());
    }
    for (auto it = product.moduleProperties.begin(); it != product.moduleProperties.end(); ++it) {
        if (!it.key().startsWith(QStringLiteral("moduleProviders.")))
            continue;
        const QString provider = it.key().mid(QStringLiteral("moduleProviders.").size());
        const QVariantMap providerConfigFromBuildConfig = it.value().toMap();
        if (providerConfigFromBuildConfig.empty())
            continue;
        QVariantMap currentMapForProvider = product.theModuleProviderConfig.value(provider).toMap();
        for (auto propIt = providerConfigFromBuildConfig.begin();
             propIt != providerConfigFromBuildConfig.end(); ++propIt) {
            currentMapForProvider.insert(propIt.key(), propIt.value());
        }
        product.theModuleProviderConfig.insert(provider, currentMapForProvider);
    }
    product.moduleProviderConfigRetrieved = true;
    return product.theModuleProviderConfig;
}

ModuleLoader::ModuleProviderResult ModuleLoader::findModuleProvider(const QualifiedId &name,
        ModuleLoader::ProductContext &product, ModuleProviderLookup lookupType,
        const CodeLocation &dependsItemLocation)
{
    for (const QString &path : m_reader->allSearchPaths()) {
        QString fullPath = FileInfo::resolvePath(path, QStringLiteral("module-providers"));
        switch (lookupType) {
        case ModuleProviderLookup::Regular:
            for (const QString &component : name)
                fullPath = FileInfo::resolvePath(fullPath, component);
            break;
        case ModuleProviderLookup::Fallback:
            fullPath = FileInfo::resolvePath(fullPath, QStringLiteral("__fallback"));
            break;
        }
        const QString providerFile = FileInfo::resolvePath(fullPath,
                                                           QStringLiteral("provider.qbs"));
        if (!FileInfo::exists(providerFile)) {
            qCDebug(lcModuleLoader) << "No module provider found at" << providerFile;
            continue;
        }
        QTemporaryFile dummyItemFile;
        if (!dummyItemFile.open()) {
            throw ErrorInfo(Tr::tr("Failed to create temporary file for running module provider "
                                   "for dependency '%1': %2").arg(name.toString(),
                                                                  dummyItemFile.errorString()));
        }
        m_tempQbsFiles << dummyItemFile.fileName();
        qCDebug(lcModuleLoader) << "Instantiating module provider at" << providerFile;
        const QString projectBuildDir = product.project->item->variantProperty(
                    StringConstants::buildDirectoryProperty())->value().toString();
        const QString searchPathBaseDir = ModuleProviderInfo::outputDirPath(projectBuildDir, name);
        const QVariant moduleConfig = moduleProviderConfig(product).value(name.toString());
        QTextStream stream(&dummyItemFile);
        using Qt::endl;
        stream.setCodec("UTF-8");
        stream << "import qbs.FileInfo" << endl;
        stream << "import qbs.Utilities" << endl;
        stream << "import '" << providerFile << "' as Provider" << endl;
        stream << "Provider {" << endl;
        stream << "    name: " << toJSLiteral(name.toString()) << endl;
        stream << "    property var config: (" << toJSLiteral(moduleConfig) << ')' << endl;
        stream << "    outputBaseDir: FileInfo.joinPaths(baseDirPrefix, "
                  "        Utilities.getHash(JSON.stringify(config)))" << endl;
        stream << "    property string baseDirPrefix: " << toJSLiteral(searchPathBaseDir) << endl;
        stream << "    property stringList searchPaths: (relativeSearchPaths || [])"
                  "        .map(function(p) { return FileInfo.joinPaths(outputBaseDir, p); })"
               << endl;
        stream << "}" << endl;
        stream.flush();
        Item * const providerItem = loadItemFromFile(dummyItemFile.fileName(), dependsItemLocation);
        if (providerItem->type() != ItemType::ModuleProvider) {
            throw ErrorInfo(Tr::tr("File '%1' declares an item of type '%2', "
                                   "but '%3' was expected.")
                .arg(providerFile, providerItem->typeName(),
                     BuiltinDeclarations::instance().nameForType(ItemType::ModuleProvider)));
        }
        providerItem->setParent(product.item);
        const QVariantMap configMap = moduleConfig.toMap();
        for (auto it = configMap.begin(); it != configMap.end(); ++it) {
            const PropertyDeclaration decl = providerItem->propertyDeclaration(it.key());
            if (!decl.isValid()) {
                throw ErrorInfo(Tr::tr("No such property '%1' in module provider '%2'.")
                                .arg(it.key(), name.toString()));
            }
            providerItem->setProperty(it.key(), VariantValue::create(it.value()));
        }
        EvalContextSwitcher contextSwitcher(m_evaluator->engine(), EvalContext::ModuleProvider);
        const QStringList searchPaths
                = m_evaluator->stringListValue(providerItem, QStringLiteral("searchPaths"));
        const auto addToGlobalInfo = [=] {
            m_moduleProviderInfo.emplace_back(ModuleProviderInfo(name, moduleConfig.toMap(),
                                                         searchPaths, m_parameters.dryRun()));
        };
        if (searchPaths.empty()) {
            qCDebug(lcModuleLoader) << "Module provider did run, but did not set up "
                                       "any modules.";
            addToGlobalInfo();
            return {true, false};
        }
        qCDebug(lcModuleLoader) << "Module provider added" << searchPaths.size()
                                << "new search path(s)";

        // (1) is needed so the immediate new look-up works.
        // (2) is needed so the next use of SearchPathManager considers the new paths.
        // (3) is needed for the code that removes the product-specific search paths when
        //     product handling is done.
        // (4) is needed for possible re-use in subsequent products and builds.
        m_reader->pushExtraSearchPaths(searchPaths); // (1)
        product.searchPaths << searchPaths; // (2)
        product.newlyAddedModuleProviderSearchPaths.push_back(searchPaths); // (3)
        addToGlobalInfo(); // (4)
        return {true, true};
    }
    return {};
}

void ModuleLoader::setScopeForDescendants(Item *item, Item *scope)
{
    for (Item * const child : item->children()) {
        child->setScope(scope);
        setScopeForDescendants(child, scope);
    }
}

void ModuleLoader::overrideItemProperties(Item *item, const QString &buildConfigKey,
        const QVariantMap &buildConfig)
{
    const QVariant buildConfigValue = buildConfig.value(buildConfigKey);
    if (buildConfigValue.isNull())
        return;
    const QVariantMap overridden = buildConfigValue.toMap();
    for (QVariantMap::const_iterator it = overridden.constBegin(); it != overridden.constEnd();
            ++it) {
        const PropertyDeclaration decl = item->propertyDeclaration(it.key());
        if (!decl.isValid()) {
            ErrorInfo error(Tr::tr("Unknown property: %1.%2").arg(buildConfigKey, it.key()));
            handlePropertyError(error, m_parameters, m_logger);
            continue;
        }
        item->setProperty(it.key(),
                VariantValue::create(convertToPropertyType(it.value(), decl.type(),
                        QStringList(buildConfigKey), it.key())));
    }
}

void ModuleLoader::collectAllModules(Item *item, std::vector<Item::Module> *modules)
{
    for (const Item::Module &m : item->modules()) {
        if (moduleRepresentsDisabledProduct(m))
            m.item->removeModules();
        auto it = std::find_if(modules->begin(), modules->end(),
                               [m] (const Item::Module &m2) { return m.name == m2.name; });
        if (it != modules->end()) {
            // If a module is required somewhere, it is required in the top-level item.
            if (m.required)
                it->required = true;
            it->versionRange.narrowDown(m.versionRange);
            continue;
        }
        modules->push_back(m);
        collectAllModules(m.item, modules);
    }
}

std::vector<Item::Module> ModuleLoader::allModules(Item *item)
{
    std::vector<Item::Module> lst;
    collectAllModules(item, &lst);
    return lst;
}

bool ModuleLoader::moduleRepresentsDisabledProduct(const Item::Module &module)
{
    if (!module.isProduct)
        return false;
    const Item *exportItem = module.item->prototype();
    while (exportItem && exportItem->type() != ItemType::Export)
        exportItem = exportItem->prototype();
    QBS_CHECK(exportItem);
    Item * const productItem = exportItem->parent();
    QBS_CHECK(productItem->type() == ItemType::Product);
    return m_disabledItems.contains(productItem) || !checkItemCondition(productItem);
}

void ModuleLoader::addProductModuleDependencies(ProductContext *productContext, const QString &name)
{
    auto deps = productContext->productModuleDependencies.at(name);
    QList<ModuleLoaderResult::ProductInfo::Dependency> depsToAdd;
    const bool productIsMultiplexed = !productContext->multiplexConfigurationId.isEmpty();
    for (auto &dep : deps) {
        const auto productRange = m_productsByName.equal_range(dep.name);
        std::vector<const ProductContext *> dependencies;
        bool hasNonMultiplexedDependency = false;
        for (auto it = productRange.first; it != productRange.second; ++it) {
            if (!it->second->multiplexConfigurationId.isEmpty()) {
                dependencies.push_back(it->second);
                if (productIsMultiplexed && dep.profile.isEmpty())
                    break;
            } else {
                hasNonMultiplexedDependency = true;
                break;
            }
        }

        if (hasNonMultiplexedDependency) {
            depsToAdd.push_back(dep);
            continue;
        }

        for (std::size_t i = 0; i < dependencies.size(); ++i) {
            const bool profileMatch = dep.profile.isEmpty()
                    || dep.profile == StringConstants::star()
                    || dep.profile == dependencies.at(i)->profileName;
            if (i == 0) {
                if (productIsMultiplexed && dep.profile.isEmpty()) {
                    const ValuePtr &multiplexConfigIdProp = productContext->item->property(
                                StringConstants::multiplexConfigurationIdProperty());
                    dep.multiplexConfigurationId = std::static_pointer_cast<VariantValue>(
                                multiplexConfigIdProp)->value().toString();
                    depsToAdd.push_back(dep);
                    break;
                } else if (profileMatch) {
                    dep.multiplexConfigurationId = dependencies.at(i)->multiplexConfigurationId;
                    depsToAdd.push_back(dep);
                }
            } else if (profileMatch) {
                ModuleLoaderResult::ProductInfo::Dependency newDependency = dep;
                newDependency.multiplexConfigurationId
                        = dependencies.at(i)->multiplexConfigurationId;
                depsToAdd << newDependency;
            }
        }
    }
    productContext->info.usedProducts.insert(productContext->info.usedProducts.end(),
                depsToAdd.cbegin(), depsToAdd.cend());
}

static void collectProductModuleDependencies(Item *item, Set<QualifiedId> &allDeps)
{
    for (const Item::Module &m : item->modules()) {
        if (m.isProduct && allDeps.insert(m.name).second)
            collectProductModuleDependencies(m.item, allDeps);
    }
}

void ModuleLoader::addProductModuleDependencies(ModuleLoader::ProductContext *ctx)
{
    Set<QualifiedId> deps;
    collectProductModuleDependencies(ctx->item, deps);
    for (const QualifiedId &dep : deps)
        addProductModuleDependencies(ctx, dep.toString());
}

void ModuleLoader::addTransitiveDependencies(ProductContext *ctx)
{
    AccumulatingTimer timer(m_parameters.logElapsedTime()
                            ? &m_elapsedTimeTransitiveDependencies : nullptr);
    qCDebug(lcModuleLoader) << "addTransitiveDependencies";

    std::vector<Item::Module> transitiveDeps = allModules(ctx->item);
    std::sort(transitiveDeps.begin(), transitiveDeps.end());
    for (const Item::Module &m : ctx->item->modules()) {
        auto it = std::lower_bound(transitiveDeps.begin(), transitiveDeps.end(), m);
        QBS_CHECK(it != transitiveDeps.end() && it->name == m.name);
        transitiveDeps.erase(it);
    }
    for (const Item::Module &module : qAsConst(transitiveDeps)) {
        if (module.isProduct) {
            ctx->item->addModule(module);
        } else {
            Item::Module dep;
            dep.item = loadModule(ctx, nullptr, ctx->item, ctx->item->location(), QString(),
                                  module.name, QString(), FallbackMode::Disabled,
                                  module.required, &dep.isProduct, &dep.parameters);
            if (!dep.item) {
                throw ErrorInfo(Tr::tr("Module '%1' not found when setting up transitive "
                                       "dependencies for product '%2'.").arg(module.name.toString(),
                                                                             ctx->name),
                                ctx->item->location());
            }
            dep.name = module.name;
            dep.required = module.required;
            dep.versionRange = module.versionRange;
            ctx->item->addModule(dep);
        }
    }
}

Item *ModuleLoader::createNonPresentModule(const QString &name, const QString &reason, Item *module)
{
    qCDebug(lcModuleLoader) << "Non-required module '" << name << "' not loaded (" << reason << ")."
                        << "Creating dummy module for presence check.";
    if (!module) {
        module = Item::create(m_pool, ItemType::ModuleInstance);
        module->setFile(FileContext::create());
        module->setProperty(StringConstants::nameProperty(), VariantValue::create(name));
    }
    module->setProperty(StringConstants::presentProperty(), VariantValue::falseValue());
    return module;
}

void ModuleLoader::handleProductError(const ErrorInfo &error,
                                      ModuleLoader::ProductContext *productContext)
{
    const bool alreadyHadError = productContext->info.delayedError.hasError();
    if (!alreadyHadError) {
        productContext->info.delayedError.append(Tr::tr("Error while handling product '%1':")
                                                 .arg(productContext->name),
                                                 productContext->item->location());
    }
    if (error.isInternalError()) {
        if (alreadyHadError) {
            qCDebug(lcModuleLoader()) << "ignoring subsequent internal error" << error.toString()
                                      << "in product" << productContext->name;
            return;
        }
        for (const auto &kv : productContext->productModuleDependencies) {
            const auto rangeForName = m_productsByName.equal_range(kv.first);
            for (auto rangeIt = rangeForName.first; rangeIt != rangeForName.second; ++rangeIt) {
                const ProductContext * const dep = rangeIt->second;
                if (dep->info.delayedError.hasError()) {
                    qCDebug(lcModuleLoader()) << "ignoring internal error" << error.toString()
                                              << "in product" << productContext->name
                                              << "assumed to be caused by erroneous dependency"
                                              << dep->name;
                    return;
                }
            }
        }
    }
    const auto errorItems = error.items();
    for (const ErrorItem &ei : errorItems)
        productContext->info.delayedError.append(ei.description(), ei.codeLocation());
    productContext->project->result->productInfos.insert(productContext->item,
                                                         productContext->info);
    m_disabledItems << productContext->item;
    m_erroneousProducts.insert(productContext->name);
}

static void gatherAssignedProperties(ItemValue *iv, const QualifiedId &prefix,
                                     QualifiedIdSet &properties)
{
    const Item::PropertyMap &props = iv->item()->properties();
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        switch (it.value()->type()) {
        case Value::JSSourceValueType:
            properties << (QualifiedId(prefix) << it.key());
            break;
        case Value::ItemValueType:
            if (iv->item()->type() == ItemType::ModulePrefix) {
                gatherAssignedProperties(std::static_pointer_cast<ItemValue>(it.value()).get(),
                                         QualifiedId(prefix) << it.key(), properties);
            }
            break;
        default:
            break;
        }
    }
}

QualifiedIdSet ModuleLoader::gatherModulePropertiesSetInGroup(const Item *group)
{
    QualifiedIdSet propsSetInGroup;
    const Item::PropertyMap &props = group->properties();
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        if (it.value()->type() == Value::ItemValueType) {
            gatherAssignedProperties(std::static_pointer_cast<ItemValue>(it.value()).get(),
                                     QualifiedId(it.key()), propsSetInGroup);
        }
    }
    return propsSetInGroup;
}

void ModuleLoader::markModuleTargetGroups(Item *group, const Item::Module &module)
{
    QBS_CHECK(group->type() == ItemType::Group);
    if (m_evaluator->boolValue(group, StringConstants::filesAreTargetsProperty())) {
        group->setProperty(StringConstants::modulePropertyInternal(),
                           VariantValue::create(module.name.toString()));
    }
    for (Item * const child : group->children())
        markModuleTargetGroups(child, module);
}

void ModuleLoader::copyGroupsFromModuleToProduct(const ProductContext &productContext,
                                                 const Item::Module &module,
                                                 const Item *modulePrototype)
{
    for (Item * const child : modulePrototype->children()) {
        if (child->type() == ItemType::Group) {
            Item * const clonedGroup = child->clone();
            clonedGroup->setScope(productContext.scope);
            setScopeForDescendants(clonedGroup, productContext.scope);
            Item::addChild(productContext.item, clonedGroup);
            markModuleTargetGroups(clonedGroup, module);
        }
    }
}

void ModuleLoader::copyGroupsFromModulesToProduct(const ProductContext &productContext)
{
    for (const Item::Module &module : productContext.item->modules()) {
        Item *prototype = module.item;
        bool modulePassedValidation;
        while ((modulePassedValidation = prototype->isPresentModule()) && prototype->prototype())
            prototype = prototype->prototype();
        if (modulePassedValidation)
            copyGroupsFromModuleToProduct(productContext, module, prototype);
    }
}

QString ModuleLoaderResult::ProductInfo::Dependency::uniqueName() const
{
    return ResolvedProduct::uniqueName(name, multiplexConfigurationId);
}

QString ModuleLoader::ProductContext::uniqueName() const
{
    return ResolvedProduct::uniqueName(name, multiplexConfigurationId);
}

} // namespace Internal
} // namespace qbs
