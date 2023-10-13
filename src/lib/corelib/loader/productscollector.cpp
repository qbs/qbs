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

#include "productscollector.h"

#include "dependenciesresolver.h"
#include "itemreader.h"
#include "loaderutils.h"
#include "localprofiles.h"
#include "productitemmultiplexer.h"
#include "probesresolver.h"

#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/item.h>
#include <language/language.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/profile.h>
#include <tools/profiling.h>
#include <language/scriptengine.h>
#include <tools/set.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <QDir>
#include <QDirIterator>

namespace qbs::Internal {

class ProductsCollector::Private
{
public:
    Private(LoaderState &loaderState) : loaderState(loaderState) {}

    void handleProject(Item *projectItem, ProjectContext *parentProject,
                       const Set<QString> &referencedFilePaths);
    QList<Item *> multiplexProductItem(ProductContext &dummyContext, Item *productItem);
    void prepareProduct(ProjectContext &projectContext, Item *productItem,
                        ProductContext *mainProduct = nullptr);
    void handleSubProject(ProjectContext &projectContext, Item *projectItem,
                          const Set<QString> &referencedFilePaths);
    void copyProperties(const Item *sourceProject, Item *targetProject);
    QList<Item *> loadReferencedFile(
            const QString &relativePath, const CodeLocation &referencingLocation,
            const Set<QString> &referencedFilePaths, ProductContext &dummyContext);
    bool mergeExportItems(ProductContext &productContext);
    bool checkExportItemCondition(Item *exportItem, const ProductContext &product);
    void initProductProperties(const ProductContext &product);
    void checkProjectNamesInOverrides();
    void collectProductsByNameAndItem();
    void checkProductNamesInOverrides();
    void mergeProperty(Item *dst, const QString &name, const ValuePtr &value);

    LoaderState &loaderState;
    Settings settings{loaderState.parameters().settingsDirectory()};
    Set<QString> disabledProjects;
    Version qbsVersion;
    Item *tempScopeItem = nullptr;

private:
    // TODO: Put this in loaderutils
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

ProductsCollector::ProductsCollector(LoaderState &loaderState)
    : d(makePimpl<Private>(loaderState)) {}

ProductsCollector::~ProductsCollector() = default;

void ProductsCollector::run(Item *rootProject)
{
    d->handleProject(rootProject, nullptr, {rootProject->file()->filePath()});
    d->checkProjectNamesInOverrides();
    d->collectProductsByNameAndItem();
    d->checkProductNamesInOverrides();
    d->loaderState.topLevelProject().checkForLocalProfileAsTopLevelProfile(
                d->loaderState.parameters().topLevelProfile());
}

void ProductsCollector::Private::handleProject(Item *projectItem, ProjectContext *parentProject,
                                               const Set<QString> &referencedFilePaths)
{
    const SetupProjectParameters &parameters = loaderState.parameters();
    Evaluator &evaluator = loaderState.evaluator();
    Logger &logger = loaderState.logger();
    ItemReader &itemReader = loaderState.itemReader();
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();

    auto p = std::make_unique<ProjectContext>();
    auto &projectContext = *p;
    projectContext.item = projectItem;
    projectContext.parent = parentProject;
    projectContext.topLevelProject = &topLevelProject;
    ItemValuePtr itemValue = ItemValue::create(projectItem);
    projectContext.scope = Item::create(&loaderState.itemPool(), ItemType::Scope);
    projectContext.scope->setFile(projectItem->file());
    projectContext.scope->setProperty(StringConstants::projectVar(), itemValue);
    ProductContext dummyProduct;
    dummyProduct.project = &projectContext;
    dummyProduct.moduleProperties = parameters.finalBuildConfigurationTree();
    dummyProduct.profileModuleProperties = dummyProduct.moduleProperties;
    dummyProduct.profileName = parameters.topLevelProfile();
    loadBaseModule(dummyProduct, projectItem, loaderState);

    projectItem->overrideProperties(parameters.overriddenValuesTree(),
                                    StringConstants::projectPrefix(), parameters, logger);
    projectContext.name = evaluator.stringValue(projectItem, StringConstants::nameProperty());
    if (projectContext.name.isEmpty()) {
        projectContext.name = FileInfo::baseName(projectItem->location().filePath());
        projectItem->setProperty(StringConstants::nameProperty(),
                                 VariantValue::create(projectContext.name));
    }
    if (parentProject)
        parentProject->children.push_back(p.get());
    topLevelProject.addProject(p.release());
    projectItem->overrideProperties(parameters.overriddenValuesTree(),
                                    StringConstants::projectsOverridePrefix() + projectContext.name,
                                    parameters, logger);
    if (!topLevelProject.checkItemCondition(projectItem, evaluator)) {
        disabledProjects.insert(projectContext.name);
        return;
    }
    SearchPathsManager searchPathsManager(itemReader, itemReader.readExtraSearchPaths(projectItem)
                                          << projectItem->file()->dirPath());
    projectContext.searchPathsStack = itemReader.extraSearchPathsStack();

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

    ProbesResolver(loaderState).resolveProbes(dummyProduct, projectItem);

    collectProfilesFromItems(projectItem, projectContext.scope, loaderState);

    QList<Item *> multiplexedProducts;
    for (Item * const child : projectItem->children()) {
        if (child->type() == ItemType::Product)
            multiplexedProducts << multiplexProductItem(dummyProduct, child);
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
            handleProject(child, &projectContext, referencedFilePaths);
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
                filePath, referencingLocation, referencedFilePaths, dummyProduct);
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
            handleProject(subItem, &projectContext,
                          Set<QString>(referencedFilePaths) << subItem->file()->filePath());
            break;
        default:
            break;
        }
    }
}

QList<Item *> ProductsCollector::Private::multiplexProductItem(ProductContext &dummyContext,
                                                               Item *productItem)
{
    // Overriding the product item properties must be done here already, because multiplexing
    // properties might depend on product properties.
    const QString &nameKey = StringConstants::nameProperty();
    QString productName = loaderState.evaluator().stringValue(productItem, nameKey);
    if (productName.isEmpty()) {
        productName = FileInfo::completeBaseName(productItem->file()->filePath());
        productItem->setProperty(nameKey, VariantValue::create(productName));
    }
    productItem->overrideProperties(loaderState.parameters().overriddenValuesTree(),
                                    StringConstants::productsOverridePrefix() + productName,
                                    loaderState.parameters(), loaderState.logger());
    dummyContext.item = productItem;
    TempBaseModuleAttacher tbma(this, dummyContext);
    return multiplex(productName, productItem, tbma.tempBaseModuleItem(),
        [&] { tbma.drop(); }, loaderState);
}

void ProductsCollector::Private::prepareProduct(ProjectContext &projectContext, Item *productItem,
                                                ProductContext *mainProduct)
{
    const SetupProjectParameters &parameters = loaderState.parameters();
    Evaluator &evaluator = loaderState.evaluator();
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();

    AccumulatingTimer timer(parameters.logElapsedTime()
                            ? &topLevelProject.timingData().preparingProducts : nullptr);
    topLevelProject.checkCancelation();
    qCDebug(lcModuleLoader) << "prepareProduct" << productItem->file()->filePath();

    if (mainProduct)
        mainProduct->shadowProduct = std::make_unique<ProductContext>();
    else
        projectContext.products.emplace_back();
    ProductContext &productContext = mainProduct
            ? *mainProduct->shadowProduct : projectContext.products.back();
    productContext.item = productItem;
    productContext.project = &projectContext;

    // Retrieve name, profile and multiplex id.
    productContext.name = evaluator.stringValue(productItem, StringConstants::nameProperty());
    QBS_CHECK(!productContext.name.isEmpty());
    const ItemValueConstPtr qbsItemValue = productItem->itemProperty(StringConstants::qbsModule(),
                                                                     loaderState.itemPool());
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
    std::optional<QVariantMap> flatConfig
            = topLevelProject.profileConfig(productContext.profileName);
    if (!flatConfig) {
        const Profile profile(productContext.profileName, &settings,
                              loaderState.topLevelProject().localProfiles());
        if (!profile.exists()) {
            ErrorInfo error(Tr::tr("Profile '%1' does not exist.").arg(profile.name()),
                            productItem->location());
            productContext.handleError(error);
            return;
        }
        flatConfig = SetupProjectParameters::expandedBuildConfiguration(
            profile, parameters.configurationName());
        topLevelProject.addProfileConfig(productContext.profileName, *flatConfig);
    }
    productContext.profileModuleProperties = SetupProjectParameters::finalBuildConfigurationTree(
        *flatConfig, {});
    productContext.moduleProperties = SetupProjectParameters::finalBuildConfigurationTree(
        *flatConfig, parameters.overriddenValues());
    initProductProperties(productContext);

    // Set up product scope. This is mainly for using the "product" and "project"
    // variables in some contexts.
    ItemValuePtr itemValue = ItemValue::create(productItem);
    productContext.scope = Item::create(&loaderState.itemPool(), ItemType::Scope);
    productContext.scope->setProperty(StringConstants::productVar(), itemValue);
    productContext.scope->setFile(productItem->file());
    productContext.scope->setScope(productContext.project->scope);

    // If there are any child items with an id, set up a scope for them. This is mostly
    // relevant for Probe items. While we might get away with using the file's id scope
    // in the absence of multiplexing, having a proper per-product scope seems cleaner.
    QBS_CHECK(productItem->scope());
    QBS_CHECK(productItem->scope() == productContext.project->scope);
    for (Item * const child : productItem->children()) {
        if (child->id().isEmpty())
            continue;
        if (productItem->scope() == productContext.project->scope) {
            productItem->setScope(Item::create(&loaderState.itemPool(), ItemType::Scope));
            productItem->scope()->setScope(productContext.project->scope);
        }
        const ItemValuePtr childValue = ItemValue::create(child);
        productItem->scope()->setProperty(child->id(), childValue);
        productContext.scope->setProperty(child->id(), childValue);
    }

    const bool hasExportItems = mergeExportItems(productContext);

    setScopeForDescendants(productItem, productContext.scope);

    if (!hasExportItems)
        return;

    // This "shadow product" exists only to pull in a dependency on the actual product
    // and nothing else, thus providing us with the pure environment that we need to
    // evaluate the product's exported properties in isolation in the project resolver.
    Item * const importer = Item::create(&loaderState.itemPool(), ItemType::Product);
    importer->setProperty(QStringLiteral("name"),
                          VariantValue::create(StringConstants::shadowProductPrefix()
                                               + productContext.name));
    importer->setFile(productItem->file());
    importer->setLocation(productItem->location());
    importer->setScope(projectContext.scope);
    importer->setupForBuiltinType(parameters.deprecationWarningMode(), loaderState.logger());
    Item * const dependsItem = Item::create(&loaderState.itemPool(), ItemType::Depends);
    dependsItem->setProperty(QStringLiteral("name"), VariantValue::create(productContext.name));
    dependsItem->setProperty(QStringLiteral("required"), VariantValue::create(false));
    dependsItem->setFile(importer->file());
    dependsItem->setLocation(importer->location());
    dependsItem->setupForBuiltinType(parameters.deprecationWarningMode(), loaderState.logger());
    dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                             VariantValue::create(productContext.multiplexConfigurationId));
    Item::addChild(importer, dependsItem);
    prepareProduct(projectContext, importer, &productContext);
}

void ProductsCollector::Private::handleSubProject(
        ProjectContext &projectContext, Item *projectItem, const Set<QString> &referencedFilePaths)
{
    const SetupProjectParameters &parameters = loaderState.parameters();
    Evaluator &evaluator = loaderState.evaluator();
    Logger &logger = loaderState.logger();
    ItemReader &itemReader = loaderState.itemReader();
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();

    qCDebug(lcModuleLoader) << "handleSubProject" << projectItem->file()->filePath();

    Item * const propertiesItem = projectItem->child(ItemType::PropertiesInSubProject);
    if (!topLevelProject.checkItemCondition(projectItem, evaluator))
        return;
    if (propertiesItem) {
        propertiesItem->setScope(projectItem);
        if (!topLevelProject.checkItemCondition(propertiesItem, evaluator))
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
        loadedItem = itemReader.setupItemFromFile(subProjectFilePath, projectItem->location());
    } catch (const ErrorInfo &error) {
        if (parameters.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        logger.printWarning(error);
        return;
    }

    loadedItem = itemReader.wrapInProjectIfNecessary(loadedItem);
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
    handleProject(loadedItem, &projectContext, Set<QString>(referencedFilePaths) << subProjectFilePath);
}

void ProductsCollector::Private::copyProperties(const Item *sourceProject, Item *targetProject)
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

QList<Item *> ProductsCollector::Private::loadReferencedFile(
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
    Item * const subItem = loaderState.itemReader().setupItemFromFile(
                absReferencePath, referencingLocation);
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
        collectProfilesFromItems(subItem, dummyContext.project->scope, loaderState);
        loadedItems << multiplexProductItem(dummyContext, subItem);
    }
    return loadedItems;
}

void ProductsCollector::Private::mergeProperty(Item *dst, const QString &name,
                                               const ValuePtr &value)
{
    if (value->type() == Value::ItemValueType) {
        const ItemValueConstPtr itemValue = std::static_pointer_cast<ItemValue>(value);
        const Item * const valueItem = itemValue->item();
        Item * const subItem = dst->itemProperty(name, itemValue, loaderState.itemPool())->item();
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
                baseValue->clone(loaderState.itemPool()));
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

bool ProductsCollector::Private::mergeExportItems(ProductContext &productContext)
{
    const SetupProjectParameters &parameters = loaderState.parameters();
    Evaluator &evaluator = loaderState.evaluator();
    Logger &logger = loaderState.logger();
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();

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

    Item *merged = Item::create(&loaderState.itemPool(), ItemType::Export);
    const QString &nameKey = StringConstants::nameProperty();
    const ValuePtr nameValue = VariantValue::create(productContext.name);
    merged->setProperty(nameKey, nameValue);
    Set<FileContextConstPtr> filesWithExportItem;
    QVariantMap defaultParameters;
    for (Item * const exportItem : exportItems) {
        topLevelProject.checkCancelation();
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
                defaultParameters = mergeDependencyParameters(defaultParameters,
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
bool ProductsCollector::Private::checkExportItemCondition(Item *exportItem,
                                                          const ProductContext &product)
{
    class ScopeHandler {
    public:
        ScopeHandler(Item *exportItem, const ProductContext &productContext, Item **cachedScopeItem,
                     ItemPool &itemPool) : m_exportItem(exportItem), m_itemPool(itemPool)
        {
            if (!*cachedScopeItem)
                *cachedScopeItem = Item::create(&m_itemPool, ItemType::Scope);
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
        ItemPool &m_itemPool;
    } scopeHandler(exportItem, product, &tempScopeItem, loaderState.itemPool());
    return loaderState.topLevelProject().checkItemCondition(exportItem, loaderState.evaluator());
}

void ProductsCollector::Private::initProductProperties(const ProductContext &product)
{
    QString buildDir = ResolvedProduct::deriveBuildDirectoryName(product.name,
                                                                 product.multiplexConfigurationId);
    buildDir = FileInfo::resolvePath(product.project->topLevelProject->buildDirectory(), buildDir);
    product.item->setProperty(StringConstants::buildDirectoryProperty(),
                              VariantValue::create(buildDir));
    const QString sourceDir = QFileInfo(product.item->file()->filePath()).absolutePath();
    product.item->setProperty(StringConstants::sourceDirectoryProperty(),
                              VariantValue::create(sourceDir));
}

void ProductsCollector::Private::checkProjectNamesInOverrides()
{
    for (const QString &projectNameInOverride
         : loaderState.topLevelProject().projectNamesUsedInOverrides()) {
        if (disabledProjects.contains(projectNameInOverride))
            continue;
        if (!any_of(loaderState.topLevelProject().projects(),
                    [&projectNameInOverride](const ProjectContext *p) {
                return p->name == projectNameInOverride; })) {
            handlePropertyError(Tr::tr("Unknown project '%1' in property override.")
                                .arg(projectNameInOverride),
                                loaderState.parameters(), loaderState.logger());
        }
    }
}

void ProductsCollector::Private::collectProductsByNameAndItem()
{
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();
    for (ProjectContext * const project : topLevelProject.projects()) {
        for (ProductContext &product : project->products)
            topLevelProject.addProduct(product);
    }
}

void ProductsCollector::Private::checkProductNamesInOverrides()
{
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();
    for (const QString &productNameInOverride : topLevelProject.productNamesUsedInOverrides()) {
        if (!topLevelProject.productWithNameAndConstraint(
                    productNameInOverride, [&productNameInOverride](const ProductContext &product) {
                // In an override string such as "a.b.c:d, we cannot tell whether we have a product
                // "a" and a module "b.c" or a product "a.b" and a module "c", so we need to take
                // care not to emit false positives here.
                return product.name == productNameInOverride
                       || product.name.startsWith(productNameInOverride + StringConstants::dot());
            })) {
            handlePropertyError(Tr::tr("Unknown product '%1' in property override.")
                                .arg(productNameInOverride),
                                loaderState.parameters(), loaderState.logger());
        }
    }
}

ProductsCollector::Private::TempBaseModuleAttacher::TempBaseModuleAttacher(
    ProductsCollector::Private *d, ProductContext &product)
    : m_productItem(product.item)
{
    const ValuePtr qbsValue = m_productItem->property(StringConstants::qbsModule());

    // Cloning is necessary because the original value will get "instantiated" now.
    if (qbsValue)
        m_origQbsValue = qbsValue->clone(d->loaderState.itemPool());

    m_tempBaseModule = loadBaseModule(product, m_productItem, d->loaderState);
}

void ProductsCollector::Private::TempBaseModuleAttacher::drop()
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
