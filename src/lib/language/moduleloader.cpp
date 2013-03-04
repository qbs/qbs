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

#include "builtinvalue.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "itemreader.h"
#include "scriptengine.h"
#include <jsextensions/file.h>
#include <jsextensions/process.h>
#include <jsextensions/textfile.h>
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
    , m_logger(logger)
    , m_progressObserver(0)
    , m_reader(new ItemReader(builtins))
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

ModuleLoaderResult ModuleLoader::load(const QString &filePath, const QVariantMap &userProperties,
                                      bool wrapWithProjectItem)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] load" << filePath;
    m_userProperties = userProperties;

    ItemPtr root = m_reader->readFile(filePath);
    if (!root)
        return ModuleLoaderResult();

    if (wrapWithProjectItem && root->typeName() != QLatin1String("Project"))
        root = wrapWithProject(root);

    ModuleLoaderResult result;
    handleProject(&result, root);
    result.root = root;
    return result;
}

void ModuleLoader::handleProject(ModuleLoaderResult *loadResult, const ItemPtr &item)
{
    ProjectContext projectContext;
    projectContext.result = loadResult;
    projectContext.extraSearchPaths = readExtraSearchPaths(item);
    projectContext.extraSearchPaths += item->file()->dirPath();
    projectContext.item = item;
    ItemValuePtr itemValue = ItemValue::create();
    itemValue->setItemWeakPointer(item);
    projectContext.scope = Item::create();
    projectContext.scope->setProperty(QLatin1String("project"), itemValue);

    foreach (const ItemPtr &child, item->children()) {
        child->setScope(projectContext.scope);
        if (child->typeName() == QLatin1String("Product"))
            handleProduct(&projectContext, child);
    }

    const QString projectFileDirPath = FileInfo::path(item->file()->filePath());
    const QStringList refs = toStringList(m_evaluator->property(item, "references"));
    foreach (const QString &filePath, refs) {
        const QString absReferencePath = FileInfo::resolvePath(projectFileDirPath, filePath);
        ItemPtr subprj = m_reader->readFile(absReferencePath);
        if (subprj->typeName() != "Product")
            throw Error("TODO: implement project refs");
        subprj->setParent(projectContext.item);
        QList<ItemPtr> projectChildren = projectContext.item->children();
        projectChildren += subprj;
        projectContext.item->setChildren(projectChildren);
        handleProduct(&projectContext, subprj);
    }
}

void ModuleLoader::handleProduct(ProjectContext *projectContext, const ItemPtr &item)
{
    checkCancelation();
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] handleProduct " << item->file()->filePath();
    ProductContext productContext;
    productContext.project = projectContext;
    productContext.extraSearchPaths = readExtraSearchPaths(item);
    productContext.item = item;
    ItemValuePtr itemValue = ItemValue::create();
    itemValue->setItemWeakPointer(item);
    productContext.scope = Item::create();
    productContext.scope->setProperty(QLatin1String("product"), itemValue);
    productContext.scope->setScope(projectContext->scope);
    DependsContext dependsContext;
    dependsContext.product = &productContext;
    dependsContext.productDependencies = &productContext.info.usedProducts;
    resolveDependencies(&dependsContext, item);
    createAdditionalModuleInstancesInProduct(&productContext);

    foreach (const ItemPtr &child, item->children()) {
        child->setScope(productContext.scope);
        if (child->typeName() == QLatin1String("Group"))
            handleGroup(&productContext, child);
        else if (child->typeName() == QLatin1String("Artifact"))
            handleArtifact(&productContext, child);
        else if (child->typeName() == QLatin1String("ProductModule"))
            handleProductModule(&productContext, child);
        else if (child->typeName() == QLatin1String("Probe"))
            resolveProbe(item, child);
    }

    projectContext->result->productInfos.insert(item, productContext.info);
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
        ItemPtr instance = Item::create();
        instantiateModule(productContext, productContext->item, instance, module.item->prototype(),
                          module.name);
        module.item = instance;
        productContext->item->modules().append(module);
    }
}

static ItemPtr findTargetItem(const ItemPtr &item, const QStringList &name)
{
    ItemPtr targetItem = item;
    ValuePtr v;
    for (int i = 0; i < name.count(); ++i) {
        v = targetItem->properties().value(name.at(i));
        if (v && v->type() == Value::ItemValueType)
            targetItem = v.staticCast<ItemValue>()->item();
        if (!v || !targetItem) {
            ItemPtr newItem = Item::create();
            targetItem->setProperty(name.at(i), ItemValue::create(newItem));
            targetItem = newItem;
        }
    }
    QBS_ASSERT(name.isEmpty() || targetItem != item, return ItemPtr());
    return targetItem;
}

extern bool debugProperties;

void ModuleLoader::handleGroup(ProductContext *productContext, const ItemPtr &item)
{
    checkCancelation();
    propagateModulesFromProduct(productContext, item);
}

void ModuleLoader::handleArtifact(ProductContext *productContext, const ItemPtr &item)
{
    checkCancelation();
    propagateModulesFromProduct(productContext, item);
}

void ModuleLoader::handleProductModule(ModuleLoader::ProductContext *productContext,
                                       const ItemPtr &item)
{
    checkCancelation();
    if (productContext->filesWithProductModule.contains(item->file()))
        throw Error(Tr::tr("Multiple ProductModule items in one product are prohibited."),
                    item->location());
    productContext->filesWithProductModule += item->file();
    DependsContext dependsContext;
    dependsContext.product = productContext;
    dependsContext.productDependencies = &productContext->info.usedProductsFromProductModule;
    resolveDependencies(&dependsContext, item);
}

void ModuleLoader::propagateModulesFromProduct(ProductContext *productContext, const ItemPtr &item)
{
    for (Item::Modules::const_iterator it = productContext->item->modules().constBegin();
         it != productContext->item->modules().constEnd(); ++it)
    {
        Item::Module m = *it;
        ItemPtr targetItem = findTargetItem(item, m.name);
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

void ModuleLoader::resolveDependencies(DependsContext *dependsContext, const ItemPtr &item)
{
    const QStringList baseModuleName(QLatin1String("qbs"));
    Item::Module baseModuleDesc;
    baseModuleDesc.name = baseModuleName;
    baseModuleDesc.item = loadModule(dependsContext->product, item, QString(), baseModuleName);
    if (!baseModuleDesc.item)
        throw Error(Tr::tr("Cannot load base qbs module."));
    baseModuleDesc.item->setProperty(QLatin1String("getenv"),
                                     BuiltinValue::create(BuiltinValue::GetEnvFunction));
    baseModuleDesc.item->setProperty(QLatin1String("getHostOS"),
                                     BuiltinValue::create(BuiltinValue::GetHostOSFunction));
    item->modules() += baseModuleDesc;

    // Resolve all Depends items.
    QHash<ItemPtr, ItemModuleList> loadedModules;
    foreach (const ItemPtr &child, item->children())
        if (child->typeName() == QLatin1String("Depends"))
            resolveDependsItem(dependsContext, item, child, &loadedModules[child]);

    // Check Depends conditions after all modules are loaded.
    for (QHash<ItemPtr, ItemModuleList>::const_iterator it = loadedModules.constBegin();
         it != loadedModules.constEnd(); ++it)
    {
        const ItemPtr &dependsItem = it.key();
        if (checkItemCondition(dependsItem)) {
            foreach (const Item::Module &module, it.value()) {
                item->modules() += module;
                resolveProbes(module.item);
            }
        }
    }
}

void ModuleLoader::resolveDependsItem(DependsContext *dependsContext, const ItemPtr &item,
                                     const ItemPtr &dependsItem, ItemModuleList *results)
{
    checkCancelation();
    const QString name = m_evaluator->property(dependsItem, "name").toString().toLower();
    const QStringList nameParts = name.split('.');
    if (nameParts.count() > 2) {
        QString msg = Tr::tr("There cannot be more than one dot in a module name.");
        throw Error(msg, dependsItem->location());
    }

    QString superModuleName;
    QStringList submodules = toStringList(m_evaluator->property(dependsItem, "submodules"));
    if (nameParts.count() == 2) {
        if (!submodules.isEmpty())
            throw Error(Tr::tr("Depends.submodules cannot be used if name contains a dot."),
                        dependsItem->location());
        superModuleName = nameParts.first();
        submodules += nameParts.last();
    }
    if (submodules.count() > 1 && !dependsItem->id().isEmpty()) {
        QString msg = Tr::tr("A Depends item with more than one module cannot have an id.");
        throw Error(msg, dependsItem->location());
    }
    if (superModuleName.isEmpty()) {
        if (submodules.isEmpty())
            submodules += name;
        else
            superModuleName = name;
    }

    QStringList moduleNames;
    foreach (const QString &submoduleName, submodules)
        moduleNames += submoduleName.toLower();

    Item::Module result;
    foreach (const QString &moduleName, moduleNames) {
        QStringList qualifiedModuleName(moduleName);
        if (!superModuleName.isEmpty())
            qualifiedModuleName.prepend(superModuleName);
        ItemPtr moduleItem = loadModule(dependsContext->product, item, dependsItem->id(),
                                        qualifiedModuleName);
        if (moduleItem) {
            if (m_logger.traceEnabled())
                m_logger.qbsTrace() << "module loaded: " << fullModuleName(qualifiedModuleName);
            result.name = qualifiedModuleName;
            result.item = moduleItem;
            results->append(result);
        } else {
            ModuleLoaderResult::ProductInfo::Dependency dependency;
            dependency.name = moduleName;
            dependency.required = m_evaluator->property(item, QLatin1String("required")).toBool();
            dependency.failureMessage
                    = m_evaluator->property(item, QLatin1String("failureMessage")).toString();
            dependsContext->productDependencies->append(dependency);
        }
    }
}

ItemPtr ModuleLoader::moduleInstanceItem(const ItemPtr &item, const QStringList &moduleName)
{
    ItemPtr instance = item;
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
            ItemPtr newItem = Item::create();
            instance->setProperty(moduleName.at(i), ItemValue::create(newItem));
            instance = newItem;
        }
    }
    return instance;
}

ItemPtr ModuleLoader::loadModule(ProductContext *productContext, const ItemPtr &item,
                                 const QString &moduleId, const QStringList &moduleName)
{
    const ItemPtr moduleInstance = moduleId.isEmpty()
            ? moduleInstanceItem(item, moduleName)
            : moduleInstanceItem(item, QStringList(moduleId));
    if (!moduleInstance->typeName().isNull()) {
        // already handled
        return moduleInstance;
    }

    const QStringList extraSearchPaths = productContext->extraSearchPaths.isEmpty()
            ? productContext->project->extraSearchPaths : productContext->extraSearchPaths;
    ItemPtr modulePrototype = searchAndLoadModuleFile(productContext, moduleName, extraSearchPaths);
    if (!modulePrototype)
        return ItemPtr();
    instantiateModule(productContext, item, moduleInstance, modulePrototype, moduleName);
    return moduleInstance;
}

ItemPtr ModuleLoader::searchAndLoadModuleFile(ProductContext *productContext,
                                             const QStringList &moduleName,
                                             const QStringList &extraSearchPaths)
{
    QStringList searchPaths = extraSearchPaths;
    searchPaths.append(m_moduleSearchPaths);

    const QString moduleSubDir = ModuleLoader::moduleSubDir(moduleName);
    bool triedToLoadModule = false;
    foreach (const QString &path, searchPaths) {
        QString dirPath = FileInfo::resolvePath(path, moduleSubDir);
        QStringList moduleFileNames = m_moduleDirListCache.value(dirPath);
        if (moduleFileNames.isEmpty()) {
            QString origDirPath = dirPath;
            FileInfo dirInfo(dirPath);
            if (!dirInfo.isDir()) {
                bool found = false;
                if (HostOsInfo::isWindowsHost()) {
                    // On case sensitive file systems try to find the path.
                    QStringList subPaths = moduleName;
                    QDir dir(path);
                    if (!dir.cd(moduleSearchSubDir))
                        continue;
                    do {
                        QStringList lst = dir.entryList(QStringList(subPaths.takeFirst()),
                                                        QDir::Dirs);
                        if (lst.count() != 1)
                            break;
                        if (!dir.cd(lst.first()))
                            break;
                        if (subPaths.isEmpty()) {
                            found = true;
                            dirPath = dir.absolutePath();
                        }
                    } while (!found);
                }
                if (!found)
                    continue;
            }
            QDirIterator dirIter(dirPath, QStringList(QLatin1String("*.qbs")));
            while (dirIter.hasNext())
                moduleFileNames += dirIter.next();

            m_moduleDirListCache.insert(origDirPath, moduleFileNames);
        }
        foreach (const QString &filePath, moduleFileNames) {
            triedToLoadModule = true;
            ItemPtr module = loadModuleFile(productContext,
                                            moduleName.count() == 1
                                                && moduleName.first() == QLatin1String("qbs"),
                                            filePath);
            if (module)
                return module;
        }
    }

    if (triedToLoadModule)
        throw Error(Tr::tr("Module %1 could not be loaded.").arg(fullModuleName(moduleName)));

    return ItemPtr();
}

ItemPtr ModuleLoader::loadModuleFile(ProductContext *productContext, bool isBaseModule,
                                     const QString &filePath)
{
    checkCancelation();
    ItemPtr module = productContext->moduleItemCache.value(filePath);
    if (module) {
        m_logger.qbsTrace() << "[LDR] loadModuleFile cache hit for " << filePath;
        return module;
    }

    if (module = productContext->project->moduleItemCache.value(filePath)) {
        m_logger.qbsTrace() << "[LDR] loadModuleFile returns clone for " << filePath;
        return module->clone();
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
        return ItemPtr();
    }

    productContext->moduleItemCache.insert(filePath, module);
    productContext->project->moduleItemCache.insert(filePath, module);
    return module;
}

static void collectItemsWithId_impl(const ItemPtr &item, QList<ItemPtr> *result)
{
    if (!item->id().isEmpty())
        result->append(item);
    foreach (const ItemPtr &child, item->children())
        collectItemsWithId_impl(child, result);
}

static QList<ItemPtr> collectItemsWithId(const ItemPtr &item)
{
    QList<ItemPtr> result;
    collectItemsWithId_impl(item, &result);
    return result;
}

void ModuleLoader::instantiateModule(ProductContext *productContext, const ItemPtr &instanceScope,
                                     const ItemPtr &moduleInstance, const ItemPtr &modulePrototype,
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
    ItemPtr moduleScope = Item::create();
    moduleScope->setScope(instanceScope);
    copyProperty(QLatin1String("project"), productContext->project->scope, moduleScope);
    copyProperty(QLatin1String("product"), productContext->scope, moduleScope);
    moduleInstance->setScope(moduleScope);

    QHash<ItemPtr, ItemPtr> prototypeInstanceMap;
    prototypeInstanceMap[modulePrototype] = moduleInstance;

    // create instances for every child of the prototype
    createChildInstances(productContext, moduleInstance, modulePrototype, &prototypeInstanceMap);

    // create ids from from the prototype in the instance
    if (modulePrototype->file()->idScope()) {
        foreach (const ItemPtr itemWithId, collectItemsWithId(modulePrototype)) {
            ItemPtr idProto = itemWithId;
            ItemPtr idInstance = prototypeInstanceMap.value(idProto);
            QBS_ASSERT(idInstance, continue);
            ItemValuePtr idInstanceValue = ItemValue::create();
            idInstanceValue->setItemWeakPointer(idInstance);
            moduleScope->setProperty(itemWithId->id(), idInstanceValue);
        }
    }

    // create module instances for the dependencies of this module
    foreach (Item::Module m, modulePrototype->modules()) {
        ItemPtr depinst = moduleInstanceItem(moduleInstance, m.name);
        const bool safetyCheck = true;
        if (safetyCheck) {
            ItemPtr obj = moduleInstance;
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
    const QVariantMap userModuleProperties = m_userProperties.value(fullName).toMap();
    for (QVariantMap::const_iterator vmit = userModuleProperties.begin();
         vmit != userModuleProperties.end(); ++vmit)
    {
        if (!moduleInstance->hasProperty(vmit.key()))
            throw Error(Tr::tr("Unknown property: %1.%2").arg(fullModuleName(moduleName), vmit.key()));
        moduleInstance->setProperty(vmit.key(), VariantValue::create(vmit.value()));
    }
}

void ModuleLoader::createChildInstances(ProductContext *productContext, const ItemPtr &instance,
                                        const ItemPtr &prototype,
                                        QHash<ItemPtr, ItemPtr> *prototypeInstanceMap) const
{
    foreach (const ItemPtr &childPrototype, prototype->children()) {
        ItemPtr childInstance = Item::create();
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

void ModuleLoader::resolveProbes(const ItemPtr &item)
{
    foreach (const ItemPtr &child, item->children())
        if (child->typeName() == QLatin1String("Probe"))
            resolveProbe(item, child);
}

void ModuleLoader::resolveProbe(const ItemPtr &parent, const ItemPtr &probe)
{
    const JSSourceValueConstPtr configureScript = probe->sourceProperty(QLatin1String("configure"));
    if (!configureScript)
        throw Error(Tr::tr("Probe.configure must be set."), probe->location());
    typedef QPair<QString, QScriptValue> ProbeProperty;
    QList<ProbeProperty> probeBindings;
    for (ItemPtr obj = probe; obj; obj = obj->prototype()) {
        foreach (const QString &name, obj->properties().keys()) {
            if (name == QLatin1String("configure"))
                continue;
            QScriptValue sv = m_evaluator->property(probe, name);
            if (sv.isError()) {
                ValuePtr value = obj->property(name);
                throw Error(sv.toString(), value ? value->location() : CodeLocation());
            }
            probeBindings += ProbeProperty(name, sv);
        }
    }
    QScriptValue scope = m_engine->newObject();
    File::init(scope);
    Process::init(scope);
    TextFile::init(scope);
    m_engine->currentContext()->pushScope(m_evaluator->scriptValue(parent));
    m_engine->currentContext()->pushScope(scope);
    foreach (const ProbeProperty &b, probeBindings)
        scope.setProperty(b.first, b.second);
    QScriptValue sv = m_engine->evaluate(configureScript->sourceCode());
    if (sv.isError())
        throw Error(sv.toString(), configureScript->location());
    foreach (const ProbeProperty &b, probeBindings) {
        const QVariant newValue = scope.property(b.first).toVariant();
        if (newValue != b.second.toVariant())
            probe->setProperty(b.first, VariantValue::create(newValue));
    }
    m_engine->currentContext()->popScope();
    m_engine->currentContext()->popScope();
}

void ModuleLoader::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled())
        throw Error(Tr::tr("Loading canceled."));
}

bool ModuleLoader::checkItemCondition(const ItemPtr &item)
{
    QScriptValue value = m_evaluator->property(item, QLatin1String("condition"));
    if (!value.isValid() || value.isUndefined()) {
        // Item doesn't have a condition binding. Handled as true.
        return true;
    }
    if (value.isError()) {
        CodeLocation location;
        ValuePtr prop = item->property("condition");
        if (prop && prop->type() == Value::JSSourceValueType)
            location = prop.staticCast<JSSourceValue>()->location();
        Error e(Tr::tr("Error in condition."), location);
        e.append(value.toString());
        throw e;
    }
    return value.toBool();
}

QStringList ModuleLoader::readExtraSearchPaths(const ItemPtr &item)
{
    QStringList result;
    QScriptValue scriptValue = m_evaluator->property(item, QLatin1String("moduleSearchPaths"));
    foreach (const QString &path, toStringList(scriptValue))
        result += FileInfo::resolvePath(item->file()->dirPath(), path);
    return result;
}

ItemPtr ModuleLoader::wrapWithProject(const ItemPtr &item)
{
    ItemPtr prj = Item::create();
    prj->setChildren(QList<ItemPtr>() << item);
    item->setParent(prj);
    prj->setTypeName("Project");
    prj->setFile(item->file());
    prj->setLocation(item->location());
    return prj;
}

QString ModuleLoader::moduleSubDir(const QStringList &moduleName)
{
#if QT_VERSION >= 0x050000
    return moduleName.join(QLatin1Char('/'));
#else
    return moduleName.join(QLatin1String("/"));
#endif
}

void ModuleLoader::copyProperty(const QString &propertyName, const ItemConstPtr &source,
                                const ItemPtr &destination)
{
    destination->setProperty(propertyName, source->property(propertyName));
}

QString ModuleLoader::fullModuleName(const QStringList &moduleName)
{
    // Currently the same as the module sub directory.
    // ### Might be nicer to be a valid JS identifier.
    return moduleSubDir(moduleName);
}

} // namespace Internal
} // namespace qbs
