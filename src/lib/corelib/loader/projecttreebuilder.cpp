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

#include "projecttreebuilder.h"

#include "dependenciesresolver.h"
#include "groupshandler.h"
#include "itemreader.h"
#include "localprofiles.h"
#include "moduleinstantiator.h"
#include "modulepropertymerger.h"
#include "probesresolver.h"
#include "productitemmultiplexer.h"

#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/filetags.h>
#include <language/item.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/filetime.h>
#include <tools/preferences.h>
#include <tools/progressobserver.h>
#include <tools/profile.h>
#include <tools/profiling.h>
#include <tools/scripttools.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>
#include <tools/version.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <utility>
#include <vector>

namespace qbs::Internal {

namespace {

class TimingData {
public:
    qint64 prepareProducts = 0;
    qint64 handleProducts = 0;
    qint64 propertyChecking = 0;
};

} // namespace

class ProjectTreeBuilder::Private
{
public:
    Private(const SetupProjectParameters &parameters, ItemPool &itemPool, Evaluator &evaluator,
            Logger &logger)
        : parameters(parameters), itemPool(itemPool), evaluator(evaluator), logger(logger) {}

    Item *loadTopLevelProjectItem();
    void checkOverriddenValues();
    void collectNameFromOverride(const QString &overrideString);
    Item *loadItemFromFile(const QString &filePath, const CodeLocation &referencingLocation);
    Item *wrapInProjectIfNecessary(Item *item);
    void handleTopLevelProject(Item *projectItem, const Set<QString> &referencedFilePaths);
    void handleProject(Item *projectItem, const Set<QString> &referencedFilePaths);
    void prepareProduct(ProjectContext &projectContext, Item *productItem);
    void handleNextProduct();
    void handleProduct(ProductContext &productContext, Deferral deferral);
    void handleSubProject(ProjectContext &projectContext, Item *projectItem,
                          const Set<QString> &referencedFilePaths);
    void initProductProperties(const ProductContext &product);
    void printProfilingInfo();
    void checkProjectNamesInOverrides();
    void checkProductNamesInOverrides();
    void collectProductsByName();
    void handleModuleSetupError(ProductContext &product, const Item::Module &module,
                                const ErrorInfo &error);
    bool checkItemCondition(Item *item);
    QList<Item *> multiplexProductItem(ProductContext &dummyContext, Item *productItem);
    void checkCancelation() const;
    QList<Item *> loadReferencedFile(const QString &relativePath,
                                     const CodeLocation &referencingLocation,
                                     const Set<QString> &referencedFilePaths,
                                     ProductContext &dummyContext);
    void copyProperties(const Item *sourceProject, Item *targetProject);
    bool mergeExportItems(ProductContext &productContext);
    bool checkExportItemCondition(Item *exportItem, const ProductContext &product);
    void resolveProbes(ProductContext &product, Item *item);

    const SetupProjectParameters &parameters;
    ItemPool &itemPool;
    Evaluator &evaluator;
    Logger &logger;
    ProgressObserver *progressObserver = nullptr;
    TimingData timingData;
    ItemReader reader{parameters, logger};
    ProbesResolver probesResolver{parameters, evaluator, logger};
    ModulePropertyMerger propertyMerger{parameters, evaluator, logger};
    ModuleInstantiator moduleInstantiator{parameters, itemPool, propertyMerger, logger};
    ProductItemMultiplexer multiplexer{parameters, evaluator, logger, [this](Item *productItem) {
                                           return moduleInstantiator.retrieveQbsItem(productItem);
                                       }};
    GroupsHandler groupsHandler{parameters, moduleInstantiator, evaluator, logger};
    LocalProfiles localProfiles{parameters, evaluator, logger};
    DependenciesResolver dependenciesResolver{parameters, itemPool, evaluator, reader,
                                              probesResolver, moduleInstantiator, logger};
    FileTime lastResolveTime;
    QVariantMap storedProfiles;

    Settings settings{parameters.settingsDirectory()};
    Set<QString> projectNamesUsedInOverrides;
    Set<QString> productNamesUsedInOverrides;
    Set<QString> disabledProjects;
    TopLevelProjectContext topLevelProject;

    Version qbsVersion;
    Item *tempScopeItem = nullptr;

private:
    class TempBaseModuleAttacher {
    public:
        TempBaseModuleAttacher(Private *d, ProductContext &product);
        ~TempBaseModuleAttacher() { drop(); }
        void drop();
        Item *tempBaseModuleItem() const { return m_tempBaseModule; }

    private:
        Item * const m_productItem;
        ValuePtr m_origQbsValue;
        Item *m_tempBaseModule = nullptr;
    };
};

ProjectTreeBuilder::ProjectTreeBuilder(const SetupProjectParameters &parameters, ItemPool &itemPool,
                                       Evaluator &evaluator, Logger &logger)
    : d(makePimpl<Private>(parameters, itemPool, evaluator, logger)) {}
ProjectTreeBuilder::~ProjectTreeBuilder() = default;

void ProjectTreeBuilder::setProgressObserver(ProgressObserver *progressObserver)
{
    d->progressObserver = progressObserver;
}

void ProjectTreeBuilder::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    d->probesResolver.setOldProjectProbes(oldProbes);
}

void ProjectTreeBuilder::setOldProductProbes(
    const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    d->probesResolver.setOldProductProbes(oldProbes);
}

void ProjectTreeBuilder::setLastResolveTime(const FileTime &time) { d->lastResolveTime = time; }

void ProjectTreeBuilder::setStoredProfiles(const QVariantMap &profiles)
{
    d->storedProfiles = profiles;
}

void ProjectTreeBuilder::setStoredModuleProviderInfo(
    const StoredModuleProviderInfo &moduleProviderInfo)
{
    d->dependenciesResolver.setStoredModuleProviderInfo(moduleProviderInfo);
}

ProjectTreeBuilder::Result ProjectTreeBuilder::load()
{
    TimedActivityLogger mainTimer(d->logger, Tr::tr("ProjectTreeBuilder"),
                                  d->parameters.logElapsedTime());
    qCDebug(lcModuleLoader) << "load" << d->parameters.projectFilePath();

    d->checkOverriddenValues();
    d->reader.setPool(&d->itemPool);

    Result result;
    d->topLevelProject.profileConfigs = d->storedProfiles;
    result.root = d->loadTopLevelProjectItem();
    d->handleTopLevelProject(result.root, {QDir::cleanPath(d->parameters.projectFilePath())});

    result.qbsFiles = d->reader.filesRead() - d->dependenciesResolver.tempQbsFiles();
    result.productInfos = d->topLevelProject.productInfos;
    result.profileConfigs = d->topLevelProject.profileConfigs;
    for (auto it = d->localProfiles.profiles().begin(); it != d->localProfiles.profiles().end();
         ++it) {
        result.profileConfigs.remove(it.key());
    }
    result.projectProbes = d->topLevelProject.probes;
    result.storedModuleProviderInfo = d->dependenciesResolver.storedModuleProviderInfo();

    d->printProfilingInfo();

    return result;
}

Item *ProjectTreeBuilder::Private::loadTopLevelProjectItem()
{
    const QStringList topLevelSearchPaths
        = parameters.finalBuildConfigurationTree()
              .value(StringConstants::projectPrefix()).toMap()
              .value(StringConstants::qbsSearchPathsProperty()).toStringList();
    SearchPathsManager searchPathsManager(reader, topLevelSearchPaths);
    Item * const root = loadItemFromFile(parameters.projectFilePath(), CodeLocation());
    if (!root)
        return {};

    switch (root->type()) {
    case ItemType::Product:
        return wrapInProjectIfNecessary(root);
    case ItemType::Project:
        return root;
    default:
        throw ErrorInfo(Tr::tr("The top-level item must be of type 'Project' or 'Product', but it"
                               " is of type '%1'.").arg(root->typeName()), root->location());
    }
}

void ProjectTreeBuilder::Private::checkOverriddenValues()
{
    static const auto matchesPrefix = [](const QString &key) {
        static const QStringList prefixes({StringConstants::projectPrefix(),
                                           QStringLiteral("projects"),
                                           QStringLiteral("products"), QStringLiteral("modules"),
                                           StringConstants::moduleProviders(),
                                           StringConstants::qbsModule()});
        for (const auto &prefix : prefixes) {
            if (key.startsWith(prefix + QLatin1Char('.')))
                return true;
        }
        return false;
    };
    const QVariantMap &overriddenValues = parameters.overriddenValues();
    for (auto it = overriddenValues.begin(); it != overriddenValues.end(); ++it) {
        if (matchesPrefix(it.key())) {
            collectNameFromOverride(it.key());
            continue;
        }

        ErrorInfo e(Tr::tr("Property override key '%1' not understood.").arg(it.key()));
        e.append(Tr::tr("Please use one of the following:"));
        e.append(QLatin1Char('\t') + Tr::tr("projects.<project-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("modules.<module-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<module-name>."
                                            "<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("moduleProviders.<provider-name>."
                                            "<property-name>:value"));
        handlePropertyError(e, parameters, logger);
    }
}

void ProjectTreeBuilder::Private::collectNameFromOverride(const QString &overrideString)
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
        projectNamesUsedInOverrides.insert(projectName);
        return;
    }
    const QString &productName = extract(StringConstants::productsOverridePrefix(), overrideString);
    if (!productName.isEmpty()) {
        productNamesUsedInOverrides.insert(productName.left(
            productName.indexOf(StringConstants::dot())));
        return;
    }
}

Item *ProjectTreeBuilder::Private::loadItemFromFile(const QString &filePath,
                                                    const CodeLocation &referencingLocation)
{
    return reader.setupItemFromFile(filePath, referencingLocation, evaluator);
}

Item *ProjectTreeBuilder::Private::wrapInProjectIfNecessary(Item *item)
{
    if (item->type() == ItemType::Project)
        return item;
    Item *prj = Item::create(item->pool(), ItemType::Project);
    Item::addChild(prj, item);
    prj->setFile(item->file());
    prj->setLocation(item->location());
    prj->setupForBuiltinType(parameters.deprecationWarningMode(), logger);
    return prj;
}

void ProjectTreeBuilder::Private::handleTopLevelProject(Item *projectItem,
                                                        const Set<QString> &referencedFilePaths)
{
    topLevelProject.buildDirectory = TopLevelProject::deriveBuildDirectory(
        parameters.buildRoot(),
        TopLevelProject::deriveId(parameters.finalBuildConfigurationTree()));
    projectItem->setProperty(StringConstants::sourceDirectoryProperty(),
                             VariantValue::create(QFileInfo(projectItem->file()->filePath())
                                                      .absolutePath()));
    projectItem->setProperty(StringConstants::buildDirectoryProperty(),
                             VariantValue::create(topLevelProject.buildDirectory));
    projectItem->setProperty(StringConstants::profileProperty(),
                             VariantValue::create(parameters.topLevelProfile()));
    handleProject(projectItem, referencedFilePaths);
    checkProjectNamesInOverrides();
    collectProductsByName();
    checkProductNamesInOverrides();

    for (ProjectContext * const projectContext : std::as_const(topLevelProject.projects)) {
        for (ProductContext &productContext : projectContext->products)
            topLevelProject.productsToHandle.emplace_back(&productContext, -1);
    }
    while (!topLevelProject.productsToHandle.empty())
        handleNextProduct();

    reader.clearExtraSearchPathsStack();
    AccumulatingTimer timer(parameters.logElapsedTime()
                                ? &timingData.propertyChecking : nullptr);
    checkPropertyDeclarations(projectItem, topLevelProject.disabledItems, parameters, logger);
}

void ProjectTreeBuilder::Private::handleProject(Item *projectItem,
                                                const Set<QString> &referencedFilePaths)
{
    auto p = std::make_unique<ProjectContext>();
    auto &projectContext = *p;
    projectContext.topLevelProject = &topLevelProject;
    ItemValuePtr itemValue = ItemValue::create(projectItem);
    projectContext.scope = Item::create(&itemPool, ItemType::Scope);
    projectContext.scope->setFile(projectItem->file());
    projectContext.scope->setProperty(StringConstants::projectVar(), itemValue);
    ProductContext dummyProductContext;
    dummyProductContext.project = &projectContext;
    dummyProductContext.moduleProperties = parameters.finalBuildConfigurationTree();
    dummyProductContext.profileModuleProperties = dummyProductContext.moduleProperties;
    dummyProductContext.profileName = parameters.topLevelProfile();
    dependenciesResolver.loadBaseModule(dummyProductContext, projectItem);

    projectItem->overrideProperties(parameters.overriddenValuesTree(),
                                    StringConstants::projectPrefix(), parameters, logger);
    projectContext.name = evaluator.stringValue(projectItem, StringConstants::nameProperty());
    if (projectContext.name.isEmpty()) {
        projectContext.name = FileInfo::baseName(projectItem->location().filePath());
        projectItem->setProperty(StringConstants::nameProperty(),
                                 VariantValue::create(projectContext.name));
    }
    projectItem->overrideProperties(parameters.overriddenValuesTree(),
                                    StringConstants::projectsOverridePrefix() + projectContext.name,
                                    parameters, logger);
    if (!checkItemCondition(projectItem)) {
        disabledProjects.insert(projectContext.name);
        return;
    }
    topLevelProject.projects.push_back(p.release());
    SearchPathsManager searchPathsManager(reader, reader.readExtraSearchPaths(projectItem, evaluator)
                                                      << projectItem->file()->dirPath());
    projectContext.searchPathsStack = reader.extraSearchPathsStack();
    projectContext.item = projectItem;

    const QString minVersionStr
        = evaluator.stringValue(projectItem, StringConstants::minimumQbsVersionProperty(),
                                 QStringLiteral("1.3.0"));
    const Version minVersion = Version::fromString(minVersionStr);
    if (!minVersion.isValid()) {
        throw ErrorInfo(Tr::tr("The value '%1' of Project.minimumQbsVersion "
                               "is not a valid version string.")
                            .arg(minVersionStr), projectItem->location());
    }
    if (!qbsVersion.isValid())
        qbsVersion = Version::fromString(QLatin1String(QBS_VERSION));
    if (qbsVersion < minVersion) {
        throw ErrorInfo(Tr::tr("The project requires at least qbs version %1, but "
                               "this is qbs version %2.").arg(minVersion.toString(),
                                 qbsVersion.toString()));
    }

    for (Item * const child : projectItem->children())
        child->setScope(projectContext.scope);

    resolveProbes(dummyProductContext, projectItem);
    projectContext.topLevelProject->probes << dummyProductContext.info.probes;

    localProfiles.collectProfilesFromItems(projectItem, projectContext.scope);

    QList<Item *> multiplexedProducts;
    for (Item * const child : projectItem->children()) {
        if (child->type() == ItemType::Product)
            multiplexedProducts << multiplexProductItem(dummyProductContext, child);
    }
    for (Item * const additionalProductItem : std::as_const(multiplexedProducts))
        Item::addChild(projectItem, additionalProductItem);

    const QList<Item *> originalChildren = projectItem->children();
    for (Item * const child : originalChildren) {
        switch (child->type()) {
        case ItemType::Product:
            prepareProduct(projectContext, child);
            break;
        case ItemType::SubProject:
            handleSubProject(projectContext, child, referencedFilePaths);
            break;
        case ItemType::Project:
            copyProperties(projectItem, child);
            handleProject(child, referencedFilePaths);
            break;
        default:
            break;
        }
    }

    const QStringList refs = evaluator.stringListValue(
        projectItem, StringConstants::referencesProperty());
    const CodeLocation referencingLocation
        = projectItem->property(StringConstants::referencesProperty())->location();
    QList<Item *> additionalProjectChildren;
    for (const QString &filePath : refs) {
        try {
            additionalProjectChildren << loadReferencedFile(
                filePath, referencingLocation, referencedFilePaths, dummyProductContext);
        } catch (const ErrorInfo &error) {
            if (parameters.productErrorMode() == ErrorHandlingMode::Strict)
                throw;
            logger.printWarning(error);
        }
    }
    for (Item * const subItem : std::as_const(additionalProjectChildren)) {
        Item::addChild(projectContext.item, subItem);
        switch (subItem->type()) {
        case ItemType::Product:
            prepareProduct(projectContext, subItem);
            break;
        case ItemType::Project:
            copyProperties(projectItem, subItem);
            handleProject(subItem,
                          Set<QString>(referencedFilePaths) << subItem->file()->filePath());
            break;
        default:
            break;
        }
    }

}

void ProjectTreeBuilder::Private::prepareProduct(ProjectContext &projectContext, Item *productItem)
{
    AccumulatingTimer timer(parameters.logElapsedTime()
                                ? &timingData.prepareProducts : nullptr);
    checkCancelation();
    qCDebug(lcModuleLoader) << "prepareProduct" << productItem->file()->filePath();

    ProductContext productContext;
    productContext.item = productItem;
    productContext.project = &projectContext;

    // Retrieve name, profile and multiplex id.
    productContext.name = evaluator.stringValue(productItem, StringConstants::nameProperty());
    QBS_CHECK(!productContext.name.isEmpty());
    const ItemValueConstPtr qbsItemValue = productItem->itemProperty(StringConstants::qbsModule());
    if (qbsItemValue && qbsItemValue->item()->hasProperty(StringConstants::profileProperty())) {
        TempBaseModuleAttacher tbma(this, productContext);
        productContext.profileName = evaluator.stringValue(
            tbma.tempBaseModuleItem(), StringConstants::profileProperty(), QString());
    } else {
        productContext.profileName = parameters.topLevelProfile();
    }
    productContext.multiplexConfigurationId = evaluator.stringValue(
        productItem, StringConstants::multiplexConfigurationIdProperty());
    QBS_CHECK(!productContext.profileName.isEmpty());

    // Set up full module property map based on the profile.
    const auto it = topLevelProject.profileConfigs.constFind(productContext.profileName);
    QVariantMap flatConfig;
    if (it == topLevelProject.profileConfigs.constEnd()) {
        const Profile profile(productContext.profileName, &settings, localProfiles.profiles());
        if (!profile.exists()) {
            ErrorInfo error(Tr::tr("Profile '%1' does not exist.").arg(profile.name()),
                            productItem->location());
            productContext.handleError(error);
            return;
        }
        flatConfig = SetupProjectParameters::expandedBuildConfiguration(
            profile, parameters.configurationName());
        topLevelProject.profileConfigs.insert(productContext.profileName, flatConfig);
    } else {
        flatConfig = it.value().toMap();
    }
    productContext.profileModuleProperties = SetupProjectParameters::finalBuildConfigurationTree(
        flatConfig, {});
    productContext.moduleProperties = SetupProjectParameters::finalBuildConfigurationTree(
        flatConfig, parameters.overriddenValues());
    initProductProperties(productContext);

    // Set up product scope. This is mainly for using the "product" and "project"
    // variables in some contexts.
    ItemValuePtr itemValue = ItemValue::create(productItem);
    productContext.scope = Item::create(&itemPool, ItemType::Scope);
    productContext.scope->setProperty(StringConstants::productVar(), itemValue);
    productContext.scope->setFile(productItem->file());
    productContext.scope->setScope(productContext.project->scope);

    const bool hasExportItems = mergeExportItems(productContext);

    setScopeForDescendants(productItem, productContext.scope);

    projectContext.products.push_back(productContext);

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
    importer->setScope(projectContext.scope);
    importer->setupForBuiltinType(parameters.deprecationWarningMode(), logger);
    Item * const dependsItem = Item::create(productItem->pool(), ItemType::Depends);
    dependsItem->setProperty(QStringLiteral("name"), VariantValue::create(productContext.name));
    dependsItem->setProperty(QStringLiteral("required"), VariantValue::create(false));
    dependsItem->setFile(importer->file());
    dependsItem->setLocation(importer->location());
    dependsItem->setupForBuiltinType(parameters.deprecationWarningMode(), logger);
    Item::addChild(importer, dependsItem);
    Item::addChild(productItem, importer);
    prepareProduct(projectContext, importer);
}

void ProjectTreeBuilder::Private::handleNextProduct()
{
    auto [product, queueSizeOnInsert] = topLevelProject.productsToHandle.front();
    topLevelProject.productsToHandle.pop_front();

    // If the queue of in-progress products has shrunk since the last time we tried handling
    // this product, there has been forward progress and we can allow a deferral.
    const Deferral deferral = queueSizeOnInsert == -1
                                      || queueSizeOnInsert > int(topLevelProject.productsToHandle.size())
                                  ? Deferral::Allowed : Deferral::NotAllowed;

    reader.setExtraSearchPathsStack(product->project->searchPathsStack);
    try {
        handleProduct(*product, deferral);
        if (product->name.startsWith(StringConstants::shadowProductPrefix()))
            topLevelProject.probes << product->info.probes;
    } catch (const ErrorInfo &err) {
        product->handleError(err);
    }

    // The search paths stack can change during dependency resolution (due to module providers);
    // check that we've rolled back all the changes
    QBS_CHECK(reader.extraSearchPathsStack() == product->project->searchPathsStack);

    // If we encountered a dependency to an in-progress product or to a bulk dependency,
    // we defer handling this product if it hasn't failed yet and there is still forward progress.
    if (!product->info.delayedError.hasError() && !product->dependenciesResolved) {
        topLevelProject.productsToHandle.emplace_back(
            product, int(topLevelProject.productsToHandle.size()));
    }
}

void ProjectTreeBuilder::Private::handleProduct(ProductContext &product, Deferral deferral)
{
    checkCancelation();

    AccumulatingTimer timer(parameters.logElapsedTime() ? &timingData.handleProducts : nullptr);
    if (product.info.delayedError.hasError())
        return;

    product.dependenciesResolved = dependenciesResolver.resolveDependencies(product, deferral);
    if (!product.dependenciesResolved)
        return;

    // Run probes for modules and product.
    for (const Item::Module &module : product.item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        if (module.productInfo && topLevelProject.disabledItems.contains(module.productInfo->item)) {
            createNonPresentModule(itemPool, module.name.toString(),
                                   QLatin1String("module's exporting product is disabled"),
                                   module.item);
            continue;
        }
        try {
            resolveProbes(product, module.item);
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
                    evaluator.stringValue(module.item,
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
            handleModuleSetupError(product, module, error);
            if (product.info.delayedError.hasError())
                return;
        }
    }
    resolveProbes(product, product.item);

    // After the probes have run, we can switch on the evaluator cache.
    FileTags fileTags = evaluator.fileTagsValue(product.item, StringConstants::typeProperty());
    EvalCacheEnabler cacheEnabler(&evaluator, evaluator.stringValue(
                                                  product.item,
                                                  StringConstants::sourceDirectoryProperty()));

    // Run module validation scripts.
    for (const Item::Module &module : product.item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        try {
            evaluator.boolValue(module.item, StringConstants::validateProperty());
            for (const auto &dep : module.item->modules()) {
                if (dep.required && !dep.item->isPresentModule()) {
                    throw ErrorInfo(Tr::tr("Module '%1' depends on module '%2', which was not "
                                           "loaded successfully")
                                        .arg(module.name.toString(), dep.name.toString()));
                }
            }
            fileTags += evaluator.fileTagsValue(
                module.item, StringConstants::additionalProductTypesProperty());
        } catch (const ErrorInfo &error) {
            handleModuleSetupError(product, module, error);
            if (product.info.delayedError.hasError())
                return;
        }
    }

    // Disable modules that have been pulled in only by now-disabled modules.
    // Note that this has to happen in the reverse order compared to the other loops,
    // with the leaves checked last.
    for (auto it = product.item->modules().rbegin(); it != product.item->modules().rend(); ++it) {
        const Item::Module &module = *it;
        if (!module.item->isPresentModule())
            continue;
        bool hasPresentLoadingItem = false;
        for (const Item * const loadingItem : module.loadingItems) {
            if (loadingItem == product.item) {
                hasPresentLoadingItem = true;
                break;
            }
            if (!loadingItem->isPresentModule())
                continue;
            if (loadingItem->prototype() && loadingItem->prototype()->type() == ItemType::Export) {
                QBS_CHECK(loadingItem->prototype()->parent()->type() == ItemType::Product);
                if (topLevelProject.disabledItems.contains(loadingItem->prototype()->parent()))
                    continue;
            }
            hasPresentLoadingItem = true;
            break;
        }
        if (!hasPresentLoadingItem) {
            createNonPresentModule(itemPool, module.name.toString(),
                                   QLatin1String("imported only by disabled module(s)"),
                                   module.item);
            continue;
        }
    }

    // Now do the canonical module property values merge. Note that this will remove
    // previously attached values from modules that failed validation.
    // Evaluator cache entries that could potentially change due to this will be purged.
    propertyMerger.doFinalMerge(product.item);

    const bool enabled = checkItemCondition(product.item);
    dependenciesResolver.checkDependencyParameterDeclarations(product.item, product.name);

    groupsHandler.setupGroups(product.item, product.scope);
    product.info.modulePropertiesSetInGroups = groupsHandler.modulePropertiesSetInGroups();
    topLevelProject.disabledItems.unite(groupsHandler.disabledGroups());

    // Collect the full list of fileTags, including the values contributed by modules.
    if (!product.info.delayedError.hasError() && enabled
        && !product.name.startsWith(StringConstants::shadowProductPrefix())) {
        for (const FileTag &tag : fileTags)
            topLevelProject.productsByType.insert({tag, &product});
        product.item->setProperty(StringConstants::typeProperty(),
                                  VariantValue::create(sorted(fileTags.toStringList())));
    }
    topLevelProject.productInfos[product.item] = product.info;
}

void ProjectTreeBuilder::Private::handleSubProject(
    ProjectContext &projectContext, Item *projectItem, const Set<QString> &referencedFilePaths)
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

    Item *loadedItem = nullptr;
    QString subProjectFilePath;
    try {
        const QString projectFileDirPath = FileInfo::path(projectItem->file()->filePath());
        const QString relativeFilePath
            = evaluator.stringValue(projectItem, StringConstants::filePathProperty());
        subProjectFilePath = FileInfo::resolvePath(projectFileDirPath, relativeFilePath);
        if (referencedFilePaths.contains(subProjectFilePath))
            throw ErrorInfo(Tr::tr("Cycle detected while loading subproject file '%1'.")
                                .arg(relativeFilePath), projectItem->location());
        loadedItem = loadItemFromFile(subProjectFilePath, projectItem->location());
    } catch (const ErrorInfo &error) {
        if (parameters.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        logger.printWarning(error);
        return;
    }

    loadedItem = wrapInProjectIfNecessary(loadedItem);
    const bool inheritProperties = evaluator.boolValue(
        projectItem, StringConstants::inheritPropertiesProperty());

    if (inheritProperties)
        copyProperties(projectItem->parent(), loadedItem);
    if (propertiesItem) {
        const Item::PropertyMap &overriddenProperties = propertiesItem->properties();
        for (auto it = overriddenProperties.begin(); it != overriddenProperties.end(); ++it)
            loadedItem->setProperty(it.key(), it.value());
    }

    Item::addChild(projectItem, loadedItem);
    projectItem->setScope(projectContext.scope);
    handleProject(loadedItem, Set<QString>(referencedFilePaths) << subProjectFilePath);
}

void ProjectTreeBuilder::Private::initProductProperties(const ProductContext &product)
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

void ProjectTreeBuilder::Private::printProfilingInfo()
{
    if (!parameters.logElapsedTime())
        return;
    logger.qbsLog(LoggerInfo, true) << "  "
                                    << Tr::tr("Project file loading and parsing took %1.")
                                           .arg(elapsedTimeString(reader.elapsedTime()));
    logger.qbsLog(LoggerInfo, true) << "  "
                                    << Tr::tr("Preparing products took %1.")
                                           .arg(elapsedTimeString(timingData.prepareProducts));
    logger.qbsLog(LoggerInfo, true) << "  "
                                    << Tr::tr("Handling products took %1.")
                                           .arg(elapsedTimeString(timingData.handleProducts));
    dependenciesResolver.printProfilingInfo(4);
    moduleInstantiator.printProfilingInfo(6);
    propertyMerger.printProfilingInfo(6);
    groupsHandler.printProfilingInfo(4);
    probesResolver.printProfilingInfo(4);
    logger.qbsLog(LoggerInfo, true) << "  "
                                    << Tr::tr("Property checking took %1.")
                                           .arg(elapsedTimeString(timingData.propertyChecking));
}

void ProjectTreeBuilder::Private::checkProjectNamesInOverrides()
{
    for (const QString &projectNameInOverride : projectNamesUsedInOverrides) {
        if (disabledProjects.contains(projectNameInOverride))
            continue;
        if (!any_of(topLevelProject.projects, [&projectNameInOverride](const ProjectContext *p) {
                return p->name == projectNameInOverride; })) {
            handlePropertyError(Tr::tr("Unknown project '%1' in property override.")
                                    .arg(projectNameInOverride), parameters, logger);
        }
    }
}

void ProjectTreeBuilder::Private::checkProductNamesInOverrides()
{
    for (const QString &productNameInOverride : productNamesUsedInOverrides) {
        if (topLevelProject.erroneousProducts.contains(productNameInOverride))
            continue;
        if (!any_of(topLevelProject.productsByName, [&productNameInOverride](
                                        const std::pair<QString, ProductContext *> &elem) {
                // In an override string such as "a.b.c:d, we cannot tell whether we have a product
                // "a" and a module "b.c" or a product "a.b" and a module "c", so we need to take
                // care not to emit false positives here.
                return elem.first == productNameInOverride
                       || elem.first.startsWith(productNameInOverride + StringConstants::dot());
            })) {
            handlePropertyError(Tr::tr("Unknown product '%1' in property override.")
                                    .arg(productNameInOverride), parameters, logger);
        }
    }
}

void ProjectTreeBuilder::Private::collectProductsByName()
{
    for (ProjectContext * const project : topLevelProject.projects) {
        for (ProductContext &product : project->products)
            topLevelProject.productsByName.insert({product.name, &product});
    }
}

void ProjectTreeBuilder::Private::handleModuleSetupError(
    ProductContext &product, const Item::Module &module, const ErrorInfo &error)
{
    if (module.required) {
        product.handleError(error);
    } else {
        qCDebug(lcModuleLoader()) << "non-required module" << module.name.toString()
                                  << "found, but not usable in product" << product.name
                                  << error.toString();
        createNonPresentModule(itemPool, module.name.toString(),
                               QStringLiteral("failed validation"), module.item);
    }
}

bool ProjectTreeBuilder::Private::checkItemCondition(Item *item)
{
    return topLevelProject.checkItemCondition(item, evaluator);
}

QList<Item *> ProjectTreeBuilder::Private::multiplexProductItem(ProductContext &dummyContext,
                                                                Item *productItem)
{
    // Overriding the product item properties must be done here already, because multiplexing
    // properties might depend on product properties.
    const QString &nameKey = StringConstants::nameProperty();
    QString productName = evaluator.stringValue(productItem, nameKey);
    if (productName.isEmpty()) {
        productName = FileInfo::completeBaseName(productItem->file()->filePath());
        productItem->setProperty(nameKey, VariantValue::create(productName));
    }
    productItem->overrideProperties(parameters.overriddenValuesTree(),
                                    StringConstants::productsOverridePrefix() + productName,
                                    parameters, logger);
    dummyContext.item = productItem;
    TempBaseModuleAttacher tbma(this, dummyContext);
    return multiplexer.multiplex(productName, productItem, tbma.tempBaseModuleItem(),
                                 [&] { tbma.drop(); });
}

void ProjectTreeBuilder::Private::checkCancelation() const
{
    if (progressObserver && progressObserver->canceled()) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration %1.")
                            .arg(TopLevelProject::deriveId(
                                parameters.finalBuildConfigurationTree())));
    }
}

QList<Item *> ProjectTreeBuilder::Private::loadReferencedFile(
    const QString &relativePath, const CodeLocation &referencingLocation,
    const Set<QString> &referencedFilePaths, ProductContext &dummyContext)
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
        localProfiles.collectProfilesFromItems(subItem, dummyContext.project->scope);
        loadedItems << multiplexProductItem(dummyContext, subItem);
    }
    return loadedItems;
}

void ProjectTreeBuilder::Private::copyProperties(const Item *sourceProject, Item *targetProject)
{
    if (!sourceProject)
        return;
    const QList<PropertyDeclaration> builtinProjectProperties
        = BuiltinDeclarations::instance().declarationsForType(ItemType::Project).properties();
    Set<QString> builtinProjectPropertyNames;
    for (const PropertyDeclaration &p : builtinProjectProperties)
        builtinProjectPropertyNames << p.name();

    for (auto it = sourceProject->propertyDeclarations().begin();
         it != sourceProject->propertyDeclarations().end(); ++it) {

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

static void mergeProperty(Item *dst, const QString &name, const ValuePtr &value)
{
    if (value->type() == Value::ItemValueType) {
        const ItemValueConstPtr itemValue = std::static_pointer_cast<ItemValue>(value);
        const Item * const valueItem = itemValue->item();
        Item * const subItem = dst->itemProperty(name, itemValue)->item();
        for (auto it = valueItem->properties().begin(); it != valueItem->properties().end(); ++it)
            mergeProperty(subItem, it.key(), it.value());
        return;
    }

    // If the property already exists, set up the base value.
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

bool ProjectTreeBuilder::Private::mergeExportItems(ProductContext &productContext)
{
    std::vector<Item *> exportItems;
    QList<Item *> children = productContext.item->children();

    const auto isExport = [](Item *item) { return item->type() == ItemType::Export; };
    std::copy_if(children.cbegin(), children.cend(), std::back_inserter(exportItems), isExport);
    qbs::Internal::removeIf(children, isExport);

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
    QVariantMap defaultParameters;
    for (Item * const exportItem : std::as_const(exportItems)) {
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
                mergeParameters(defaultParameters,
                                getJsVariant(evaluator.engine()->context(),
                                             evaluator.scriptValue(child)).toMap());
            } else {
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
                handlePropertyError(error, parameters, logger);
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
    merged->setupForBuiltinType(parameters.deprecationWarningMode(), logger);

    productContext.mergedExportItem = merged;
    productContext.defaultParameters = defaultParameters;
    return !exportItems.empty();
}

// TODO: This seems dubious. Can we merge the conditions instead?
bool ProjectTreeBuilder::Private::checkExportItemCondition(Item *exportItem,
                                                           const ProductContext &product)
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
    } scopeHandler(exportItem, product, &tempScopeItem);
    return checkItemCondition(exportItem);
}

void ProjectTreeBuilder::Private::resolveProbes(ProductContext &product, Item *item)
{
    product.info.probes << probesResolver.resolveProbes({product.name, product.uniqueName()}, item);
}

ProjectTreeBuilder::Private::TempBaseModuleAttacher::TempBaseModuleAttacher(
    ProjectTreeBuilder::Private *d, ProductContext &product)
    : m_productItem(product.item)
{
    const ValuePtr qbsValue = m_productItem->property(StringConstants::qbsModule());

    // Cloning is necessary because the original value will get "instantiated" now.
    if (qbsValue)
        m_origQbsValue = qbsValue->clone();

    m_tempBaseModule = d->dependenciesResolver.loadBaseModule(product, m_productItem);
}

void ProjectTreeBuilder::Private::TempBaseModuleAttacher::drop()
{
    if (!m_tempBaseModule)
        return;

    // "Unload" the qbs module again.
    if (m_origQbsValue)
        m_productItem->setProperty(StringConstants::qbsModule(), m_origQbsValue);
    else
        m_productItem->removeProperty(StringConstants::qbsModule());
    m_productItem->removeModules();
    m_tempBaseModule = nullptr;
}

} // namespace qbs::Internal
