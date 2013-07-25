/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "moduleloader.h"

#include "builtindeclarations.h"
#include "builtinvalue.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "itemreader.h"
#include "scriptengine.h"
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QDebug>
#include <QDir>
#include <QDirIterator>

namespace qbs {
namespace Internal {

class ModuleLoader::ItemModuleList : public QList<Item::Module> {};

const QString moduleSearchSubDir = QLatin1String("modules");

ModuleLoader::ModuleLoader(ScriptEngine *engine, BuiltinDeclarations *builtins,
                           const Logger &logger)
    : m_engine(engine)
    , m_pool(0)
    , m_logger(logger)
    , m_progressObserver(0)
    , m_reader(new ItemReader(builtins, logger))
    , m_evaluator(new Evaluator(engine, logger))
{
}

ModuleLoader::~ModuleLoader()
{
    delete m_evaluator;
    delete m_reader;
}

void ModuleLoader::setProgressObserver(ProgressObserver *progressObserver)
{
    m_progressObserver = progressObserver;
}

void ModuleLoader::setSearchPaths(const QStringList &searchPaths)
{
    m_reader->setSearchPaths(searchPaths);

    m_moduleDirListCache.clear();
    m_moduleSearchPaths.clear();
    foreach (const QString &path, searchPaths)
        m_moduleSearchPaths += FileInfo::resolvePath(path, moduleSearchSubDir);
}

ModuleLoaderResult ModuleLoader::load(const QString &filePath,
        const QVariantMap &overriddenProperties, const QVariantMap &buildConfigProperties,
        bool wrapWithProjectItem)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] load" << filePath;
    m_overriddenProperties = overriddenProperties;
    m_buildConfigProperties = buildConfigProperties;

    ModuleLoaderResult result;
    m_pool = result.itemPool.data();
    m_reader->setPool(m_pool);

    Item *root = m_reader->readFile(filePath);
    if (!root)
        return ModuleLoaderResult();

    if (wrapWithProjectItem && root->typeName() != QLatin1String("Project"))
        root = wrapWithProject(root);

    handleProject(&result, root, QSet<QString>() << QDir::cleanPath(filePath));
    result.root = root;
    result.qbsFiles = m_reader->filesRead();
    return result;
}

void ModuleLoader::handleProject(ModuleLoaderResult *loadResult, Item *item,
        const QSet<QString> &referencedFilePaths)
{
    if (!checkItemCondition(item))
        return;
    ProjectContext projectContext;
    projectContext.result = loadResult;
    projectContext.extraSearchPaths = readExtraSearchPaths(item);
    projectContext.extraSearchPaths += FileInfo::resolvePath(item->file()->dirPath(),
                                                             moduleSearchSubDir);
    projectContext.item = item;
    ItemValuePtr itemValue = ItemValue::create(item);
    projectContext.scope = Item::create(m_pool);
    projectContext.scope->setProperty(QLatin1String("project"), itemValue);

    ProductContext dummyProductContext;
    dummyProductContext.project = &projectContext;
    loadBaseModule(&dummyProductContext, item);
    overrideItemProperties(item, QLatin1String("project"), m_overriddenProperties);

    foreach (Item *child, item->children()) {
        child->setScope(projectContext.scope);
        if (child->typeName() == QLatin1String("Product")) {
            handleProduct(&projectContext, child);
        } else if (child->typeName() == QLatin1String("SubProject")) {
            handleSubProject(&projectContext, child, referencedFilePaths);
        } else if (child->typeName() == QLatin1String("Project")) {
            copyProperties(item, child);
            handleProject(loadResult, child, referencedFilePaths);
        }
    }

    const QString projectFileDirPath = FileInfo::path(item->file()->filePath());
    const QStringList refs = toStringList(m_evaluator->property(item, "references"));
    foreach (const QString &filePath, refs) {
        const QString absReferencePath = FileInfo::resolvePath(projectFileDirPath, filePath);
        if (referencedFilePaths.contains(absReferencePath))
            throw ErrorInfo(Tr::tr("Cycle detected while referencing file '%1'.").arg(filePath),
                            item->property(QLatin1String("references"))->location());
        Item *subItem = m_reader->readFile(absReferencePath);
        subItem->setScope(projectContext.scope);
        subItem->setParent(projectContext.item);
        QList<Item *> projectChildren = projectContext.item->children();
        projectChildren += subItem;
        projectContext.item->setChildren(projectChildren);
        if (subItem->typeName() == "Product") {
            handleProduct(&projectContext, subItem);
        } else if (subItem->typeName() == "Project") {
            copyProperties(item, subItem);
            handleProject(loadResult, subItem,
                          QSet<QString>(referencedFilePaths) << absReferencePath);
        } else {
            throw ErrorInfo(Tr::tr("The top-level item of a file in a \"references\" list must be "
                               "a Product or a Project, but it is \"%1\".").arg(subItem->typeName()),
                        subItem->location());
        }
    }
}

void ModuleLoader::handleProduct(ProjectContext *projectContext, Item *item)
{
    checkCancelation();
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] handleProduct " << item->file()->filePath();
    ProductContext productContext;
    productContext.project = projectContext;
    productContext.extraSearchPaths = readExtraSearchPaths(item);
    productContext.item = item;
    ItemValuePtr itemValue = ItemValue::create(item);
    productContext.scope = Item::create(m_pool);
    productContext.scope->setProperty(QLatin1String("product"), itemValue);
    productContext.scope->setScope(projectContext->scope);
    DependsContext dependsContext;
    dependsContext.product = &productContext;
    dependsContext.productDependencies = &productContext.info.usedProducts;
    setScopeForDescendants(item, productContext.scope);
    resolveDependencies(&dependsContext, item);
    createAdditionalModuleInstancesInProduct(&productContext);

    foreach (Item *child, item->children()) {
        if (child->typeName() == QLatin1String("Group"))
            handleGroup(&productContext, child);
        else if (child->typeName() == QLatin1String("Artifact"))
            handleArtifact(&productContext, child);
        else if (child->typeName() == QLatin1String("Export"))
            deferExportItem(&productContext, child);
        else if (child->typeName() == QLatin1String("ProductModule"))   // ### remove in 1.2
            handleProductModule(&productContext, child);
        else if (child->typeName() == QLatin1String("Probe"))
            resolveProbe(item, child);
    }

    mergeExportItems(&productContext);
    projectContext->result->productInfos.insert(item, productContext.info);
}

void ModuleLoader::handleSubProject(ModuleLoader::ProjectContext *projectContext, Item *item,
        const QSet<QString> &referencedFilePaths)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] handleSubProject " << item->file()->filePath();

    Item * const propertiesItem = item->child(QLatin1String("Properties"));
    bool subProjectEnabled = true;
    if (propertiesItem)
        subProjectEnabled = checkItemCondition(propertiesItem);
    if (!subProjectEnabled)
        return;

    const QString projectFileDirPath = FileInfo::path(item->file()->filePath());
    const QString relativeFilePath = m_evaluator->property(item,
                                                           QLatin1String("filePath")).toString();
    QString subProjectFilePath = FileInfo::resolvePath(projectFileDirPath, relativeFilePath);
    if (referencedFilePaths.contains(subProjectFilePath))
        throw ErrorInfo(Tr::tr("Cycle detected while loading subproject file '%1'.")
                            .arg(relativeFilePath), item->location());
    Item *loadedItem = m_reader->readFile(subProjectFilePath);
    if (loadedItem->typeName() == QLatin1String("Product"))
        loadedItem = wrapWithProject(loadedItem);
    const bool inheritProperties
            = m_evaluator->boolValue(item, QLatin1String("inheritProperties"), true);

    if (inheritProperties)
        copyProperties(item->parent(), loadedItem);
    if (propertiesItem) {
        const Item::PropertyMap &overriddenProperties = propertiesItem->properties();
        for (Item::PropertyMap::ConstIterator it = overriddenProperties.constBegin();
             it != overriddenProperties.constEnd(); ++it) {
            loadedItem->setProperty(it.key(), overriddenProperties.value(it.key()));
        }
    }

    if (loadedItem->typeName() != QLatin1String("Project")) {
        ErrorInfo error;
        error.append(Tr::tr("Expected Project item, but encountered '%1'.")
                     .arg(loadedItem->typeName()), loadedItem->location());
        const ValuePtr &filePathProperty = item->properties().value(QLatin1String("filePath"));
        error.append(Tr::tr("The problematic file was referenced from here."),
                     filePathProperty->location());
        throw error;
    }

    Item::addChild(item, loadedItem);
    item->setScope(projectContext->scope);
    handleProject(projectContext->result, loadedItem,
                  QSet<QString>(referencedFilePaths) << subProjectFilePath);
}

void ModuleLoader::createAdditionalModuleInstancesInProduct(ProductContext *productContext)
{
    Item::Modules modulesToCheck;
    QSet<QStringList> modulesInProduct;
    foreach (const Item::Module &module, productContext->item->modules()) {
        modulesInProduct += module.name;
        modulesToCheck += module.item->prototype()->modules();
    }
    while (!modulesToCheck.isEmpty()) {
        Item::Module module = modulesToCheck.takeFirst();
        if (modulesInProduct.contains(module.name))
            continue;
        modulesInProduct += module.name;
        modulesToCheck += module.item->prototype()->modules();
        Item *instance = Item::create(m_pool);
        instantiateModule(productContext, productContext->item, instance, module.item->prototype(),
                          module.name);
        module.item = instance;
        productContext->item->modules().append(module);
    }
}

static Item *findTargetItem(Item *item, const QStringList &name, ItemPool *pool)
{
    Item *targetItem = item;
    ValuePtr v;
    for (int i = 0; i < name.count(); ++i) {
        v = targetItem->properties().value(name.at(i));
        if (v && v->type() == Value::ItemValueType)
            targetItem = v.staticCast<ItemValue>()->item();
        if (!v || !targetItem) {
            Item *newItem = Item::create(pool);
            targetItem->setProperty(name.at(i), ItemValue::create(newItem));
            targetItem = newItem;
        }
    }
    QBS_ASSERT(name.isEmpty() || targetItem != item, return 0);
    return targetItem;
}

extern bool debugProperties;

void ModuleLoader::handleGroup(ProductContext *productContext, Item *item)
{
    checkCancelation();
    propagateModulesFromProduct(productContext, item);
}

void ModuleLoader::handleArtifact(ProductContext *productContext, Item *item)
{
    checkCancelation();
    propagateModulesFromProduct(productContext, item);
}

void ModuleLoader::deferExportItem(ModuleLoader::ProductContext *productContext, Item *item)
{
    productContext->exportItems.append(item);
}

void ModuleLoader::handleProductModule(ModuleLoader::ProductContext *productContext,
                                       Item *item)
{
    m_logger.printWarning(ErrorInfo(Tr::tr("ProductModule {} is deprecated. "
                                     "Please use Export {} instead."), item->location()));
    deferExportItem(productContext, item);
}

static void mergeProperty(Item *dst, const QString &name, const ValuePtr &value)
{
    if (value->type() == Value::ItemValueType) {
        Item *valueItem = value.staticCast<ItemValue>()->item();
        if (!valueItem)
            return;
        Item *subItem = dst->itemProperty(name, true)->item();
        for (QMap<QString, ValuePtr>::const_iterator it = valueItem->properties().constBegin();
                it != valueItem->properties().constEnd(); ++it)
            mergeProperty(subItem, it.key(), it.value());
    } else {
        dst->setProperty(name, value);
    }
}

void ModuleLoader::mergeExportItems(ModuleLoader::ProductContext *productContext)
{
    Item *merged = Item::create(productContext->item->pool());
    merged->setTypeName(QLatin1String("Export"));
    QSet<Item *> exportItems;
    foreach (Item *exportItem, productContext->exportItems) {
        checkCancelation();
        if (Q_UNLIKELY(productContext->filesWithExportItem.contains(exportItem->file())))
            throw ErrorInfo(Tr::tr("Multiple Export items in one product are prohibited."),
                        exportItem->location());
        merged->setLocation(exportItem->location());
        productContext->filesWithExportItem += exportItem->file();
        exportItems.insert(exportItem);
        foreach (Item *child, exportItem->children())
            Item::addChild(merged, child);
        for (QMap<QString, ValuePtr>::const_iterator it = exportItem->properties().constBegin();
                it != exportItem->properties().constEnd(); ++it) {
            mergeProperty(merged, it.key(), it.value());
        }
    }

    QList<Item *> children = productContext->item->children();
    for (int i = 0; i < children.count();) {
        if (exportItems.contains(children.at(i)))
            children.removeAt(i);
        else
            ++i;
    }
    productContext->item->setChildren(children);
    Item::addChild(productContext->item, merged);

    DependsContext dependsContext;
    dependsContext.product = productContext;
    dependsContext.productDependencies = &productContext->info.usedProductsFromExportItem;
    resolveDependencies(&dependsContext, merged);
}

void ModuleLoader::propagateModulesFromProduct(ProductContext *productContext, Item *item)
{
    for (Item::Modules::const_iterator it = productContext->item->modules().constBegin();
         it != productContext->item->modules().constEnd(); ++it)
    {
        Item::Module m = *it;
        Item *targetItem = findTargetItem(item, m.name, m_pool);
        targetItem->setPrototype(m.item);
        targetItem->setScope(m.item->scope());
        targetItem->modules() = m.item->modules();

        // "parent" should point to the group/artifact parent
        targetItem->setParent(item->parent());

        // the outer item of a module is the product's instance of it
        targetItem->setOuterItem(m.item);   // ### Is this always the same as the scope item?

        m.item = targetItem;
        item->modules() += m;
    }
}

void ModuleLoader::resolveDependencies(DependsContext *dependsContext, Item *item)
{
    loadBaseModule(dependsContext->product, item);

    // Resolve all Depends items.
    QHash<Item *, ItemModuleList> loadedModules;
    ProductDependencyResults productDependencies;
    foreach (Item *child, item->children())
        if (child->typeName() == QLatin1String("Depends"))
            resolveDependsItem(dependsContext, item, child, &loadedModules[child],
                               &productDependencies);

    // Check Depends conditions after all modules are loaded.
    QSet<QString> loadedModuleNames;
    for (QHash<Item *, ItemModuleList>::const_iterator it = loadedModules.constBegin();
         it != loadedModules.constEnd(); ++it)
    {
        Item *dependsItem = it.key();
        if (checkItemCondition(dependsItem)) {
            foreach (const Item::Module &module, it.value()) {
                const QString fullName = fullModuleName(module.name);
                if (loadedModuleNames.contains(fullName)) {
                    m_logger.printWarning(ErrorInfo(Tr::tr("Duplicate dependency '%1'.").arg(fullName),
                                        item->location()));
                    continue;
                }
                loadedModuleNames.insert(fullName);
                item->modules() += module;
                resolveProbes(module.item);
            }
        }
    }

    // Check Depends conditions for all product dependencies.
    for (ProductDependencyResults::const_iterator it = productDependencies.constBegin();
             it != productDependencies.constEnd(); ++it) {
        Item *dependsItem = it->first;
        if (checkItemCondition(dependsItem))
            dependsContext->productDependencies->append(it->second);
    }
}

void ModuleLoader::resolveDependsItem(DependsContext *dependsContext, Item *item,
        Item *dependsItem, ItemModuleList *moduleResults,
        ProductDependencyResults *productResults)
{
    checkCancelation();
    const QString name = m_evaluator->property(dependsItem, "name").toString();
    const QStringList nameParts = name.split('.');
    if (Q_UNLIKELY(nameParts.count() > 2)) {
        QString msg = Tr::tr("There cannot be more than one dot in a module name.");
        throw ErrorInfo(msg, dependsItem->location());
    }

    QString superModuleName;
    QStringList submodules = toStringList(m_evaluator->property(dependsItem, "submodules"));
    if (nameParts.count() == 2) {
        if (Q_UNLIKELY(!submodules.isEmpty()))
            throw ErrorInfo(Tr::tr("Depends.submodules cannot be used if name contains a dot."),
                        dependsItem->location());
        superModuleName = nameParts.first();
        submodules += nameParts.last();
    }
    if (Q_UNLIKELY(submodules.count() > 1 && !dependsItem->id().isEmpty())) {
        QString msg = Tr::tr("A Depends item with more than one module cannot have an id.");
        throw ErrorInfo(msg, dependsItem->location());
    }
    if (superModuleName.isEmpty()) {
        if (submodules.isEmpty())
            submodules += name;
        else
            superModuleName = name;
    }

    QStringList moduleNames;
    foreach (const QString &submoduleName, submodules)
        moduleNames += submoduleName;

    Item::Module result;
    foreach (const QString &moduleName, moduleNames) {
        QStringList qualifiedModuleName(moduleName);
        if (!superModuleName.isEmpty())
            qualifiedModuleName.prepend(superModuleName);
        Item *moduleItem = loadModule(dependsContext->product, item, dependsItem->location(),
                                        dependsItem->id(), qualifiedModuleName);
        if (moduleItem) {
            if (m_logger.traceEnabled())
                m_logger.qbsTrace() << "module loaded: " << fullModuleName(qualifiedModuleName);
            result.name = qualifiedModuleName;
            result.item = moduleItem;
            moduleResults->append(result);
        } else {
            ModuleLoaderResult::ProductInfo::Dependency dependency;
            dependency.name = moduleName;
            dependency.required = m_evaluator->property(item, QLatin1String("required")).toBool();
            dependency.failureMessage
                    = m_evaluator->property(item, QLatin1String("failureMessage")).toString();
            productResults->append(ProductDependencyResult(dependsItem, dependency));
        }
    }
}

Item *ModuleLoader::moduleInstanceItem(Item *item, const QStringList &moduleName)
{
    Item *instance = item;
    for (int i = 0; i < moduleName.count(); ++i) {
        bool createNewItem = true;
        const ValuePtr v = instance->properties().value(moduleName.at(i));
        if (v && v->type() == Value::ItemValueType) {
            const ItemValuePtr iv = v.staticCast<ItemValue>();
            if (iv->item()) {
                createNewItem = false;
                instance = iv->item();
            }
        }
        if (createNewItem) {
            Item *newItem = Item::create(m_pool);
            instance->setProperty(moduleName.at(i), ItemValue::create(newItem));
            instance = newItem;
        }
    }
    return instance;
}

Item *ModuleLoader::loadModule(ProductContext *productContext, Item *item,
        const CodeLocation &dependsItemLocation,
        const QString &moduleId, const QStringList &moduleName)
{
    Item *moduleInstance = moduleId.isEmpty()
            ? moduleInstanceItem(item, moduleName)
            : moduleInstanceItem(item, QStringList(moduleId));
    if (!moduleInstance->typeName().isNull()) {
        // already handled
        return moduleInstance;
    }

    const QStringList extraSearchPaths = productContext->extraSearchPaths.isEmpty()
            ? productContext->project->extraSearchPaths : productContext->extraSearchPaths;
    Item *modulePrototype = searchAndLoadModuleFile(productContext, dependsItemLocation,
                                                      moduleName, extraSearchPaths);
    if (!modulePrototype)
        return 0;
    instantiateModule(productContext, item, moduleInstance, modulePrototype, moduleName);
    return moduleInstance;
}

Item *ModuleLoader::searchAndLoadModuleFile(ProductContext *productContext,
        const CodeLocation &dependsItemLocation, const QStringList &moduleName,
        const QStringList &extraSearchPaths)
{
    QStringList searchPaths = extraSearchPaths;
    searchPaths.append(m_moduleSearchPaths);

    bool triedToLoadModule = moduleName.count() > 1;
    const QString fullName = fullModuleName(moduleName);
    foreach (const QString &path, searchPaths) {
        const QString dirPath = findExistingModulePath(path, moduleName);
        if (dirPath.isEmpty())
            continue;
        QStringList moduleFileNames = m_moduleDirListCache.value(dirPath);
        if (moduleFileNames.isEmpty()) {
            QDirIterator dirIter(dirPath, QStringList(QLatin1String("*.qbs")));
            while (dirIter.hasNext())
                moduleFileNames += dirIter.next();

            m_moduleDirListCache.insert(dirPath, moduleFileNames);
        }
        foreach (const QString &filePath, moduleFileNames) {
            triedToLoadModule = true;
            Item *module = loadModuleFile(productContext, fullName,
                                            moduleName.count() == 1
                                                && moduleName.first() == QLatin1String("qbs"),
                                            filePath);
            if (module)
                return module;
        }
    }

    if (Q_UNLIKELY(triedToLoadModule))
        throw ErrorInfo(Tr::tr("Module %1 could not be loaded.").arg(fullName),
                    dependsItemLocation);

    return 0;
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
    case PropertyDeclaration::Variant:
        break;
    case PropertyDeclaration::Verbatim:
        return QVariant::String;
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
                        .arg(v.toString(), name.join(QLatin1String("."))));
    }
    return c;
}

static PropertyDeclaration firstValidPropertyDeclaration(Item *item, const QString &name)
{
    PropertyDeclaration decl;
    do {
        decl = item->propertyDeclarations().value(name);
        if (decl.isValid())
            return decl;
        item = item->prototype();
    } while (item);
    return decl;
}

Item *ModuleLoader::loadModuleFile(ProductContext *productContext, const QString &fullModuleName,
        bool isBaseModule, const QString &filePath)
{
    checkCancelation();
    Item *module = productContext->moduleItemCache.value(filePath);
    if (module) {
        m_logger.qbsTrace() << "[LDR] loadModuleFile cache hit for " << filePath;
        return module;
    }

    module = productContext->project->moduleItemCache.value(filePath);
    if (module) {
        m_logger.qbsTrace() << "[LDR] loadModuleFile returns clone for " << filePath;
        return module->clone(m_pool);
    }

    m_logger.qbsTrace() << "[LDR] loadModuleFile " << filePath;
    module = m_reader->readFile(filePath);
    if (!isBaseModule) {
        DependsContext dependsContext;
        dependsContext.product = productContext;
        dependsContext.productDependencies = &productContext->info.usedProducts;
        resolveDependencies(&dependsContext, module);
    }
    if (!checkItemCondition(module)) {
        m_logger.qbsTrace() << "[LDR] module condition is false";
        return 0;
    }

    // Module properties that are defined in the profile are used as default values.
    const QVariantMap profileModuleProperties
            = m_buildConfigProperties.value(fullModuleName).toMap();
    for (QVariantMap::const_iterator vmit = profileModuleProperties.begin();
            vmit != profileModuleProperties.end(); ++vmit)
    {
        if (Q_UNLIKELY(!module->hasProperty(vmit.key())))
            throw ErrorInfo(Tr::tr("Unknown property: %1.%2").arg(fullModuleName, vmit.key()));
        const PropertyDeclaration decl = firstValidPropertyDeclaration(module, vmit.key());
        module->setProperty(vmit.key(),
                VariantValue::create(convertToPropertyType(vmit.value(), decl.type,
                        QStringList(fullModuleName), vmit.key())));
    }

    productContext->moduleItemCache.insert(filePath, module);
    productContext->project->moduleItemCache.insert(filePath, module);
    return module;
}

void ModuleLoader::loadBaseModule(ProductContext *productContext, Item *item)
{
    const QStringList baseModuleName(QLatin1String("qbs"));
    Item::Module baseModuleDesc;
    baseModuleDesc.name = baseModuleName;
    baseModuleDesc.item = loadModule(productContext, item, CodeLocation(), QString(),
                                     baseModuleName);
    if (Q_UNLIKELY(!baseModuleDesc.item))
        throw ErrorInfo(Tr::tr("Cannot load base qbs module."));
    baseModuleDesc.item->setProperty(QLatin1String("getenv"),
                                     BuiltinValue::create(BuiltinValue::GetEnvFunction));
    baseModuleDesc.item->setProperty(QLatin1String("getHostOS"),
                                     BuiltinValue::create(BuiltinValue::GetHostOSFunction));
    item->modules() += baseModuleDesc;
}

static void collectItemsWithId_impl(Item *item, QList<Item *> *result)
{
    if (!item->id().isEmpty())
        result->append(item);
    foreach (Item *child, item->children())
        collectItemsWithId_impl(child, result);
}

static QList<Item *> collectItemsWithId(Item *item)
{
    QList<Item *> result;
    collectItemsWithId_impl(item, &result);
    return result;
}

void ModuleLoader::instantiateModule(ProductContext *productContext, Item *instanceScope,
                                     Item *moduleInstance, Item *modulePrototype,
                                     const QStringList &moduleName)
{
    const QString fullName = fullModuleName(moduleName);
    modulePrototype->setProperty(QLatin1String("name"),
                                 VariantValue::create(fullName));

    moduleInstance->setPrototype(modulePrototype);
    moduleInstance->setFile(modulePrototype->file());
    moduleInstance->setLocation(modulePrototype->location());
    moduleInstance->setTypeName(modulePrototype->typeName());

    // create module scope
    Item *moduleScope = Item::create(m_pool);
    moduleScope->setScope(instanceScope);
    copyProperty(QLatin1String("project"), productContext->project->scope, moduleScope);
    copyProperty(QLatin1String("product"), productContext->scope, moduleScope);
    moduleInstance->setScope(moduleScope);
    moduleInstance->setModuleInstanceFlag(true);

    QHash<Item *, Item *> prototypeInstanceMap;
    prototypeInstanceMap[modulePrototype] = moduleInstance;

    // create instances for every child of the prototype
    createChildInstances(productContext, moduleInstance, modulePrototype, &prototypeInstanceMap);

    // create ids from from the prototype in the instance
    if (modulePrototype->file()->idScope()) {
        foreach (Item *itemWithId, collectItemsWithId(modulePrototype)) {
            Item *idProto = itemWithId;
            Item *idInstance = prototypeInstanceMap.value(idProto);
            QBS_ASSERT(idInstance, continue);
            ItemValuePtr idInstanceValue = ItemValue::create(idInstance);
            moduleScope->setProperty(itemWithId->id(), idInstanceValue);
        }
    }

    // create module instances for the dependencies of this module
    foreach (Item::Module m, modulePrototype->modules()) {
        Item *depinst = moduleInstanceItem(moduleInstance, m.name);
        const bool safetyCheck = true;
        if (safetyCheck) {
            Item *obj = moduleInstance;
            for (int i = 0; i < m.name.count(); ++i) {
                ItemValuePtr iv = obj->itemProperty(m.name.at(i));
                QBS_CHECK(iv);
                obj = iv->item();
                QBS_CHECK(obj);
            }
            QBS_CHECK(obj == depinst);
        }
        depinst->setPrototype(m.item);
        depinst->setFile(m.item->file());
        depinst->setLocation(m.item->location());
        depinst->setTypeName(m.item->typeName());
        depinst->setScope(moduleInstance);
        m.item = depinst;
        moduleInstance->modules() += m;
    }

    // override module properties given on the command line
    const QVariantMap userModuleProperties = m_overriddenProperties.value(fullName).toMap();
    for (QVariantMap::const_iterator vmit = userModuleProperties.begin();
         vmit != userModuleProperties.end(); ++vmit) {
        if (Q_UNLIKELY(!moduleInstance->hasProperty(vmit.key()))) {
            throw ErrorInfo(Tr::tr("Unknown property: %1.%2")
                            .arg(fullModuleName(moduleName), vmit.key()));
        }
        const PropertyDeclaration decl = firstValidPropertyDeclaration(moduleInstance, vmit.key());
        moduleInstance->setProperty(vmit.key(),
                VariantValue::create(convertToPropertyType(vmit.value(), decl.type, moduleName,
                        vmit.key())));
    }
}

void ModuleLoader::createChildInstances(ProductContext *productContext, Item *instance,
                                        Item *prototype,
                                        QHash<Item *, Item *> *prototypeInstanceMap) const
{
    foreach (Item *childPrototype, prototype->children()) {
        Item *childInstance = Item::create(m_pool);
        prototypeInstanceMap->insert(childPrototype, childInstance);
        childInstance->setPrototype(childPrototype);
        childInstance->setTypeName(childPrototype->typeName());
        childInstance->setFile(childPrototype->file());
        childInstance->setLocation(childPrototype->location());
        childInstance->setScope(productContext->scope);
        Item::addChild(instance, childInstance);
        createChildInstances(productContext, childInstance, childPrototype, prototypeInstanceMap);
    }
}

void ModuleLoader::resolveProbes(Item *item)
{
    foreach (Item *child, item->children())
        if (child->typeName() == QLatin1String("Probe"))
            resolveProbe(item, child);
}

void ModuleLoader::resolveProbe(Item *parent, Item *probe)
{
    const JSSourceValueConstPtr configureScript = probe->sourceProperty(QLatin1String("configure"));
    if (Q_UNLIKELY(!configureScript))
        throw ErrorInfo(Tr::tr("Probe.configure must be set."), probe->location());
    typedef QPair<QString, QScriptValue> ProbeProperty;
    QList<ProbeProperty> probeBindings;
    for (Item *obj = probe; obj; obj = obj->prototype()) {
        foreach (const QString &name, obj->properties().keys()) {
            if (name == QLatin1String("configure"))
                continue;
            QScriptValue sv = m_evaluator->property(probe, name);
            if (Q_UNLIKELY(sv.isError())) {
                ValuePtr value = obj->property(name);
                throw ErrorInfo(sv.toString(), value ? value->location() : CodeLocation());
            }
            probeBindings += ProbeProperty(name, sv);
        }
    }
    QScriptValue scope = m_engine->newObject();
    m_engine->currentContext()->pushScope(m_evaluator->scriptValue(parent));
    m_engine->currentContext()->pushScope(scope);
    m_engine->currentContext()->pushScope(m_evaluator->fileScope(configureScript->file()));
    foreach (const ProbeProperty &b, probeBindings)
        scope.setProperty(b.first, b.second);
    QScriptValue sv = m_engine->evaluate(configureScript->sourceCode());
    if (Q_UNLIKELY(sv.isError()))
        throw ErrorInfo(sv.toString(), configureScript->location());
    foreach (const ProbeProperty &b, probeBindings) {
        const QVariant newValue = scope.property(b.first).toVariant();
        if (newValue != b.second.toVariant())
            probe->setProperty(b.first, VariantValue::create(newValue));
    }
    m_engine->currentContext()->popScope();
    m_engine->currentContext()->popScope();
    m_engine->currentContext()->popScope();
}

void ModuleLoader::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled()) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration %1.")
                    .arg(TopLevelProject::deriveId(m_buildConfigProperties)));
    }
}

bool ModuleLoader::checkItemCondition(Item *item)
{
    return m_evaluator->boolValue(item, QLatin1String("condition"), true);
}

QStringList ModuleLoader::readExtraSearchPaths(Item *item)
{
    QStringList result;
    QScriptValue scriptValue = m_evaluator->property(item, QLatin1String("moduleSearchPaths"));
    const ValueConstPtr prop = item->property(QLatin1String("moduleSearchPaths"));
    foreach (const QString &path, toStringList(scriptValue))
        result += FileInfo::resolvePath(FileInfo::path(prop->location().fileName()), path);
    return result;
}

void ModuleLoader::copyProperties(const Item *sourceProject, Item *targetProject)
{
    if (!sourceProject)
        return;
    const QList<PropertyDeclaration> &builtinProjectProperties
            = m_reader->builtins()->declarationsForType(QLatin1String("Project"));
    QSet<QString> builtinProjectPropertyNames;
    foreach (const PropertyDeclaration &p, builtinProjectProperties)
        builtinProjectPropertyNames << p.name;

    for (Item::PropertyDeclarationMap::ConstIterator it
         = sourceProject->propertyDeclarations().constBegin();
         it != sourceProject->propertyDeclarations().constEnd(); ++it) {

        // We must not inherit built-in properties such as "name", but "moduleSearchPaths" is
        // an exception.
        if (it.key() == QLatin1String("moduleSearchPaths")) {
            const JSSourceValueConstPtr &v
                    = targetProject->property(it.key()).dynamicCast<const JSSourceValue>();
            QBS_ASSERT(v, continue);
            if (v->sourceCode() == QLatin1String("undefined"))
                copyProperty(it.key(), sourceProject, targetProject);
            continue;
        }

        if (builtinProjectPropertyNames.contains(it.key()))
            continue;

        if (targetProject->properties().contains(it.key()))
            continue; // Ignore stuff the target project already has.

        targetProject->setPropertyDeclaration(it.key(), it.value());
        copyProperty(it.key(), sourceProject, targetProject);
    }
}

Item *ModuleLoader::wrapWithProject(Item *item)
{
    Item *prj = Item::create(item->pool());
    prj->setChildren(QList<Item *>() << item);
    item->setParent(prj);
    prj->setTypeName("Project");
    prj->setFile(item->file());
    prj->setLocation(item->location());
    m_reader->builtins()->setupItemForBuiltinType(prj);
    return prj;
}

QString ModuleLoader::findExistingModulePath(const QString &searchPath,
        const QStringList &moduleName)
{
    QString dirPath = searchPath;
    foreach (const QString &moduleNamePart, moduleName) {
        dirPath = FileInfo::resolvePath(dirPath, moduleNamePart);
        if (!FileInfo::exists(dirPath) || !FileInfo::isFileCaseCorrect(dirPath))
            return QString();
    }
    return dirPath;
}

void ModuleLoader::copyProperty(const QString &propertyName, const Item *source,
                                Item *destination)
{
    destination->setProperty(propertyName, source->property(propertyName));
}

void ModuleLoader::setScopeForDescendants(Item *item, Item *scope)
{
    foreach (Item *child, item->children()) {
        child->setScope(scope);
        setScopeForDescendants(child, scope);
    }
}

QString ModuleLoader::fullModuleName(const QStringList &moduleName)
{
    // Currently the same as the module sub directory.
    // ### Might be nicer to be a valid JS identifier.
#if QT_VERSION >= 0x050000
    return moduleName.join(QLatin1Char('/'));
#else
    return moduleName.join(QLatin1String("/"));
#endif
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
        const PropertyDeclaration decl = item->propertyDeclarations().value(it.key());
        item->setProperty(it.key(),
                VariantValue::create(convertToPropertyType(it.value(), decl.type,
                        QStringList(buildConfigKey), it.key())));
    }
}

} // namespace Internal
} // namespace qbs
