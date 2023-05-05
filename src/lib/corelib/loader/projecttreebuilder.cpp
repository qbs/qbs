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

#include "groupshandler.h"
#include "itemreader.h"
#include "localprofiles.h"
#include "moduleinstantiator.h"
#include "moduleloader.h"
#include "modulepropertymerger.h"
#include "probesresolver.h"
#include "productitemmultiplexer.h"

#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/filetags.h>
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

class ContextBase
{
public:
    Item *item = nullptr;
    Item *scope = nullptr;
    QString name;
};

class ProjectContext;

class ProductContext : public ContextBase
{
public:
    ProjectContext *project = nullptr;
    Item *mergedExportItem = nullptr;
    ProjectTreeBuilder::Result::ProductInfo info;
    QString profileName;
    QString multiplexConfigurationId;
    QVariantMap profileModuleProperties; // Tree-ified module properties from profile.
    QVariantMap moduleProperties;        // Tree-ified module properties from profile + overridden values.
    QVariantMap defaultParameters; // In Export item.
    QStringList searchPaths;

    struct ResolvedDependsItem {
        Item *item = nullptr;
        QualifiedId name;
        QStringList subModules;
        FileTags productTypes;
        QStringList multiplexIds;
        std::optional<QStringList> profiles;
        VersionRange versionRange;
        QVariantMap parameters;
        bool limitToSubProject = false;
        FallbackMode fallbackMode = FallbackMode::Enabled;
        bool requiredLocally = true;
        bool requiredGlobally = true;
    };
    struct ResolvedAndMultiplexedDependsItem {
        ResolvedAndMultiplexedDependsItem(ProductContext *product,
                                          const ResolvedDependsItem &dependency)
            : product(product), item(dependency.item), name(product->name),
            versionRange(dependency.versionRange), parameters(dependency.parameters),
            fallbackMode(FallbackMode::Disabled), checkProduct(false) {}
        ResolvedAndMultiplexedDependsItem(const ResolvedDependsItem &dependency,
                                          QualifiedId name, QString profile, QString multiplexId)
            : item(dependency.item), name(std::move(name)), profile(std::move(profile)),
            multiplexId(std::move(multiplexId)),
            versionRange(dependency.versionRange), parameters(dependency.parameters),
            limitToSubProject(dependency.limitToSubProject), fallbackMode(dependency.fallbackMode),
            requiredLocally(dependency.requiredLocally),
            requiredGlobally(dependency.requiredGlobally) {}
        ResolvedAndMultiplexedDependsItem() = default;
        static ResolvedAndMultiplexedDependsItem makeBaseDependency() {
            ResolvedAndMultiplexedDependsItem item;
            item.fallbackMode = FallbackMode::Disabled;
            item.name = StringConstants::qbsModule();
            return item;
        }

        QString id() const;
        CodeLocation location() const;
        QString displayName() const;

        ProductContext *product = nullptr;
        Item *item = nullptr;
        QualifiedId name;
        QString profile;
        QString multiplexId;
        VersionRange versionRange;
        QVariantMap parameters;
        bool limitToSubProject = false;
        FallbackMode fallbackMode = FallbackMode::Enabled;
        bool requiredLocally = true;
        bool requiredGlobally = true;
        bool checkProduct = true;
    };
    struct DependenciesResolvingState {
        Item *loadingItem = nullptr;
        ResolvedAndMultiplexedDependsItem loadingItemOrigin;
        std::queue<Item *> pendingDependsItems;
        std::optional<ResolvedDependsItem> currentDependsItem;
        std::queue<ResolvedAndMultiplexedDependsItem> pendingResolvedDependencies;
        bool requiredByLoadingItem = true;
    };
    std::list<DependenciesResolvingState> resolveDependenciesState;

    QString uniqueName() const;
};

class TopLevelProjectContext
{
public:
    TopLevelProjectContext() = default;
    TopLevelProjectContext(const TopLevelProjectContext &) = delete;
    TopLevelProjectContext &operator=(const TopLevelProjectContext &) = delete;
    ~TopLevelProjectContext() { qDeleteAll(projects); }

    std::vector<ProjectContext *> projects;
    std::list<std::pair<ProductContext *, int>> productsToHandle;
    std::vector<ProbeConstPtr> probes;
    QString buildDirectory;
};

class ProjectContext : public ContextBase
{
public:
    TopLevelProjectContext *topLevelProject = nullptr;
    ProjectTreeBuilder::Result *result = nullptr;
    std::vector<ProductContext> products;
    std::vector<QStringList> searchPathsStack;
};


using ShadowProductInfo = std::pair<bool, QString>;
enum class Deferral { Allowed, NotAllowed };
enum class HandleDependency { Use, Ignore, Defer };

class TimingData {
public:
    qint64 prepareProducts = 0;
    qint64 productDependencies = 0;
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
    void handleTopLevelProject(Result &loadResult, const Set<QString> &referencedFilePaths);
    void handleProject(Result *loadResult, TopLevelProjectContext *topLevelProjectContext,
                       Item *projectItem, const Set<QString> &referencedFilePaths);
    void prepareProduct(ProjectContext &projectContext, Item *productItem);
    void handleNextProduct(TopLevelProjectContext &tlp);
    void handleProduct(ProductContext &productContext, Deferral deferral);
    bool resolveDependencies(ProductContext &product, Deferral deferral);
    void setSearchPathsForProduct(ProductContext &product);
    void handleSubProject(ProjectContext &projectContext, Item *projectItem,
                          const Set<QString> &referencedFilePaths);
    void initProductProperties(const ProductContext &product);
    void printProfilingInfo();
    void checkProjectNamesInOverrides(const TopLevelProjectContext &tlp);
    void checkProductNamesInOverrides();
    void collectProductsByName(const TopLevelProjectContext &topLevelProject);
    void adjustDependsItemForMultiplexing(const ProductContext &product, Item *dependsItem);
    ShadowProductInfo getShadowProductInfo(const ProductContext &product) const;
    void handleProductError(const ErrorInfo &error, ProductContext &productContext);
    void handleModuleSetupError(ProductContext &product, const Item::Module &module,
                                const ErrorInfo &error);
    bool checkItemCondition(Item *item);
    QStringList readExtraSearchPaths(Item *item, bool *wasSet = nullptr);
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

    Item *loadBaseModule(ProductContext &product, Item *item);

    struct LoadModuleResult {
        Item *moduleItem = nullptr;
        ProductContext *product = nullptr;
        HandleDependency handleDependency = HandleDependency::Use;
    };
    LoadModuleResult loadModule(ProductContext &product, Item *loadingItem,
                                const ProductContext::ResolvedAndMultiplexedDependsItem &dependency,
                                Deferral deferral);

    std::optional<ProductContext::ResolvedDependsItem>
    resolveDependsItem(const ProductContext &product, Item *dependsItem);
    std::queue<ProductContext::ResolvedAndMultiplexedDependsItem>
    multiplexDependency(const ProductContext &product,
                        const ProductContext::ResolvedDependsItem &dependency);
    static bool haveSameSubProject(const ProductContext &p1, const ProductContext &p2);
    QVariantMap extractParameters(Item *dependsItem) const;

    const SetupProjectParameters &parameters;
    ItemPool &itemPool;
    Evaluator &evaluator;
    Logger &logger;
    ProgressObserver *progressObserver = nullptr;
    TimingData timingData;
    ItemReader reader{parameters, logger};
    ProbesResolver probesResolver{parameters, evaluator, logger};
    ModuleProviderLoader moduleProviderLoader{parameters, reader, evaluator, probesResolver, logger};
    ModuleLoader moduleLoader{parameters, moduleProviderLoader, reader, evaluator, logger};
    ModulePropertyMerger propertyMerger{parameters, evaluator, logger};
    ModuleInstantiator moduleInstantiator{parameters, itemPool, propertyMerger, logger};
    ProductItemMultiplexer multiplexer{parameters, evaluator, logger, [this](Item *productItem) {
                                           return moduleInstantiator.retrieveQbsItem(productItem);
                                       }};
    GroupsHandler groupsHandler{parameters, moduleInstantiator, evaluator, logger};
    LocalProfiles localProfiles{parameters, evaluator, logger};
    FileTime lastResolveTime;
    QVariantMap storedProfiles;

    Set<Item *> disabledItems;
    Settings settings{parameters.settingsDirectory()};
    Set<QString> projectNamesUsedInOverrides;
    Set<QString> productNamesUsedInOverrides;
    Set<QString> disabledProjects;
    Set<QString> erroneousProducts;
    std::multimap<QString, ProductContext *> productsByName;

    // For fast look-up when resolving Depends.productTypes.
    // The contract is that it contains fully handled, error-free, enabled products.
    std::multimap<FileTag, ProductContext *> productsByType;

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
    d->moduleProviderLoader.setStoredModuleProviderInfo(moduleProviderInfo);
}

ProjectTreeBuilder::Result ProjectTreeBuilder::load()
{
    TimedActivityLogger mainTimer(d->logger, Tr::tr("ProjectTreeBuilder"),
                                  d->parameters.logElapsedTime());
    qCDebug(lcModuleLoader) << "load" << d->parameters.projectFilePath();

    d->checkOverriddenValues();
    d->reader.setPool(&d->itemPool);

    Result result;
    result.profileConfigs = d->storedProfiles;
    result.root = d->loadTopLevelProjectItem();
    d->handleTopLevelProject(result, {QDir::cleanPath(d->parameters.projectFilePath())});

    result.qbsFiles = d->reader.filesRead() - d->moduleProviderLoader.tempQbsFiles();
    for (auto it = d->localProfiles.profiles().begin(); it != d->localProfiles.profiles().end();
         ++it) {
        result.profileConfigs.remove(it.key());
    }

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

void ProjectTreeBuilder::Private::handleTopLevelProject(Result &loadResult,
                                                        const Set<QString> &referencedFilePaths)
{
    TopLevelProjectContext tlp;
    tlp.buildDirectory = TopLevelProject::deriveBuildDirectory(
        parameters.buildRoot(),
        TopLevelProject::deriveId(parameters.finalBuildConfigurationTree()));
    Item * const projectItem = loadResult.root;
    projectItem->setProperty(StringConstants::sourceDirectoryProperty(),
                             VariantValue::create(QFileInfo(projectItem->file()->filePath())
                                                      .absolutePath()));
    projectItem->setProperty(StringConstants::buildDirectoryProperty(),
                             VariantValue::create(tlp.buildDirectory));
    projectItem->setProperty(StringConstants::profileProperty(),
                             VariantValue::create(parameters.topLevelProfile()));
    handleProject(&loadResult, &tlp, projectItem, referencedFilePaths);
    checkProjectNamesInOverrides(tlp);
    collectProductsByName(tlp);
    checkProductNamesInOverrides();

    for (ProjectContext * const projectContext : qAsConst(tlp.projects)) {
        for (ProductContext &productContext : projectContext->products)
            tlp.productsToHandle.emplace_back(&productContext, -1);
    }
    while (!tlp.productsToHandle.empty())
        handleNextProduct(tlp);

    loadResult.projectProbes = tlp.probes;
    loadResult.storedModuleProviderInfo = moduleProviderLoader.storedModuleProviderInfo();

    reader.clearExtraSearchPathsStack();
    AccumulatingTimer timer(parameters.logElapsedTime()
                                ? &timingData.propertyChecking : nullptr);
    checkPropertyDeclarations(projectItem, disabledItems, parameters, logger);
}

void ProjectTreeBuilder::Private::handleProject(
    Result *loadResult, TopLevelProjectContext *topLevelProjectContext, Item *projectItem,
    const Set<QString> &referencedFilePaths)
{
    auto p = std::make_unique<ProjectContext>();
    auto &projectContext = *p;
    projectContext.topLevelProject = topLevelProjectContext;
    projectContext.result = loadResult;
    ItemValuePtr itemValue = ItemValue::create(projectItem);
    projectContext.scope = Item::create(&itemPool, ItemType::Scope);
    projectContext.scope->setFile(projectItem->file());
    projectContext.scope->setProperty(StringConstants::projectVar(), itemValue);
    ProductContext dummyProductContext;
    dummyProductContext.project = &projectContext;
    dummyProductContext.moduleProperties = parameters.finalBuildConfigurationTree();
    dummyProductContext.profileModuleProperties = dummyProductContext.moduleProperties;
    dummyProductContext.profileName = parameters.topLevelProfile();
    loadBaseModule(dummyProductContext, projectItem);

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
    topLevelProjectContext->projects.push_back(p.release());
    SearchPathsManager searchPathsManager(reader, readExtraSearchPaths(projectItem)
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
    for (Item * const additionalProductItem : qAsConst(multiplexedProducts))
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
            handleProject(loadResult, topLevelProjectContext, child, referencedFilePaths);
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
    for (Item * const subItem : qAsConst(additionalProjectChildren)) {
        Item::addChild(projectContext.item, subItem);
        switch (subItem->type()) {
        case ItemType::Product:
            prepareProduct(projectContext, subItem);
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
    const auto it = projectContext.result->profileConfigs.constFind(productContext.profileName);
    QVariantMap flatConfig;
    if (it == projectContext.result->profileConfigs.constEnd()) {
        const Profile profile(productContext.profileName, &settings, localProfiles.profiles());
        if (!profile.exists()) {
            ErrorInfo error(Tr::tr("Profile '%1' does not exist.").arg(profile.name()),
                            productItem->location());
            handleProductError(error, productContext);
            return;
        }
        flatConfig = SetupProjectParameters::expandedBuildConfiguration(
            profile, parameters.configurationName());
        projectContext.result->profileConfigs.insert(productContext.profileName, flatConfig);
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

void ProjectTreeBuilder::Private::handleNextProduct(TopLevelProjectContext &tlp)
{
    auto [product, queueSizeOnInsert] = tlp.productsToHandle.front();
    tlp.productsToHandle.pop_front();

    // If the queue of in-progress products has shrunk since the last time we tried handling
    // this product, there has been forward progress and we can allow a deferral.
    const Deferral deferral = queueSizeOnInsert == -1
                                      || queueSizeOnInsert > int(tlp.productsToHandle.size())
                                  ? Deferral::Allowed : Deferral::NotAllowed;

    reader.setExtraSearchPathsStack(product->project->searchPathsStack);
    try {
        handleProduct(*product, deferral);
        if (product->name.startsWith(StringConstants::shadowProductPrefix()))
            tlp.probes << product->info.probes;
    } catch (const ErrorInfo &err) {
        handleProductError(err, *product);
    }

    // The search paths stack can change during dependency resolution (due to module providers);
    // check that we've rolled back all the changes
    QBS_CHECK(reader.extraSearchPathsStack() == product->project->searchPathsStack);

    // If we encountered a dependency to an in-progress product or to a bulk dependency,
    // we defer handling this product if it hasn't failed yet and there is still forward progress.
    if (!product->info.delayedError.hasError() && !product->resolveDependenciesState.empty())
        tlp.productsToHandle.emplace_back(product, int(tlp.productsToHandle.size()));
}

void ProjectTreeBuilder::Private::handleProduct(ProductContext &product, Deferral deferral)
{
    checkCancelation();

    AccumulatingTimer timer(parameters.logElapsedTime() ? &timingData.handleProducts : nullptr);
    if (product.info.delayedError.hasError())
        return;

    if (!resolveDependencies(product, deferral))
        return;

    // Run probes for modules and product.
    for (const Item::Module &module : product.item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        if (module.productInfo && disabledItems.contains(module.productInfo->item)) {
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
                if (disabledItems.contains(loadingItem->prototype()->parent()))
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
    moduleLoader.checkDependencyParameterDeclarations(product.item, product.name);

    groupsHandler.setupGroups(product.item, product.scope);
    product.info.modulePropertiesSetInGroups = groupsHandler.modulePropertiesSetInGroups();
    disabledItems.unite(groupsHandler.disabledGroups());

    // Collect the full list of fileTags, including the values contributed by modules.
    if (!product.info.delayedError.hasError() && enabled) {
        for (const FileTag &tag : fileTags)
            productsByType.insert({tag, &product});
        product.item->setProperty(StringConstants::typeProperty(),
                                  VariantValue::create(sorted(fileTags.toStringList())));
    }
    product.project->result->productInfos[product.item] = product.info;
}

bool ProjectTreeBuilder::Private::resolveDependencies(ProductContext &product, Deferral deferral)
{
    AccumulatingTimer timer(parameters.logElapsedTime()
                                ? &timingData.productDependencies : nullptr);

    // Initialize the state with the direct Depends items of the product item.
    // This branch is executed once per product, while the function might be entered
    // multiple times due to deferrals.
    if (product.resolveDependenciesState.empty()) {
        setSearchPathsForProduct(product);
        std::queue<Item *> topLevelDependsItems;
        for (Item * const child : product.item->children()) {
            if (child->type() == ItemType::Depends)
                topLevelDependsItems.push(child);
        }
        product.resolveDependenciesState.push_front({product.item, {}, topLevelDependsItems, });
        product.resolveDependenciesState.front().pendingResolvedDependencies.push(
                    ProductContext::ResolvedAndMultiplexedDependsItem::makeBaseDependency());
    }

    SearchPathsManager searchPathsMgr(reader, product.searchPaths);

    while (!product.resolveDependenciesState.empty()) {
fixme:
        auto &state = product.resolveDependenciesState.front();
        while (!state.pendingResolvedDependencies.empty()) {
            QBS_CHECK(!state.currentDependsItem);
            const auto dependency = state.pendingResolvedDependencies.front();
            try {
                const LoadModuleResult res = loadModule(product, state.loadingItem, dependency,
                                                        deferral);
                switch (res.handleDependency) {
                case HandleDependency::Defer:
                    QBS_CHECK(deferral == Deferral::Allowed);
                    if (res.product)
                        state.pendingResolvedDependencies.front().product = res.product;
                    return false;
                case HandleDependency::Ignore:
                    state.pendingResolvedDependencies.pop();
                    continue;
                case HandleDependency::Use:
                    if (dependency.name.toString() == StringConstants::qbsModule()) {
                        state.pendingResolvedDependencies.pop();
                        continue;
                    }
                    break;
                }

                QBS_CHECK(res.moduleItem);
                std::queue<Item *> moduleDependsItems;
                for (Item * const child : res.moduleItem->children()) {
                    if (child->type() == ItemType::Depends)
                        moduleDependsItems.push(child);
                }

                state.pendingResolvedDependencies.pop();
                product.resolveDependenciesState.push_front(
                    {res.moduleItem, dependency, moduleDependsItems, {}, {},
                     dependency.requiredGlobally || state.requiredByLoadingItem});
                product.resolveDependenciesState.front().pendingResolvedDependencies.push(
                    ProductContext::ResolvedAndMultiplexedDependsItem::makeBaseDependency());
                break;
            } catch (const ErrorInfo &e) {
                if (dependency.name.toString() == StringConstants::qbsModule())
                    throw e;

                if (!dependency.requiredLocally) {
                    state.pendingResolvedDependencies.pop();
                    continue;
                }

                // See QBS-1338 for why we do not abort handling the product.
                state.pendingResolvedDependencies.pop();
                Item::Modules &modules = product.item->modules();
                while (product.resolveDependenciesState.size() > 1) {
                    const auto loadingItemModule = std::find_if(
                        modules.begin(), modules.end(), [&](const Item::Module &m) {
                            return m.item == product.resolveDependenciesState.front().loadingItem;
                        });
                    for (auto it = loadingItemModule; it != modules.end(); ++it) {
                        createNonPresentModule(itemPool, it->name.toString(),
                                               QLatin1String("error in Depends chain"), it->item);
                    }
                    modules.erase(loadingItemModule, modules.end());
                    product.resolveDependenciesState.pop_front();
                }
                handleProductError(e, product);
                goto fixme;
            }
        }
        if (&state != &product.resolveDependenciesState.front())
            continue;

        if (state.currentDependsItem) {
            QBS_CHECK(state.pendingResolvedDependencies.empty());

            // We postpone handling Depends.productTypes for as long as possible, because
            // the full type of a product becomes available only after its modules have been loaded.
            if (!state.currentDependsItem->productTypes.empty() && deferral == Deferral::Allowed)
                return false;

            state.pendingResolvedDependencies = multiplexDependency(product,
                                                                    *state.currentDependsItem);
            state.currentDependsItem.reset();
            continue;
        }

        while (!state.pendingDependsItems.empty()) {
            QBS_CHECK(!state.currentDependsItem);
            QBS_CHECK(state.pendingResolvedDependencies.empty());
            Item * const dependsItem = state.pendingDependsItems.front();
            state.pendingDependsItems.pop();
            adjustDependsItemForMultiplexing(product, dependsItem);
            state.currentDependsItem = resolveDependsItem(product, dependsItem);
            if (!state.currentDependsItem)
                continue;
            state.currentDependsItem->requiredGlobally = state.currentDependsItem->requiredLocally
                                                         && state.loadingItemOrigin.requiredGlobally;
            goto fixme;
        }

        QBS_CHECK(!state.currentDependsItem);
        QBS_CHECK(state.pendingResolvedDependencies.empty());
        QBS_CHECK(state.pendingDependsItems.empty());

        // This ensures a sorted module list in the product (dependers after dependencies).
        if (product.resolveDependenciesState.size() > 1) {
            QBS_CHECK(state.loadingItem->type() == ItemType::ModuleInstance);
            Item::Modules &modules = product.item->modules();
            const auto loadingItemModule = std::find_if(modules.begin(), modules.end(),
                                                        [&](const Item::Module &m) {
                return m.item == state.loadingItem;
            });
            QBS_CHECK(loadingItemModule != modules.end());
            const Item::Module tempModule = *loadingItemModule;
            modules.erase(loadingItemModule);
            modules.push_back(tempModule);
        }
        product.resolveDependenciesState.pop_front();
    }
    return true;
}

void ProjectTreeBuilder::Private::setSearchPathsForProduct(ProductContext &product)
{
    QBS_CHECK(product.searchPaths.isEmpty());

    product.searchPaths = readExtraSearchPaths(product.item);
    Settings settings(parameters.settingsDirectory());
    const QStringList prefsSearchPaths = Preferences(&settings, product.profileModuleProperties)
                                             .searchPaths();
    const QStringList &currentSearchPaths = reader.allSearchPaths();
    for (const QString &p : prefsSearchPaths) {
        if (!currentSearchPaths.contains(p) && FileInfo(p).exists())
            product.searchPaths << p;
    }
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
    handleProject(projectContext.result, projectContext.topLevelProject, loadedItem,
                  Set<QString>(referencedFilePaths) << subProjectFilePath);
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
    logger.qbsLog(LoggerInfo, true) << "    "
                                    << Tr::tr("Setting up product dependencies took %1.")
                                           .arg(elapsedTimeString(timingData.productDependencies));
    moduleLoader.printProfilingInfo(6);
    moduleInstantiator.printProfilingInfo(6);
    propertyMerger.printProfilingInfo(6);
    groupsHandler.printProfilingInfo(4);
    probesResolver.printProfilingInfo(4);
    logger.qbsLog(LoggerInfo, true) << "  "
                                    << Tr::tr("Property checking took %1.")
                                           .arg(elapsedTimeString(timingData.propertyChecking));
}

void ProjectTreeBuilder::Private::checkProjectNamesInOverrides(const TopLevelProjectContext &tlp)
{
    for (const QString &projectNameInOverride : projectNamesUsedInOverrides) {
        if (disabledProjects.contains(projectNameInOverride))
            continue;
        if (!any_of(tlp.projects, [&projectNameInOverride](const ProjectContext *p) {
                return p->name == projectNameInOverride; })) {
            handlePropertyError(Tr::tr("Unknown project '%1' in property override.")
                                    .arg(projectNameInOverride), parameters, logger);
        }
    }
}

void ProjectTreeBuilder::Private::checkProductNamesInOverrides()
{
    for (const QString &productNameInOverride : productNamesUsedInOverrides) {
        if (erroneousProducts.contains(productNameInOverride))
            continue;
        if (!any_of(productsByName, [&productNameInOverride](
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

void ProjectTreeBuilder::Private::collectProductsByName(
    const TopLevelProjectContext &topLevelProject)
{
    for (ProjectContext * const project : topLevelProject.projects) {
        for (ProductContext &product : project->products)
            productsByName.insert({product.name, &product});
    }
}

void ProjectTreeBuilder::Private::adjustDependsItemForMultiplexing(const ProductContext &product,
                                                                    Item *dependsItem)
{
    const QString name = evaluator.stringValue(dependsItem, StringConstants::nameProperty());
    const bool productIsMultiplexed = !product.multiplexConfigurationId.isEmpty();
    if (name == product.name) {
        QBS_CHECK(!productIsMultiplexed); // This product must be an aggregator.
        return;
    }
    const auto productRange = productsByName.equal_range(name);
    if (productRange.first == productRange.second)
        return; // Dependency is a module. Nothing to adjust.

    bool profilesPropertyIsSet;
    const QStringList profiles = evaluator.stringListValue(
        dependsItem, StringConstants::profilesProperty(), &profilesPropertyIsSet);

    std::vector<const ProductContext *> multiplexedDependencies;
    bool hasNonMultiplexedDependency = false;
    for (auto it = productRange.first; it != productRange.second; ++it) {
        if (!it->second->multiplexConfigurationId.isEmpty())
            multiplexedDependencies.push_back(it->second);
        else
            hasNonMultiplexedDependency = true;
    }
    bool hasMultiplexedDependencies = !multiplexedDependencies.empty();

    static const auto multiplexConfigurationIntersects = [](const QVariantMap &lhs,
                                                            const QVariantMap &rhs) {
        QBS_CHECK(!lhs.isEmpty() && !rhs.isEmpty());
        for (auto lhsProperty = lhs.constBegin(); lhsProperty != lhs.constEnd(); lhsProperty++) {
            const auto rhsProperty = rhs.find(lhsProperty.key());
            const bool isCommonProperty = rhsProperty != rhs.constEnd();
            if (isCommonProperty && lhsProperty.value() != rhsProperty.value())
                return false;
        }
        return true;
    };

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
    const auto productMultiplexConfig
        = multiplexer.multiplexIdToVariantMap(product.multiplexConfigurationId);

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

            }
            // Otherwise collect partial matches and decide later
            const auto dependencyMultiplexConfig
                = multiplexer.multiplexIdToVariantMap(dependency->multiplexConfigurationId);

            if (multiplexConfigurationIntersects(dependencyMultiplexConfig, productMultiplexConfig))
                multiplexIds << dependency->multiplexConfigurationId;
        } else {
            // (2b), (3b) or (3c)
            const bool profileMatch = !profilesPropertyIsSet || profiles.empty()
                                      || profiles.contains(dependency->profileName);
            if (profileMatch)
                multiplexIds << depMultiplexId;
        }
    }
    if (multiplexIds.empty()) {
        const QString productName = ProductItemMultiplexer::fullProductDisplayName(
            product.name, product.multiplexConfigurationId);
        throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' not fulfilled. "
                               "There are no eligible multiplex candidates.").arg(productName,
                                 name),
                        dependsItem->location());
    }

    // In case of (2a), at most 1 match is allowed
    if (productIsMultiplexed && !profilesPropertyIsSet && multiplexIds.size() > 1) {
        const QString productName = ProductItemMultiplexer::fullProductDisplayName(
            product.name, product.multiplexConfigurationId);
        QStringList candidateNames;
        for (const auto &id : qAsConst(multiplexIds))
            candidateNames << ProductItemMultiplexer::fullProductDisplayName(name, id);
        throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' is ambiguous. "
                               "Eligible multiplex candidates: %3.").arg(
                                productName, name, candidateNames.join(QLatin1String(", "))),
                        dependsItem->location());
    }

    dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                             VariantValue::create(multiplexIds));
}

ShadowProductInfo ProjectTreeBuilder::Private::getShadowProductInfo(
    const ProductContext &product) const
{
    const bool isShadowProduct = product.name.startsWith(StringConstants::shadowProductPrefix());
    return std::make_pair(isShadowProduct, isShadowProduct
                          ? product.name.mid(StringConstants::shadowProductPrefix().size())
                                                           : QString());
}

void ProjectTreeBuilder::Private::handleProductError(const ErrorInfo &error,
                                                     ProductContext &productContext)
{
    const bool alreadyHadError = productContext.info.delayedError.hasError();
    if (!alreadyHadError) {
        productContext.info.delayedError.append(Tr::tr("Error while handling product '%1':")
                                                    .arg(productContext.name),
                                                productContext.item->location());
    }
    if (error.isInternalError()) {
        if (alreadyHadError) {
            qCDebug(lcModuleLoader()) << "ignoring subsequent internal error" << error.toString()
                                      << "in product" << productContext.name;
            return;
        }
    }
    const auto errorItems = error.items();
    for (const ErrorItem &ei : errorItems)
        productContext.info.delayedError.append(ei.description(), ei.codeLocation());
    productContext.project->result->productInfos[productContext.item] = productContext.info;
    disabledItems << productContext.item;
    erroneousProducts.insert(productContext.name);
}

void ProjectTreeBuilder::Private::handleModuleSetupError(
    ProductContext &product, const Item::Module &module, const ErrorInfo &error)
{
    if (module.required) {
        handleProductError(error, product);
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
    if (evaluator.boolValue(item, StringConstants::conditionProperty()))
        return true;
    disabledItems += item;
    return false;
}

QStringList ProjectTreeBuilder::Private::readExtraSearchPaths(Item *item, bool *wasSet)
{
    QStringList result;
    const QStringList paths = evaluator.stringListValue(
        item, StringConstants::qbsSearchPathsProperty(), wasSet);
    const JSSourceValueConstPtr prop = item->sourceProperty(
        StringConstants::qbsSearchPathsProperty());

    // Value can come from within a project file or as an overridden value from the user
    // (e.g command line).
    const QString basePath = FileInfo::path(prop ? prop->file()->filePath()
                                                 : parameters.projectFilePath());
    for (const QString &path : paths)
        result += FileInfo::resolvePath(basePath, path);
    return result;
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

static void mergeParameters(QVariantMap &dst, const QVariantMap &src)
{
    for (auto it = src.begin(); it != src.end(); ++it) {
        if (it.value().userType() == QMetaType::QVariantMap) {
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
        if (value->type() == Value::ItemValueType)
            adjustParametersScopes(std::static_pointer_cast<ItemValue>(value)->item(), scope);
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

static void checkForModuleNamePrefixCollision(
    const Item *product, const ProductContext::ResolvedAndMultiplexedDependsItem &dependency)
{
    if (!product)
        return;

    for (const Item::Module &m : product->modules()) {
        if (m.name.length() == dependency.name.length()
            || m.name.front() != dependency.name.front()) {
            continue;
        }
        QualifiedId shortName;
        QualifiedId longName;
        if (m.name < dependency.name) {
            shortName = m.name;
            longName = dependency.name;
        } else {
            shortName = dependency.name;
            longName = m.name;
        }
        throw ErrorInfo(Tr::tr("The name of module '%1' is equal to the first component of the "
                               "name of module '%2', which is not allowed")
                            .arg(shortName.toString(), longName.toString()), dependency.location());
    }
}

// Note: This function is never called for regular loading of the base module into a product,
//       but only for the special cases of loading the dummy base module into a project
//       and temporarily providing a base module for product multiplexing.
Item *ProjectTreeBuilder::Private::loadBaseModule(ProductContext &product, Item *item)
{
    const QualifiedId baseModuleName(StringConstants::qbsModule());
    const auto baseDependency
        = ProductContext::ResolvedAndMultiplexedDependsItem::makeBaseDependency();
    Item * const moduleItem = loadModule(product, item, baseDependency, Deferral::NotAllowed)
                                 .moduleItem;
    if (Q_UNLIKELY(!moduleItem))
        throw ErrorInfo(Tr::tr("Cannot load base qbs module."));
    return moduleItem;
}

ProjectTreeBuilder::Private::LoadModuleResult
ProjectTreeBuilder::Private::loadModule(ProductContext &product, Item *loadingItem,
    const ProductContext::ResolvedAndMultiplexedDependsItem &dependency,
    Deferral deferral)
{
    qCDebug(lcModuleLoader) << "loadModule name:" << dependency.name.toString()
                            << "id:" << dependency.id();

    QBS_CHECK(loadingItem);

    const auto findExistingModule = [&dependency](Item *item) -> std::pair<Item::Module *, Item *> {
        if (!item) // Happens if and only if called via loadBaseModule().
            return {};
        Item *moduleWithSameName = nullptr;
        for (Item::Module &m : item->modules()) {
            if (m.name != dependency.name)
                continue;
            if (!m.productInfo) {
                QBS_CHECK(!dependency.product);
                return {&m, m.item};
            }
            if ((dependency.profile.isEmpty() || (m.productInfo->profile == dependency.profile))
                && m.productInfo->multiplexId == dependency.multiplexId) {
                return {&m, m.item};
            }
            moduleWithSameName = m.item;
        }
        return {nullptr, moduleWithSameName};
    };
    ProductContext *productDep = nullptr;
    Item *moduleItem = nullptr;
    static const auto displayName = [](const ProductContext &p) {
        return ProductItemMultiplexer::fullProductDisplayName(p.name, p.multiplexConfigurationId);
    };
    const auto &[existingModule, moduleWithSameName] = findExistingModule(product.item);
    if (existingModule) {
        // Merge version range and required property. These will be checked again
        // after probes resolving.
        if (existingModule->name.toString() != StringConstants::qbsModule()) {
            moduleLoader.forwardParameterDeclarations(dependency.item, product.item->modules());

            // TODO: Use priorities like for property values. See QBS-1300.
            mergeParameters(existingModule->parameters, dependency.parameters);

            existingModule->versionRange.narrowDown(dependency.versionRange);
            existingModule->required |= dependency.requiredGlobally;
            if (int(product.resolveDependenciesState.size())
                > existingModule->maxDependsChainLength) {
                existingModule->maxDependsChainLength = product.resolveDependenciesState.size();
            }
        }
        QBS_CHECK(existingModule->item);
        moduleItem = existingModule->item;
        if (!contains(existingModule->loadingItems, loadingItem))
            existingModule->loadingItems.push_back(loadingItem);
    } else if (dependency.product) {
        // We have already done the look-up.
        productDep = dependency.product;
    } else {
        // First check if there's a matching product.
        const auto candidates = productsByName.equal_range(dependency.name.toString());
        for (auto it = candidates.first; it != candidates.second; ++it) {
            const auto candidate = it->second;
            if (candidate->multiplexConfigurationId != dependency.multiplexId)
                continue;
            if (!dependency.profile.isEmpty() && dependency.profile != candidate->profileName)
                continue;
            if (dependency.limitToSubProject && !haveSameSubProject(product, *candidate))
                continue;
            productDep = candidate;
            break;
        }
        if (!productDep) {
            // If we can tell that this is supposed to be a product dependency, we can skip
            // the module look-up.
            if (!dependency.profile.isEmpty() || !dependency.multiplexId.isEmpty()) {
                if (dependency.requiredGlobally) {
                    if (!dependency.profile.isEmpty()) {
                        throw ErrorInfo(Tr::tr("Product '%1' depends on '%2', which does not exist "
                                               "for the requested profile '%3'.")
                                            .arg(displayName(product), dependency.displayName(),
                                                 dependency.profile),
                                        product.item->location());
                    }
                    throw ErrorInfo(Tr::tr("Product '%1' depends on '%2', which does not exist.")
                                        .arg(displayName(product), dependency.displayName()),
                                    product.item->location());
                }
            } else {
                // No matching product found, look for a "real" module.
                const ModuleLoader::ProductContext loaderContext{
                    product.item, product.project->item, product.name, product.uniqueName(),
                    product.profileName, product.multiplexConfigurationId, product.moduleProperties,
                    product.profileModuleProperties};
                const ModuleLoader::Result loaderResult = moduleLoader.searchAndLoadModuleFile(
                    loaderContext, dependency.location(), dependency.name, dependency.fallbackMode,
                    dependency.requiredGlobally);
                moduleItem = loaderResult.moduleItem;
                product.info.probes << loaderResult.providerProbes;

                if (moduleItem) {
                    Item * const proto = moduleItem;
                    moduleItem = moduleItem->clone();
                    moduleItem->setPrototype(proto); // For parameter declarations.
                } else if (dependency.requiredGlobally) {
                    throw ErrorInfo(Tr::tr("Dependency '%1' not found for product '%2'.")
                                        .arg(dependency.name.toString(), displayName(product)),
                                    dependency.location());
                }
            }
        }
    }

    if (productDep && dependency.checkProduct) {
        if (productDep == &product) {
            throw ErrorInfo(Tr::tr("Dependency '%1' refers to itself.")
                                .arg(dependency.name.toString()),
                            dependency.location());
        }

        if (any_of(product.project->topLevelProject->productsToHandle, [productDep](const auto &e) {
                return e.first == productDep;
            })) {
            if (deferral == Deferral::Allowed)
                return {nullptr, productDep, HandleDependency::Defer};
            ErrorInfo e;
            e.append(Tr::tr("Cyclic dependencies detected:"));
            e.append(Tr::tr("First product is '%1'.")
                         .arg(displayName(product)), product.item->location());
            e.append(Tr::tr("Second product is '%1'.")
                         .arg(displayName(*productDep)), productDep->item->location());
            e.append(Tr::tr("Requested here."), dependency.location());
            throw e;
        }

        // This covers both the case of user-disabled products and products with errors.
        // The latter are force-disabled in handleProductError().
        if (disabledItems.contains(productDep->item)) {
            if (dependency.requiredGlobally) {
                ErrorInfo e;
                e.append(Tr::tr("Product '%1' depends on '%2',")
                             .arg(displayName(product), displayName(*productDep)),
                         product.item->location());
                e.append(Tr::tr("but product '%1' is disabled.").arg(displayName(*productDep)),
                         productDep->item->location());
                throw e;
            }
            productDep = nullptr;
        }
    }

    if (productDep) {
        QBS_CHECK(productDep->mergedExportItem);
        moduleItem = productDep->mergedExportItem->clone();
        moduleItem->setParent(nullptr);

        // Needed for isolated Export item evaluation.
        moduleItem->setPrototype(productDep->mergedExportItem);
    }

    if (moduleItem) {
        for (auto it = product.resolveDependenciesState.begin();
             it != product.resolveDependenciesState.end(); ++it) {
            Item *itemToCheck = moduleItem;
            if (it->loadingItem != itemToCheck) {
                if (!productDep)
                    continue;
                itemToCheck = productDep->item;
            }
            if (it->loadingItem != itemToCheck)
                continue;
            ErrorInfo e;
            e.append(Tr::tr("Cyclic dependencies detected:"));
            while (true) {
                e.append(it->loadingItemOrigin.name.toString(),
                         it->loadingItemOrigin.location());
                if (it->loadingItem->type() == ItemType::ModuleInstance) {
                    createNonPresentModule(itemPool, it->loadingItemOrigin.name.toString(),
                                           QLatin1String("cyclic dependency"), it->loadingItem);
                }
                if (it == product.resolveDependenciesState.begin())
                    break;
                --it;
            }
            e.append(dependency.name.toString(), dependency.location());
            throw e;
        }
        checkForModuleNamePrefixCollision(product.item, dependency);
    }

    // Can only happen with multiplexing.
    if (moduleWithSameName && moduleWithSameName != moduleItem)
        QBS_CHECK(productDep);

    QString loadingName;
    if (loadingItem == product.item) {
        loadingName = product.name;
    } else if (!product.resolveDependenciesState.empty()) {
        const auto &loadingItemOrigin = product.resolveDependenciesState.front().loadingItemOrigin;
        loadingName = loadingItemOrigin.name.toString() + loadingItemOrigin.multiplexId
                      + loadingItemOrigin.profile;
    }
    moduleInstantiator.instantiate({
        product.item, product.name, loadingItem, loadingName, moduleItem, moduleWithSameName,
        productDep ? productDep->item : nullptr, product.scope, product.project->scope,
        dependency.name, dependency.id(), bool(existingModule)});

    // At this point, a null module item is only possible for a non-required dependency.
    // Note that we still needed to to the instantiation above, as that injects the module
    // name into the surrounding item for the ".present" check.
    if (!moduleItem) {
        QBS_CHECK(!dependency.requiredGlobally);
        return {nullptr, nullptr, HandleDependency::Ignore};
    }

    const auto createModule = [&] {
        Item::Module m;
        m.item = moduleItem;
        if (productDep) {
            m.productInfo.emplace(productDep->item, productDep->multiplexConfigurationId,
                                  productDep->profileName);
        }
        m.name = dependency.name;
        m.required = dependency.requiredLocally;
        m.versionRange = dependency.versionRange;
        return m;
    };
    const auto addLocalModule = [&] {
        if (loadingItem->type() == ItemType::ModuleInstance
            && !findExistingModule(loadingItem).first) {
            loadingItem->addModule(createModule());
        }
    };

    // The module has already been loaded, so we don't need to add it to the product's list of
    // modules, nor do we need to handle its dependencies. The only thing we might need to
    // do is to add it to the "local" list of modules of the loading item, if it isn't in there yet.
    if (existingModule) {
       addLocalModule();
       return {nullptr, nullptr, HandleDependency::Ignore};
    }

    qCDebug(lcModuleLoader) << "module loaded:" << dependency.name.toString();
    if (product.item) {
        Item::Module module = createModule();

        if (module.name.toString() != StringConstants::qbsModule()) {
            // TODO: Why do we have default parameters only for Export items and
            //       property declarations only for modules? Does that make any sense?
            if (productDep)
                module.parameters = productDep->defaultParameters;
            mergeParameters(module.parameters, dependency.parameters);
        }
        module.required = dependency.requiredGlobally;
        module.loadingItems.push_back(loadingItem);
        module.maxDependsChainLength = product.resolveDependenciesState.size();
        product.item->addModule(module);
        addLocalModule();
    }
    return {moduleItem, nullptr, HandleDependency::Use};
}

bool ProjectTreeBuilder::Private::haveSameSubProject(const ProductContext &p1,
                                                     const ProductContext &p2)
{
    for (const Item *otherParent = p2.item->parent(); otherParent;
         otherParent = otherParent->parent()) {
        if (otherParent == p1.item->parent())
            return true;
    }
    return false;
}

static Item::PropertyMap filterItemProperties(const Item::PropertyMap &properties)
{
    Item::PropertyMap result;
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (it.value()->type() == Value::ItemValueType)
            result.insert(it.key(), it.value());
    }
    return result;
}

static QVariantMap safeToVariant(JSContext *ctx, const JSValue &v)
{
    QVariantMap result;
    handleJsProperties(ctx, v, [&](const JSAtom &prop, const JSPropertyDescriptor &desc) {
        const JSValue u = desc.value;
        if (JS_IsError(ctx, u))
            throw ErrorInfo(getJsString(ctx, u));
        const QString name = getJsString(ctx, prop);
        result[name] = (JS_IsObject(u) && !JS_IsArray(ctx, u) && !JS_IsRegExp(ctx, u))
                           ? safeToVariant(ctx, u) : getJsVariant(ctx, u);
    });
    return result;
}

QVariantMap ProjectTreeBuilder::Private::extractParameters(Item *dependsItem) const
{
    QVariantMap result;
    const Item::PropertyMap &itemProperties = filterItemProperties(dependsItem->properties());
    if (itemProperties.empty())
        return result;
    auto origProperties = dependsItem->properties();

    // TODO: This is not exception-safe. Also, can't we do the item value check along the
    //       way, without allocationg an extra map and exchanging the list of children?
    dependsItem->setProperties(itemProperties);

    JSValue sv = evaluator.scriptValue(dependsItem);
    try {
        result = safeToVariant(evaluator.engine()->context(), sv);
    } catch (const ErrorInfo &exception) {
        auto ei = exception;
        ei.prepend(Tr::tr("Error in dependency parameter."), dependsItem->location());
        throw ei;
    }
    dependsItem->setProperties(origProperties);
    return result;
}

std::optional<ProductContext::ResolvedDependsItem>
ProjectTreeBuilder::Private::resolveDependsItem(const ProductContext &product, Item *dependsItem)
{
    if (!checkItemCondition(dependsItem)) {
        qCDebug(lcModuleLoader) << "Depends item disabled, ignoring.";
        return {};
    }

    const QString name = evaluator.stringValue(dependsItem, StringConstants::nameProperty());
    if (name == StringConstants::qbsModule()) // Redundant
        return {};

    bool submodulesPropertySet;
    const QStringList submodules = evaluator.stringListValue(
                dependsItem, StringConstants::submodulesProperty(), &submodulesPropertySet);
    if (submodules.empty() && submodulesPropertySet) {
        qCDebug(lcModuleLoader) << "Ignoring Depends item with empty submodules list.";
        return {};
    }
    if (Q_UNLIKELY(submodules.size() > 1 && !dependsItem->id().isEmpty())) {
        QString msg = Tr::tr("A Depends item with more than one module cannot have an id.");
        throw ErrorInfo(msg, dependsItem->location());
    }
    bool productTypesWasSet;
    const QStringList productTypes = evaluator.stringListValue(
                dependsItem, StringConstants::productTypesProperty(), &productTypesWasSet);
    if (!name.isEmpty() && !productTypes.isEmpty()) {
        throw ErrorInfo(Tr::tr("The name and productTypes are mutually exclusive "
                               "in a Depends item"), dependsItem->location());
    }
    if (productTypes.isEmpty() && productTypesWasSet) {
        qCDebug(lcModuleLoader) << "Ignoring Depends item with empty productTypes list.";
        return {};
    }
    if (name.isEmpty() && !productTypesWasSet) {
        throw ErrorInfo(Tr::tr("Either name or productTypes must be set in a Depends item"),
                        dependsItem->location());
    }

    const FallbackMode fallbackMode = parameters.fallbackProviderEnabled()
            && evaluator.boolValue(dependsItem, StringConstants::enableFallbackProperty())
            ? FallbackMode::Enabled : FallbackMode::Disabled;

    bool profilesPropertyWasSet = false;
    std::optional<QStringList> profiles;
    bool required = true;
    if (productTypes.isEmpty()) {
        const QStringList profileList = evaluator.stringListValue(
                    dependsItem, StringConstants::profilesProperty(), &profilesPropertyWasSet);
        if (profilesPropertyWasSet)
            profiles.emplace(profileList);
        required = evaluator.boolValue(dependsItem, StringConstants::requiredProperty());
    }
    const Version minVersion = Version::fromString(
                evaluator.stringValue(dependsItem, StringConstants::versionAtLeastProperty()));
    const Version maxVersion = Version::fromString(
                evaluator.stringValue(dependsItem, StringConstants::versionBelowProperty()));
    const bool limitToSubProject = evaluator.boolValue(
                dependsItem, StringConstants::limitToSubProjectProperty());
    const QStringList multiplexIds = evaluator.stringListValue(
                dependsItem, StringConstants::multiplexConfigurationIdsProperty());
    adjustParametersScopes(dependsItem, dependsItem);
    moduleLoader.forwardParameterDeclarations(dependsItem, product.item->modules());
    const QVariantMap parameters = extractParameters(dependsItem);

    return ProductContext::ResolvedDependsItem{dependsItem, QualifiedId::fromString(name),
                submodules, FileTags::fromStringList(productTypes), multiplexIds, profiles,
                {minVersion, maxVersion}, parameters, limitToSubProject, fallbackMode, required};
}

// Potentially multiplexes a dependency along Depends.productTypes, Depends.subModules and
// Depends.profiles, as well as internally set up multiplexing axes.
// Each entry in the resulting queue corresponds to exactly one product or module to pull in.
std::queue<ProductContext::ResolvedAndMultiplexedDependsItem>
ProjectTreeBuilder::Private::multiplexDependency(
        const ProductContext &product, const ProductContext::ResolvedDependsItem &dependency)
{
    std::queue<ProductContext::ResolvedAndMultiplexedDependsItem> dependencies;
    if (!dependency.productTypes.empty()) {
        std::vector<ProductContext *> matchingProducts;
        for (const FileTag &typeTag : dependency.productTypes) {
            const auto range = productsByType.equal_range(typeTag);
            for (auto it = range.first; it != range.second; ++it) {
                if (it->second != &product
                    && it->second->name != product.name
                    && (!dependency.limitToSubProject
                        || haveSameSubProject(product, *it->second))) {
                    matchingProducts.push_back(it->second);
                }
            }
        }
        if (matchingProducts.empty()) {
            qCDebug(lcModuleLoader) << "Depends.productTypes does not match anything."
                                    << dependency.item->location();
            return {};
        }
        for (ProductContext * const match : matchingProducts)
            dependencies.emplace(match, dependency);
        return dependencies;
    }

    const QStringList profiles = dependency.profiles && !dependency.profiles->isEmpty()
            ? *dependency.profiles : QStringList(QString());
    const QStringList multiplexIds = !dependency.multiplexIds.isEmpty()
            ? dependency.multiplexIds : QStringList(QString());
    const QStringList subModules = !dependency.subModules.isEmpty()
            ? dependency.subModules : QStringList(QString());
    for (const QString &profile : profiles) {
        for (const QString &multiplexId : multiplexIds) {
            for (const QString &subModule : subModules) {
                QualifiedId name = dependency.name;
                if (!subModule.isEmpty())
                    name << subModule.split(QLatin1Char('.'));
                dependencies.emplace(dependency, name, profile, multiplexId);
            }
        }
    }
    return dependencies;
}

QString ProductContext::uniqueName() const
{
    return ResolvedProduct::uniqueName(name, multiplexConfigurationId);
}

QString ProductContext::ResolvedAndMultiplexedDependsItem::id() const
{
    if (!item) {
        QBS_CHECK(name.toString() == StringConstants::qbsModule());
        return {};
    }
    return item->id();
}

CodeLocation ProductContext::ResolvedAndMultiplexedDependsItem::location() const
{
    if (!item) {
        QBS_CHECK(name.toString() == StringConstants::qbsModule());
        return {};
    }
    return item->location();
}

QString ProductContext::ResolvedAndMultiplexedDependsItem::displayName() const
{
    return ProductItemMultiplexer::fullProductDisplayName(name.toString(), multiplexId);
}

ProjectTreeBuilder::Private::TempBaseModuleAttacher::TempBaseModuleAttacher(
    ProjectTreeBuilder::Private *d, ProductContext &product)
    : m_productItem(product.item)
{
    const ValuePtr qbsValue = m_productItem->property(StringConstants::qbsModule());

    // Cloning is necessary because the original value will get "instantiated" now.
    if (qbsValue)
        m_origQbsValue = qbsValue->clone();

    m_tempBaseModule = d->loadBaseModule(product, m_productItem);
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
