/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
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
#include "qualifiedid.h"
#include "scriptengine.h"
#include "value.h"

#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/preferences.h>
#include <tools/profile.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>
#include <tools/settings.h>

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QPair>

namespace qbs {
namespace Internal {

class ModuleLoader::ItemModuleList : public QList<Item::Module> {};

const QString moduleSearchSubDir = QLatin1String("modules");

ModuleLoader::ModuleLoader(ScriptEngine *engine,
                           const Logger &logger)
    : m_engine(engine)
    , m_pool(0)
    , m_logger(logger)
    , m_progressObserver(0)
    , m_reader(new ItemReader(logger))
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

static void addExtraModuleSearchPath(QStringList &list, const QString &searchPath)
{
    list += FileInfo::resolvePath(searchPath, moduleSearchSubDir);
}

void ModuleLoader::setSearchPaths(const QStringList &searchPaths)
{
    m_reader->setSearchPaths(searchPaths);

    m_moduleDirListCache.clear();
    m_moduleSearchPaths.clear();
    foreach (const QString &path, searchPaths)
        addExtraModuleSearchPath(m_moduleSearchPaths, path);

    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << "[MODLDR] module search paths:";
        foreach (const QString &path, m_moduleSearchPaths)
            m_logger.qbsTrace() << "    " << path;
    }
}

ModuleLoaderResult ModuleLoader::load(const SetupProjectParameters &parameters)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] load" << parameters.projectFilePath();
    m_parameters = parameters;
    m_validItemPropertyNamesPerItem.clear();
    m_productModuleCache.clear();
    m_modulePrototypeItemCache.clear();
    m_disabledItems.clear();
    m_reader->clearExtraSearchPathsStack();

    ModuleLoaderResult result;
    m_pool = result.itemPool.data();
    m_reader->setPool(m_pool);

    Item *root = m_reader->readFile(parameters.projectFilePath());
    if (!root)
        return ModuleLoaderResult();

    if (root->typeName() != QLatin1String("Project"))
        root = wrapWithProject(root);

    const QString buildDirectory = TopLevelProject::deriveBuildDirectory(parameters.buildRoot(),
            TopLevelProject::deriveId(parameters.topLevelProfile(),
                                      parameters.finalBuildConfigurationTree()));
    root->setProperty(QLatin1String("sourceDirectory"),
                      VariantValue::create(QFileInfo(root->file()->filePath()).absolutePath()));
    root->setProperty(QLatin1String("buildDirectory"), VariantValue::create(buildDirectory));
    root->setProperty(QLatin1String("profile"),
                      VariantValue::create(m_parameters.topLevelProfile()));
    handleTopLevelProject(&result, root, buildDirectory,
                  QSet<QString>() << QDir::cleanPath(parameters.projectFilePath()));
    result.root = root;
    result.qbsFiles = m_reader->filesRead();
    return result;
}

static void handlePropertyError(const ErrorInfo &error, const SetupProjectParameters &params,
                                Logger logger)
{
    if (params.propertyCheckingMode() == SetupProjectParameters::PropertyCheckingStrict)
        throw error;
    logger.printWarning(error);
}

class PropertyDeclarationCheck : public ValueHandler
{
    const QHash<Item *, QSet<QString> > &m_validItemPropertyNamesPerItem;
    const QSet<Item *> &m_disabledItems;
    Item *m_parentItem;
    QString m_currentName;
    SetupProjectParameters m_params;
    Logger m_logger;
public:
    PropertyDeclarationCheck(const QHash<Item *, QSet<QString> > &validItemPropertyNamesPerItem,
          const QSet<Item *> &disabledItems, const SetupProjectParameters &params,
          const Logger &logger)
        : m_validItemPropertyNamesPerItem(validItemPropertyNamesPerItem)
        , m_disabledItems(disabledItems)
        , m_parentItem(0)
        , m_params(params)
        , m_logger(logger)
    {
    }

    void operator()(Item *item)
    {
        handleItem(item);
    }

private:
    void handle(JSSourceValue *value)
    {
        if (!m_parentItem->propertyDeclaration(m_currentName).isValid()
                && !value->createdByPropertiesBlock()) {
            const ErrorInfo error(Tr::tr("Property '%1' is not declared.")
                            .arg(m_currentName), value->location());
            handlePropertyError(error, m_params, m_logger);
        }
    }

    void handle(ItemValue *value)
    {
        if (!value->item()->isModuleInstance()
                && !m_validItemPropertyNamesPerItem.value(m_parentItem).contains(m_currentName)
                && m_parentItem->file()
                && !m_parentItem->file()->idScope()->hasProperty(m_currentName)
                && !value->createdByPropertiesBlock()) {
            const ErrorInfo error(Tr::tr("Item '%1' is not declared. "
                                         "Did you forget to add a Depends item?").arg(m_currentName),
                                  value->location().isValid() ? value->location()
                                                              : m_parentItem->location());
            handlePropertyError(error, m_params, m_logger);
        } else {
            handleItem(value->item());
        }
    }

    void handleItem(Item *item)
    {
        if (m_disabledItems.contains(item)
                // The Properties child of a SubProject item is not a regular item.
                || item->typeName() == QLatin1String("Properties")) {
            return;
        }

        Item *oldParentItem = m_parentItem;
        for (Item::PropertyMap::const_iterator it = item->properties().constBegin();
                it != item->properties().constEnd(); ++it) {
            if (item->propertyDeclaration(it.key()).isValid())
                continue;
            m_currentName = it.key();
            m_parentItem = item;
            it.value()->apply(this);
        }
        m_parentItem = oldParentItem;
        foreach (Item *child, item->children()) {
            if (child->typeName() != QLatin1String("Export"))
                handleItem(child);
        }

        // Properties that don't refer to an existing module with a matching Depends item
        // only exist in the prototype, not in the instance.
        // Example 1 - setting a property of an unknown module: Export { abc.def: true }
        // Example 2 - setting a non-existing Export property: Export { blubb: true }
        if (item->typeName() == QLatin1String("Export") && item->prototype())
            handleItem(item->prototype());
    }

    void handle(VariantValue *) { /* only created internally - no need to check */ }
    void handle(BuiltinValue *) { /* only created internally - no need to check */ }
};

void ModuleLoader::handleTopLevelProject(ModuleLoaderResult *loadResult, Item *item,
        const QString &buildDirectory, const QSet<QString> &referencedFilePaths)
{
    TopLevelProjectContext tlp;
    handleProject(loadResult, &tlp, item, buildDirectory, referencedFilePaths);

    foreach (ProjectContext *projectContext, tlp.projects) {
        m_reader->setExtraSearchPathsStack(projectContext->searchPathsStack);
        for (auto it = projectContext->products.begin(); it != projectContext->products.end(); ++it)
            handleProduct(it);
    }

    m_reader->clearExtraSearchPathsStack();
    checkItemTypes(item);
    PropertyDeclarationCheck check(m_validItemPropertyNamesPerItem, m_disabledItems, m_parameters,
                                   m_logger);
    check(item);
}

void ModuleLoader::handleProject(ModuleLoaderResult *loadResult,
        TopLevelProjectContext *topLevelProjectContext, Item *item, const QString &buildDirectory,
        const QSet<QString> &referencedFilePaths)
{
    auto *p = new ProjectContext;
    auto &projectContext = *p;
    projectContext.topLevelProject = topLevelProjectContext;
    projectContext.result = loadResult;
    projectContext.buildDirectory = buildDirectory;
    ProductContext dummyProductContext;
    dummyProductContext.project = &projectContext;
    dummyProductContext.moduleProperties = m_parameters.finalBuildConfigurationTree();
    loadBaseModule(&dummyProductContext, item);
    if (!checkItemCondition(item)) {
        delete p;
        return;
    }
    topLevelProjectContext->projects << &projectContext;
    overrideItemProperties(item, QLatin1String("project"), m_parameters.overriddenValuesTree());
    const QString projectName = m_evaluator->stringValue(item, QLatin1String("name"));
    if (!projectName.isEmpty())
        overrideItemProperties(item, projectName, m_parameters.overriddenValuesTree());
    m_reader->pushExtraSearchPaths(readExtraSearchPaths(item) << item->file()->dirPath());
    projectContext.searchPathsStack = m_reader->extraSearchPathsStack();
    projectContext.item = item;
    ItemValuePtr itemValue = ItemValue::create(item);
    projectContext.scope = Item::create(m_pool);
    projectContext.scope->setFile(item->file());
    projectContext.scope->setProperty(QLatin1String("project"), itemValue);

    const QString minVersionStr
            = m_evaluator->stringValue(item, QLatin1String("minimumQbsVersion"),
                                       QLatin1String("1.3.0"));
    const Version minVersion = Version::fromString(minVersionStr);
    if (!minVersion.isValid()) {
        throw ErrorInfo(Tr::tr("The value '%1' of Project.minimumQbsVersion "
                "is not a valid version string.").arg(minVersionStr), item->location());
    }
    if (!m_qbsVersion.isValid())
        m_qbsVersion = Version::fromString(QLatin1String(QBS_VERSION));
    if (m_qbsVersion < minVersion) {
        throw ErrorInfo(Tr::tr("The project requires at least qbs version %1, but "
                               "this is qbs version %2.").arg(minVersion.toString(),
                                                              m_qbsVersion.toString()));
    }

    foreach (Item *child, item->children()) {
        child->setScope(projectContext.scope);
        if (child->typeName() == QLatin1String("Product")) {
            foreach (Item * const additionalProductItem,
                     multiplexProductItem(&dummyProductContext, child)) {
                Item::addChild(item, additionalProductItem);
            }
        }
    }

    foreach (Item *child, item->children()) {
        if (child->typeName() == QLatin1String("Product")) {
            prepareProduct(&projectContext, child);
        } else if (child->typeName() == QLatin1String("SubProject")) {
            handleSubProject(&projectContext, child, referencedFilePaths);
        } else if (child->typeName() == QLatin1String("Project")) {
            copyProperties(item, child);
            handleProject(loadResult, topLevelProjectContext, child, buildDirectory,
                          referencedFilePaths);
        }
    }

    const QString projectFileDirPath = FileInfo::path(item->file()->filePath());
    const QStringList refs = m_evaluator->stringListValue(item, QLatin1String("references"));
    typedef QPair<Item *, QString> ItemAndRefPath;
    QList<ItemAndRefPath> additionalProjectChildren;
    foreach (const QString &filePath, refs) {
        QString absReferencePath = FileInfo::resolvePath(projectFileDirPath, filePath);
        if (FileInfo(absReferencePath).isDir()) {
            QString qbsFilePath;
            QDirIterator dit(absReferencePath, QStringList(QLatin1String("*.qbs")));
            while (dit.hasNext()) {
                if (!qbsFilePath.isEmpty()) {
                    throw ErrorInfo(Tr::tr("Referenced directory '%1' contains more than one "
                                           "qbs file.").arg(absReferencePath),
                                    item->property(QLatin1String("references"))->location());
                }
                qbsFilePath = dit.next();
            }
            if (qbsFilePath.isEmpty()) {
                throw ErrorInfo(Tr::tr("Referenced directory '%1' does not contain a qbs file.")
                                .arg(absReferencePath),
                                item->property(QLatin1String("references"))->location());
            }
            absReferencePath = qbsFilePath;
        }
        if (referencedFilePaths.contains(absReferencePath))
            throw ErrorInfo(Tr::tr("Cycle detected while referencing file '%1'.").arg(filePath),
                            item->property(QLatin1String("references"))->location());
        Item *subItem = m_reader->readFile(absReferencePath);
        subItem->setScope(projectContext.scope);
        subItem->setParent(projectContext.item);
        additionalProjectChildren << qMakePair(subItem, absReferencePath);
        if (subItem->typeName() == QLatin1String("Product")) {
            foreach (Item * const additionalProductItem,
                     multiplexProductItem(&dummyProductContext, subItem)) {
                additionalProjectChildren << qMakePair(additionalProductItem, absReferencePath);
            }
        }
    }
    foreach (const ItemAndRefPath &irp, additionalProjectChildren) {
        Item * const subItem = irp.first;
        Item::addChild(projectContext.item, subItem);
        if (subItem->typeName() == QLatin1String("Product")) {
            prepareProduct(&projectContext, subItem);
        } else if (subItem->typeName() == QLatin1String("Project")) {
            copyProperties(item, subItem);
            handleProject(loadResult, topLevelProjectContext, subItem, buildDirectory,
                          QSet<QString>(referencedFilePaths) << irp.second);
        } else {
            throw ErrorInfo(Tr::tr("The top-level item of a file in a \"references\" list must be "
                               "a Product or a Project, but it is \"%1\".").arg(subItem->typeName()),
                        subItem->location());
        }
    }
    m_reader->popExtraSearchPaths();
}

QList<Item *> ModuleLoader::multiplexProductItem(ProductContext *dummyContext, Item *productItem)
{
    // Temporarily attach the qbs module here, in case we need to access one of its properties
    // to evaluate the profiles property.
    const QString qbsKey = QLatin1String("qbs");
    ValuePtr qbsValue = productItem->property(qbsKey); // Retrieve now to restore later.
    if (qbsValue)
        qbsValue = qbsValue->clone();
    loadBaseModule(dummyContext, productItem);

    // Overriding the product item properties must be done here already, because otherwise
    // the "profiles" property would not be overridable.
    QString productName = m_evaluator->stringValue(productItem, QLatin1String("name"));
    if (productName.isEmpty()) {
        productName = FileInfo::completeBaseName(productItem->file()->filePath());
        productItem->setProperty(QLatin1String("name"), VariantValue::create(productName));
    }
    overrideItemProperties(productItem, productName, m_parameters.overriddenValuesTree());

    const QString profilesKey = QLatin1String("profiles");
    const ValueConstPtr profilesValue = productItem->property(profilesKey);
    QBS_CHECK(profilesValue); // Default value set in BuiltinDeclarations.
    const QStringList profileNames = m_evaluator->stringListValue(productItem, profilesKey);
    if (profileNames.isEmpty()) {
        throw ErrorInfo(Tr::tr("The 'profiles' property cannot be an empty list."),
                        profilesValue->location());
    }
    foreach (const QString &profileName, profileNames) {
        if (profileNames.count(profileName) > 1) {
            throw ErrorInfo(Tr::tr("The profile '%1' appears in the 'profiles' list twice, "
                    "which is not allowed.").arg(profileName), profilesValue->location());
        }
    }

    // "Unload" the qbs module again.
    if (qbsValue)
        productItem->setProperty(qbsKey, qbsValue);
    else
        productItem->removeProperty(qbsKey);
    productItem->removeModules();
    m_validItemPropertyNamesPerItem[productItem].clear();

    QList<Item *> additionalProductItems;
    const QString profileKey = QLatin1String("profile");
    productItem->setProperty(profileKey, VariantValue::create(profileNames.first()));
    Settings settings(m_parameters.settingsDirectory());
    for (int i = 0; i < profileNames.count(); ++i) {
        Profile profile(profileNames.at(i), &settings);
        if (!profile.exists()) {
            throw ErrorInfo(Tr::tr("The profile '%1' does not exist.").arg(profile.name()),
                            productItem->location()); // TODO: profilesValue->location() is invalid, why?
        }
        if (i == 0)
            continue; // We use the original item for the first profile.
        Item * const cloned = productItem->clone();
        cloned->setProperty(profileKey, VariantValue::create(profileNames.at(i)));
        additionalProductItems << cloned;
    }
    return additionalProductItems;
}

void ModuleLoader::prepareProduct(ProjectContext *projectContext, Item *item)
{
    checkCancelation();
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] prepareProduct " << item->file()->filePath();

    initProductProperties(projectContext, item);
    ProductContext productContext;
    productContext.name = m_evaluator->stringValue(item, QLatin1String("name"));
    QBS_CHECK(!productContext.name.isEmpty());
    bool profilePropertySet;
    productContext.profileName = m_evaluator->stringValue(item, QLatin1String("profile"),
                                                         QString(), &profilePropertySet);
    QBS_CHECK(profilePropertySet);
    const QVariantMap::ConstIterator it
            = projectContext->result->profileConfigs.find(productContext.profileName);
    if (it == projectContext->result->profileConfigs.constEnd()) {
        const QVariantMap buildConfig = SetupProjectParameters::expandedBuildConfiguration(
                    m_parameters.settingsDirectory(), productContext.profileName,
                    m_parameters.buildVariant());
        productContext.moduleProperties = SetupProjectParameters::finalBuildConfigurationTree(
                    buildConfig, m_parameters.overriddenValues(), m_parameters.buildRoot(),
                    m_parameters.topLevelProfile());
        projectContext->result->profileConfigs.insert(productContext.profileName,
                                                      productContext.moduleProperties);
    } else {
        productContext.moduleProperties = it.value().toMap();
    }
    productContext.item = item;
    productContext.project = projectContext;

    QVector<Item *> exportItems;
    foreach (Item *child, item->children()) {
        if (child->typeName() == QLatin1String("Export"))
            exportItems << child;
    }

    Item *mergedExportItem = mergeExportItems(&productContext, exportItems);
    ProductModuleInfo &pmi = projectContext->topLevelProject->productModules[productContext.name];
    pmi.exportItem = mergedExportItem;

    ItemValuePtr itemValue = ItemValue::create(item);
    productContext.scope = Item::create(m_pool);
    productContext.scope->setProperty(QLatin1String("product"), itemValue);
    productContext.scope->setFile(item->file());
    productContext.scope->setScope(productContext.project->scope);
    setScopeForDescendants(item, productContext.scope);

    projectContext->products << productContext;
}

void ModuleLoader::handleProduct(ProductContext *productContext)
{
    checkCancelation();
    Item *item = productContext->item;
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] handleProduct " << item->file()->filePath();

    QStringList extraSearchPaths = readExtraSearchPaths(item);
    Settings settings(m_parameters.settingsDirectory());
    const QStringList prefsSearchPaths
            = Preferences(&settings, productContext->profileName).searchPaths();
    foreach (const QString &p, prefsSearchPaths) {
        if (!m_moduleSearchPaths.contains(p) && FileInfo(p).exists())
            extraSearchPaths << p;
    }
    m_reader->pushExtraSearchPaths(extraSearchPaths);

    DependsContext dependsContext;
    dependsContext.product = productContext;
    dependsContext.productDependencies = &productContext->info.usedProducts;
    resolveDependencies(&dependsContext, item);
    addTransitiveDependencies(productContext, productContext->item);
    copyGroupsFromModulesToProduct(productContext->item);
    checkItemCondition(item);

    foreach (Item *child, item->children()) {
        if (child->typeName() == QLatin1String("Group"))
            handleGroup(productContext, child);
        else if (child->typeName() == QLatin1String("Probe"))
            resolveProbe(item, child);
    }

    productContext->project->result->productInfos.insert(item, productContext->info);
    m_reader->popExtraSearchPaths();
}

void ModuleLoader::initProductProperties(const ProjectContext *project, Item *item)
{
    const QString productName = m_evaluator->stringValue(item, QLatin1String("name"));
    const QString profile = m_evaluator->stringValue(item, QLatin1String("profile"));
    QBS_CHECK(!profile.isEmpty());
    const QString buildDir = ResolvedProduct::deriveBuildDirectoryName(productName, profile);
    item->setProperty(QLatin1String("buildDirectory"),
                      VariantValue::create(
                          FileInfo::resolvePath(project->buildDirectory, buildDir)));

    item->setProperty(QLatin1String("sourceDirectory"),
                      VariantValue::create(
                          QFileInfo(item->file()->filePath()).absolutePath()));
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
    const QString relativeFilePath = m_evaluator->stringValue(item, QLatin1String("filePath"));
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
    handleProject(projectContext->result, projectContext->topLevelProject, loadedItem,
                  projectContext->buildDirectory,
                  QSet<QString>(referencedFilePaths) << subProjectFilePath);
}

void ModuleLoader::handleGroup(ProductContext *productContext, Item *item)
{
    checkCancelation();
    propagateModulesFromProduct(productContext, item);
    checkItemCondition(item);
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

Item *ModuleLoader::mergeExportItems(ModuleLoader::ProductContext *productContext,
        const QVector<Item *> &exportItemsInProduct)
{
    Item *merged = Item::create(productContext->item->pool());
    merged->setTypeName(QLatin1String("Export"));
    merged->setFile(productContext->item->file());
    merged->setLocation(productContext->item->location());
    QSet<Item *> exportItems;
    foreach (Item *exportItem, exportItemsInProduct) {
        checkCancelation();
        if (Q_UNLIKELY(productContext->filesWithExportItem.contains(exportItem->file())))
            throw ErrorInfo(Tr::tr("Multiple Export items in one product are prohibited."),
                        exportItem->location());
        merged->setFile(exportItem->file());
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
    merged->setupForBuiltinType(m_logger);
    return merged;
}

void ModuleLoader::propagateModulesFromProduct(ProductContext *productContext, Item *item)
{
    for (Item::Modules::const_iterator it = productContext->item->modules().constBegin();
         it != productContext->item->modules().constEnd(); ++it)
    {
        Item::Module m = *it;
        Item *targetItem = moduleInstanceItem(item, m.name);
        targetItem->setPrototype(m.item);
        targetItem->setModuleInstanceFlag(true);
        targetItem->setScope(m.item->scope());
        targetItem->setModules(m.item->modules());

        // "parent" should point to the group/artifact parent
        targetItem->setParent(item->parent());

        // the outer item of a module is the product's instance of it
        targetItem->setOuterItem(m.item);   // ### Is this always the same as the scope item?

        m.item = targetItem;
        item->addModule(m);
    }
}

void ModuleLoader::resolveDependencies(DependsContext *dependsContext, Item *item)
{
    loadBaseModule(dependsContext->product, item);

    // Resolve all Depends items.
    ItemModuleList loadedModules;
    ProductDependencyResults productDependencies;
    foreach (Item *child, item->children())
        if (child->typeName() == QLatin1String("Depends"))
            resolveDependsItem(dependsContext, item, child, &loadedModules, &productDependencies);

    QSet<QString> loadedModuleNames;
    foreach (const Item::Module &module, loadedModules) {
        const QString fullName = module.name.toString();
        if (loadedModuleNames.contains(fullName))
            continue;
        loadedModuleNames.insert(fullName);
        item->addModule(module);
    }

    dependsContext->productDependencies->append(productDependencies);
}

void ModuleLoader::resolveDependsItem(DependsContext *dependsContext, Item *item,
        Item *dependsItem, ItemModuleList *moduleResults,
        ProductDependencyResults *productResults)
{
    checkCancelation();
    if (!checkItemCondition(dependsItem)) {
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "Depends item disabled, ignoring.";
        return;
    }
    bool productTypesIsSet;
    const QStringList productTypes = m_evaluator->stringListValue(dependsItem,
            QLatin1String("productTypes"), &productTypesIsSet);
    bool nameIsSet;
    const QString name
            = m_evaluator->stringValue(dependsItem, QLatin1String("name"), QString(), &nameIsSet);
    bool submodulesPropertySet;
    QStringList submodules = m_evaluator->stringListValue(dependsItem, QLatin1String("submodules"),
                                                          &submodulesPropertySet);
    if (productTypesIsSet) {
        if (nameIsSet) {
            throw ErrorInfo(Tr::tr("The 'productTypes' and 'name' properties are mutually "
                                   "exclusive."), dependsItem->location());
        }
        if (submodulesPropertySet) {
            throw ErrorInfo(Tr::tr("The 'productTypes' and 'subModules' properties are mutually "
                                   "exclusive."), dependsItem->location());
        }
        if (productTypes.isEmpty()) {
            m_logger.qbsTrace() << "Ignoring Depends item with empty productTypes list.";
            return;
        }

        // TODO: We could also filter by the "profiles" property. This would required a refactoring
        //       (Dependency needs a list of profiles and the multiplexing must happen later).
        ModuleLoaderResult::ProductInfo::Dependency dependency;
        dependency.productTypes = productTypes;
        dependency.limitToSubProject
                = m_evaluator->boolValue(dependsItem, QLatin1String("limitToSubProject"));
        productResults->append(dependency);
        return;
    }
    if (submodules.isEmpty() && submodulesPropertySet) {
        m_logger.qbsTrace() << "Ignoring Depends item with empty submodules list.";
        return;
    }
    if (Q_UNLIKELY(submodules.count() > 1 && !dependsItem->id().isEmpty())) {
        QString msg = Tr::tr("A Depends item with more than one module cannot have an id.");
        throw ErrorInfo(msg, dependsItem->location());
    }

    QList<QualifiedId> moduleNames;
    const QualifiedId nameParts = QualifiedId::fromString(name);
    if (submodules.isEmpty()) {
        moduleNames << nameParts;
    } else {
        foreach (const QString &submodule, submodules)
            moduleNames << nameParts + QualifiedId::fromString(submodule);
    }

    Item::Module result;
    foreach (const QualifiedId &moduleName, moduleNames) {
        const bool isRequired
                = m_evaluator->boolValue(dependsItem, QLatin1String("required"));
        Item *moduleItem = loadModule(dependsContext->product, item, dependsItem->location(),
                                      dependsItem->id(), moduleName, false, isRequired,
                                      &result.isProduct);
        if (!moduleItem) {
            // ### 1.5: change error message to the more generic "Dependency '%1' not found.".
            throw ErrorInfo(Tr::tr("Product dependency '%1' not found.").arg(moduleName.toString()),
                            dependsItem->location());
        }
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "module loaded: " << moduleName.toString();
        result.name = moduleName;
        result.item = moduleItem;
        result.required = isRequired;
        moduleResults->append(result);
        if (result.isProduct) {
            if (m_logger.traceEnabled())
                m_logger.qbsTrace() << "product dependency loaded: " << moduleName.toString();
            const QString profilesKey = QLatin1String("profiles");
            const QStringList profiles = m_evaluator->stringListValue(dependsItem, profilesKey);
            if (profiles.isEmpty()) {
                ModuleLoaderResult::ProductInfo::Dependency dependency;
                dependency.name = moduleName.toString();
                dependency.profile = QLatin1String("*");
                productResults->append(dependency);
                continue;
            }
            foreach (const QString &profile, profiles) {
                ModuleLoaderResult::ProductInfo::Dependency dependency;
                dependency.name = moduleName.toString();
                dependency.profile = profile;
                productResults->append(dependency);
            }
        }
    }
}

Item *ModuleLoader::moduleInstanceItem(Item *item, const QualifiedId &moduleName)
{
    Item *instance = item;
    for (int i = 0; i < moduleName.count(); ++i) {
        const QString &moduleNameSegment = moduleName.at(i);
        m_validItemPropertyNamesPerItem[instance].insert(moduleNameSegment);
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
            instance->setProperty(moduleNameSegment, ItemValue::create(newItem));
            instance = newItem;
        }
    }
    QBS_ASSERT(moduleName.isEmpty() || instance != item, return 0);
    return instance;
}

Item *ModuleLoader::loadProductModule(ModuleLoader::ProductContext *productContext,
        const QString &moduleName)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] loadProductModule name: " << moduleName;
    Item *module = m_productModuleCache.value(moduleName);
    if (module) {
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "[MODLDR] loadProductModule cache hit.";
        return module;
    }
    ProductModuleInfo &pmi = productContext->project->topLevelProject->productModules[moduleName];
    module = pmi.exportItem;
    if (module) {
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "[MODLDR] loadProductModule cache miss.";
        DependsContext dependsContext;
        dependsContext.product = productContext;
        dependsContext.productDependencies = &pmi.productDependencies;
        resolveDependencies(&dependsContext, module);
        m_productModuleCache.insert(moduleName, module);
    }
    return module;
}

Item *ModuleLoader::loadModule(ProductContext *productContext, Item *item,
        const CodeLocation &dependsItemLocation,
        const QString &moduleId, const QualifiedId &moduleName, bool isBaseModule, bool isRequired,
        bool *isProductDependency)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] loadModule name: " << moduleName << ", id: " << moduleId;

    Item *moduleInstance = moduleId.isEmpty()
            ? moduleInstanceItem(item, moduleName)
            : moduleInstanceItem(item, QStringList(moduleId));
    if (!moduleInstance->typeName().isNull()) {
        // already handled
        return moduleInstance;
    }

    *isProductDependency = true;
    Item *modulePrototype = loadProductModule(productContext, moduleName.toString());
    if (!modulePrototype) {
        *isProductDependency = false;
        QStringList moduleSearchPaths;
        foreach (const QString &searchPath, m_reader->searchPaths())
            addExtraModuleSearchPath(moduleSearchPaths, searchPath);
        bool cacheHit;
        modulePrototype = searchAndLoadModuleFile(productContext, dependsItemLocation,
                moduleName, moduleSearchPaths, isRequired, &cacheHit);
        if (isBaseModule && modulePrototype && !cacheHit)
            setupBaseModulePrototype(modulePrototype);
    }
    if (!modulePrototype)
        return 0;
    instantiateModule(productContext, nullptr, item, moduleInstance, modulePrototype,
                      moduleName, *isProductDependency);
    if (moduleInstance->isPresentModule()) {
        resolveProbes(moduleInstance);
        try {
            m_evaluator->boolValue(moduleInstance, QLatin1String("validate"));
        } catch (const ErrorInfo &error) {
            if (isRequired) { // Error will be thrown for enabled products only
                moduleInstance->setDelayedError(error);
            } else {
                return createNonPresentModule(moduleName.toString(),
                                              QLatin1String("failed validation"), moduleInstance);
            }
        }
    }
    return moduleInstance;
}

Item *ModuleLoader::searchAndLoadModuleFile(ProductContext *productContext,
        const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
        const QStringList &extraSearchPaths, bool isRequired, bool *cacheHit)
{
    QStringList searchPaths = extraSearchPaths;
    searchPaths.append(m_moduleSearchPaths);

    bool triedToLoadModule = false;
    const QString fullName = moduleName.toString();
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
                                            filePath, cacheHit, &triedToLoadModule);
            if (module)
                return module;
            if (!triedToLoadModule)
                m_moduleDirListCache[dirPath].removeOne(filePath);
        }
    }

    if (!isRequired)
        return createNonPresentModule(fullName, QLatin1String("not found"), nullptr);

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
                        .arg(v.toString(), name.join(QLatin1Char('.'))));
    }
    return c;
}

Item *ModuleLoader::loadModuleFile(ProductContext *productContext, const QString &fullModuleName,
        bool isBaseModule, const QString &filePath, bool *cacheHit, bool *triedToLoad)
{
    checkCancelation();

    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] trying to load " << fullModuleName << " from " << filePath;

    const ModuleItemCache::key_type cacheKey(filePath, productContext->profileName);
    const ItemCacheValue cacheValue = m_modulePrototypeItemCache.value(cacheKey);
    if (cacheValue.module) {
        m_logger.qbsTrace() << "[LDR] loadModuleFile cache hit for " << filePath;
        *cacheHit = true;
        return cacheValue.enabled ? cacheValue.module : 0;
    }
    *cacheHit = false;
    Item * const module = m_reader->readFile(filePath);
    if (module->typeName() != QLatin1String("Module")) {
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << "[MODLDR] Alleged module " << fullModuleName << " has type '"
                                << module->typeName() << "', so it's not a module after all.";
        }
        *triedToLoad = false;
        return 0;
    }
    if (!isBaseModule) {
        DependsContext dependsContext;
        dependsContext.product = productContext;
        dependsContext.productDependencies = &productContext->info.usedProducts;
        resolveDependencies(&dependsContext, module);
    }

    // Module properties that are defined in the profile are used as default values.
    const QVariantMap profileModuleProperties
            = productContext->moduleProperties.value(fullModuleName).toMap();
    QList<ErrorInfo> unknownProfilePropertyErrors;
    for (QVariantMap::const_iterator vmit = profileModuleProperties.begin();
            vmit != profileModuleProperties.end(); ++vmit)
    {
        if (Q_UNLIKELY(!module->hasProperty(vmit.key()))) {
            const ErrorInfo error(Tr::tr("Unknown property: %1.%2").arg(fullModuleName,
                                                                        vmit.key()));
            unknownProfilePropertyErrors.append(error);
            continue;
        }
        const PropertyDeclaration decl = module->propertyDeclaration(vmit.key());
        VariantValuePtr v = VariantValue::create(convertToPropertyType(vmit.value(), decl.type(),
                QStringList(fullModuleName), vmit.key()));
        module->setProperty(vmit.key(), v);
    }

    // Check the condition last in case the condition needs to evaluate other properties that were
    // set by the profile
    if (!checkItemCondition(module)) {
        m_logger.qbsTrace() << "[LDR] module condition is false";
        m_modulePrototypeItemCache.insert(cacheKey, ItemCacheValue(module, false));
        return 0;
    }

    foreach (const ErrorInfo &error, unknownProfilePropertyErrors)
        handlePropertyError(error, m_parameters, m_logger);

    module->setProperty(QLatin1String("name"), VariantValue::create(fullModuleName));
    m_modulePrototypeItemCache.insert(cacheKey, ItemCacheValue(module, true));
    return module;
}

void ModuleLoader::loadBaseModule(ProductContext *productContext, Item *item)
{
    const QualifiedId baseModuleName(QLatin1String("qbs"));
    Item::Module baseModuleDesc;
    baseModuleDesc.name = baseModuleName;
    baseModuleDesc.item = loadModule(productContext, item, CodeLocation(), QString(),
                                     baseModuleName, true, true, &baseModuleDesc.isProduct);
    QBS_CHECK(!baseModuleDesc.isProduct);
    if (Q_UNLIKELY(!baseModuleDesc.item))
        throw ErrorInfo(Tr::tr("Cannot load base qbs module."));
    item->addModule(baseModuleDesc);
}

static QStringList hostOS()
{
    QStringList hostSystem;

#if defined(Q_OS_AIX)
    hostSystem << QLatin1String("aix");
#endif
#if defined(Q_OS_ANDROID)
    hostSystem << QLatin1String("android");
#endif
#if defined(Q_OS_BLACKBERRY)
    hostSystem << QLatin1String("blackberry");
#endif
#if defined(Q_OS_BSD4)
    hostSystem << QLatin1String("bsd") << QLatin1String("bsd4");
#endif
#if defined(Q_OS_BSDI)
    hostSystem << QLatin1String("bsdi");
#endif
#if defined(Q_OS_CYGWIN)
    hostSystem << QLatin1String("cygwin");
#endif
#if defined(Q_OS_DARWIN)
    hostSystem << QLatin1String("darwin");
#endif
#if defined(Q_OS_DGUX)
    hostSystem << QLatin1String("dgux");
#endif
#if defined(Q_OS_DYNIX)
    hostSystem << QLatin1String("dynix");
#endif
#if defined(Q_OS_FREEBSD)
    hostSystem << QLatin1String("freebsd");
#endif
#if defined(Q_OS_HPUX)
    hostSystem << QLatin1String("hpux");
#endif
#if defined(Q_OS_HURD)
    hostSystem << QLatin1String("hurd");
#endif
#if defined(Q_OS_INTEGRITY)
    hostSystem << QLatin1String("integrity");
#endif
#if defined(Q_OS_IOS)
    hostSystem << QLatin1String("ios");
#endif
#if defined(Q_OS_IRIX)
    hostSystem << QLatin1String("irix");
#endif
#if defined(Q_OS_LINUX)
    hostSystem << QLatin1String("linux");
#endif
#if defined(Q_OS_LYNX)
    hostSystem << QLatin1String("lynx");
#endif
#if defined(Q_OS_OSX)
    hostSystem << QLatin1String("osx");
#endif
#if defined(Q_OS_MSDOS)
    hostSystem << QLatin1String("msdos");
#endif
#if defined(Q_OS_NACL)
    hostSystem << QLatin1String("nacl");
#endif
#if defined(Q_OS_NETBSD)
    hostSystem << QLatin1String("netbsd");
#endif
#if defined(Q_OS_OPENBSD)
    hostSystem << QLatin1String("openbsd");
#endif
#if defined(Q_OS_OS2)
    hostSystem << QLatin1String("os2");
#endif
#if defined(Q_OS_OS2EMX)
    hostSystem << QLatin1String("os2emx");
#endif
#if defined(Q_OS_OSF)
    hostSystem << QLatin1String("osf");
#endif
#if defined(Q_OS_QNX)
    hostSystem << QLatin1String("qnx");
#endif
#if defined(Q_OS_ONX6)
    hostSystem << QLatin1String("qnx6");
#endif
#if defined(Q_OS_RELIANT)
    hostSystem << QLatin1String("reliant");
#endif
#if defined(Q_OS_SCO)
    hostSystem << QLatin1String("sco");
#endif
#if defined(Q_OS_SOLARIS)
    hostSystem << QLatin1String("solaris");
#endif
#if defined(Q_OS_SYMBIAN)
    hostSystem << QLatin1String("symbian");
#endif
#if defined(Q_OS_ULTRIX)
    hostSystem << QLatin1String("ultrix");
#endif
#if defined(Q_OS_UNIX)
    hostSystem << QLatin1String("unix");
#endif
#if defined(Q_OS_UNIXWARE)
    hostSystem << QLatin1String("unixware");
#endif
#if defined(Q_OS_VXWORKS)
    hostSystem << QLatin1String("vxworks");
#endif
#if defined(Q_OS_WIN32)
    hostSystem << QLatin1String("windows");
#endif
#if defined(Q_OS_WINCE)
    hostSystem << QLatin1String("windowsce");
#endif
#if defined(Q_OS_WINPHONE)
    hostSystem << QLatin1String("windowsphone");
#endif
#if defined(Q_OS_WINRT)
    hostSystem << QLatin1String("winrt");
#endif

    return hostSystem;
}

void ModuleLoader::setupBaseModulePrototype(Item *prototype)
{
    prototype->setProperty(QLatin1String("getNativeSetting"),
                           BuiltinValue::create(BuiltinValue::GetNativeSettingFunction));
    prototype->setProperty(QLatin1String("getEnv"),
                           BuiltinValue::create(BuiltinValue::GetEnvFunction));
    prototype->setProperty(QLatin1String("currentEnv"),
                           BuiltinValue::create(BuiltinValue::CurrentEnvFunction));
    prototype->setProperty(QLatin1String("hostOS"), VariantValue::create(hostOS()));
    prototype->setProperty(QLatin1String("canonicalArchitecture"),
                           BuiltinValue::create(BuiltinValue::CanonicalArchitectureFunction));
    prototype->setProperty(QLatin1String("rfc1034Identifier"),
                           BuiltinValue::create(BuiltinValue::Rfc1034IdentifierFunction));
    prototype->setProperty(QLatin1String("libexecPath"),
                           VariantValue::create(m_parameters.libexecPath()));

    const Version qbsVersion = Version::qbsVersion();
    prototype->setProperty(QLatin1String("versionMajor"),
                           VariantValue::create(qbsVersion.majorVersion()));
    prototype->setProperty(QLatin1String("versionMinor"),
                           VariantValue::create(qbsVersion.minorVersion()));
    prototype->setProperty(QLatin1String("versionPatch"),
                           VariantValue::create(qbsVersion.patchLevel()));
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

static Item *productOf(Item *item)
{
    do {
        item = item->parent();
    } while (item && item->typeName() != QLatin1String("Product"));
    return item;
}

static void copyNonOverriddenProperties(const Item *src, Item *dst)
{
    const Item::PropertyMap &srcProps = src->properties();
    for (auto it = srcProps.constBegin(); it != srcProps.constEnd(); ++it) {
        if (it.value()->type() != Value::JSSourceValueType || dst->hasOwnProperty(it.key()))
            continue;
        dst->setProperty(it.key(), it.value());
    }
}

void ModuleLoader::instantiateModule(ProductContext *productContext, Item *exportingProduct,
        Item *instanceScope, Item *moduleInstance, Item *modulePrototype,
        const QualifiedId &moduleName, bool isProduct)
{
    const QString fullName = moduleName.toString();
    moduleInstance->setPrototype(modulePrototype);
    moduleInstance->setFile(modulePrototype->file());
    moduleInstance->setLocation(modulePrototype->location());
    moduleInstance->setTypeName(modulePrototype->typeName());

    // create module scope
    Item *moduleScope = Item::create(m_pool);
    QBS_CHECK(instanceScope->file());
    moduleScope->setFile(instanceScope->file());
    moduleScope->setScope(instanceScope);
    productContext->project->scope->copyProperty(QLatin1String("project"), moduleScope);
    productContext->scope->copyProperty(QLatin1String("product"), moduleScope);

    if (isProduct) {
        exportingProduct = 0;
        for (Item *exportItem = modulePrototype; exportItem; exportItem = exportItem->prototype()) {
            exportingProduct = productOf(exportItem);
            if (exportingProduct)
                break;
        }
        QBS_CHECK(exportingProduct);
    }

    if (exportingProduct) {
        if (!isProduct) {
            copyNonOverriddenProperties(modulePrototype, moduleInstance);
            moduleInstance->setPrototype(modulePrototype->prototype());
        }

        Item *exportScope =  Item::create(moduleInstance->pool());
        exportScope->setFile(instanceScope->file());
        exportScope->setScope(instanceScope);
        exportScope->setProperty(QLatin1String("product"), ItemValue::create(exportingProduct));
        exportScope->setProperty(QLatin1String("project"),
                                 ItemValue::create(exportingProduct->parent()));

        for (auto it = moduleInstance->properties().begin();
             it != moduleInstance->properties().end(); ++it) {
            if (it.value()->type() != Value::JSSourceValueType)
                continue;
            const JSSourceValuePtr v = it.value().staticCast<JSSourceValue>();
            v->setExportScope(exportScope);
        }

        PropertyDeclaration pd(QLatin1String("_qbs_sourceDir"), PropertyDeclaration::String,
                               PropertyDeclaration::PropertyNotAvailableInConfig);
        moduleInstance->setPropertyDeclaration(pd.name(), pd);
        ValuePtr v = exportingProduct->property(QLatin1String("sourceDirectory"))->clone();
        moduleInstance->setProperty(pd.name(), v);
    }

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
        QBS_ASSERT(depinst != m.item, continue);
        instantiateModule(productContext, isProduct ? exportingProduct : nullptr, moduleInstance,
                          depinst, m.item, m.name, m.isProduct);
        m.item = depinst;
        moduleInstance->addModule(m);
    }

    // override module properties given on the command line
    const QVariantMap userModuleProperties
            = m_parameters.overriddenValuesTree().value(fullName).toMap();
    for (QVariantMap::const_iterator vmit = userModuleProperties.begin();
         vmit != userModuleProperties.end(); ++vmit) {
        if (Q_UNLIKELY(!moduleInstance->hasProperty(vmit.key()))) {
            const ErrorInfo error(Tr::tr("Unknown property: %1.%2")
                                  .arg(moduleName.toString(), vmit.key()));
            handlePropertyError(error, m_parameters, m_logger);
            continue;
        }
        const PropertyDeclaration decl = moduleInstance->propertyDeclaration(vmit.key());
        moduleInstance->setProperty(vmit.key(),
                VariantValue::create(convertToPropertyType(vmit.value(), decl.type(), moduleName,
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
            probeBindings += ProbeProperty(name, m_evaluator->value(probe, name));
        }
    }
    QScriptValue scope = m_engine->newObject();
    m_engine->currentContext()->pushScope(m_evaluator->scriptValue(parent));
    m_engine->currentContext()->pushScope(m_evaluator->fileScope(configureScript->file()));
    m_engine->currentContext()->pushScope(scope);
    foreach (const ProbeProperty &b, probeBindings)
        scope.setProperty(b.first, b.second);
    QScriptValue sv = m_engine->evaluate(configureScript->sourceCodeForEvaluation());
    if (Q_UNLIKELY(m_engine->hasErrorOrException(sv)))
        throw ErrorInfo(m_engine->lastErrorString(sv), configureScript->location());
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
                    .arg(TopLevelProject::deriveId(m_parameters.topLevelProfile(),
                                                   m_parameters.finalBuildConfigurationTree())));
    }
}

bool ModuleLoader::checkItemCondition(Item *item)
{
    if (m_evaluator->boolValue(item, QLatin1String("condition"), true))
        return true;
    m_disabledItems += item;
    return false;
}

void ModuleLoader::checkItemTypes(Item *item)
{
    if (Q_UNLIKELY(!item->typeName().isEmpty()
                   && !BuiltinDeclarations::instance().containsType(item->typeName()))) {
        const QString msg = Tr::tr("Unexpected item type '%1'.");
        throw ErrorInfo(msg.arg(item->typeName()), item->location());
    }

    const ItemDeclaration decl
            = BuiltinDeclarations::instance().declarationsForType(item->typeName());
    foreach (Item *child, item->children()) {
        if (child->typeName().isEmpty())
            continue;
        checkItemTypes(child);
        if (!decl.isChildTypeAllowed(child->typeName()))
            throw ErrorInfo(Tr::tr("Items of type '%1' cannot contain items of type '%2'.")
                .arg(item->typeName(), child->typeName()), item->location());
    }

    foreach (const Item::Module &m, item->modules())
        checkItemTypes(m.item);
}

QStringList ModuleLoader::readExtraSearchPaths(Item *item, bool *wasSet)
{
    QStringList result;
    const QString propertyName = QLatin1String("qbsSearchPaths");
    const QStringList paths = m_evaluator->stringListValue(item, propertyName, wasSet);
    const JSSourceValueConstPtr prop = item->sourceProperty(propertyName);

    // Value can come from within a project file or as an overridden value from the user
    // (e.g command line).
    const QString basePath = FileInfo::path(prop ? prop->file()->filePath()
                                                 : m_parameters.projectFilePath());
    foreach (const QString &path, paths)
        result += FileInfo::resolvePath(basePath, path);
    return result;
}

void ModuleLoader::copyProperties(const Item *sourceProject, Item *targetProject)
{
    if (!sourceProject)
        return;
    const QList<PropertyDeclaration> &builtinProjectProperties = BuiltinDeclarations::instance()
            .declarationsForType(QLatin1String("Project")).properties();
    QSet<QString> builtinProjectPropertyNames;
    foreach (const PropertyDeclaration &p, builtinProjectProperties)
        builtinProjectPropertyNames << p.name();

    for (Item::PropertyDeclarationMap::ConstIterator it
         = sourceProject->propertyDeclarations().constBegin();
         it != sourceProject->propertyDeclarations().constEnd(); ++it) {

        // We must not inherit built-in properties such as "name",
        // but there are exceptions.
        if (it.key() == QLatin1String("qbsSearchPaths") || it.key() == QLatin1String("profile")
                || it.key() == QLatin1String("buildDirectory")
                || it.key() == QLatin1String("sourceDirectory")) {
            const JSSourceValueConstPtr &v
                    = targetProject->property(it.key()).dynamicCast<const JSSourceValue>();
            QBS_ASSERT(v, continue);
            if (v->sourceCode() == QLatin1String("undefined"))
                sourceProject->copyProperty(it.key(), targetProject);
            continue;
        }

        if (builtinProjectPropertyNames.contains(it.key()))
            continue;

        if (targetProject->properties().contains(it.key()))
            continue; // Ignore stuff the target project already has.

        targetProject->setPropertyDeclaration(it.key(), it.value());
        sourceProject->copyProperty(it.key(), targetProject);
    }
}

Item *ModuleLoader::wrapWithProject(Item *item)
{
    Item *prj = Item::create(item->pool());
    prj->setChildren(QList<Item *>() << item);
    item->setParent(prj);
    prj->setTypeName(QLatin1String("Project"));
    prj->setFile(item->file());
    prj->setLocation(item->location());
    prj->setupForBuiltinType(m_logger);
    return prj;
}

QString ModuleLoader::findExistingModulePath(const QString &searchPath,
        const QualifiedId &moduleName)
{
    QString dirPath = searchPath;
    foreach (const QString &moduleNamePart, moduleName) {
        dirPath = FileInfo::resolvePath(dirPath, moduleNamePart);
        if (!FileInfo::exists(dirPath) || !FileInfo::isFileCaseCorrect(dirPath))
            return QString();
    }
    return dirPath;
}

void ModuleLoader::setScopeForDescendants(Item *item, Item *scope)
{
    foreach (Item *child, item->children()) {
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
        const PropertyDeclaration decl = item->propertyDeclarations().value(it.key());
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

static void collectAllModules(Item *item, QVector<Item::Module> *modules)
{
    foreach (const Item::Module &m, item->modules()) {
        auto it = std::find_if(modules->begin(), modules->end(),
                               [m] (const Item::Module &m2) { return m.name == m2.name; });
        if (it != modules->end()) {
            // If a module is required somewhere, it is required in the top-level item.
            if (m.required)
                it->required = true;
            continue;
        }
        modules->append(m);
        collectAllModules(m.item, modules);
    }
}

static QVector<Item::Module> allModules(Item *item)
{
    QVector<Item::Module> lst;
    collectAllModules(item, &lst);
    return lst;
}

void ModuleLoader::addTransitiveDependencies(ProductContext *ctx, Item *item)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[MODLDR] addTransitiveDependencies";
    QVector<Item::Module> transitiveDeps = allModules(item);
    std::sort(transitiveDeps.begin(), transitiveDeps.end());
    foreach (const Item::Module &m, item->modules()) {
        if (m.isProduct) {
            ctx->info.usedProducts.append(
                        ctx->project->topLevelProject->productModules.value(
                            m.name.toString()).productDependencies);
        }

        auto it = std::lower_bound(transitiveDeps.begin(), transitiveDeps.end(), m);
        if (it != transitiveDeps.end() && it->name == m.name)
            transitiveDeps.erase(it);
    }
    foreach (const Item::Module &module, transitiveDeps) {
        if (module.isProduct) {
            item->addModule(module);
            ctx->info.usedProducts.append(
                        ctx->project->topLevelProject->productModules.value(
                            module.name.toString()).productDependencies);
        } else {
            Item::Module dep;
            dep.item = loadModule(ctx, item, item->location(), QString(), module.name,
                                  false, module.required, &dep.isProduct);
            if (!dep.item)
                continue;
            dep.name = module.name;
            item->addModule(dep);
        }
    }
}

Item *ModuleLoader::createNonPresentModule(const QString &name, const QString &reason, Item *module)
{
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << "Non-required module '" << name << "' not loaded (" << reason << ")."
                            << "Creating dummy module for presence check.";
    }
    if (!module) {
        module = Item::create(m_pool);
        module->setFile(FileContext::create());
        module->setTypeName(QLatin1String("Module"));
    }
    module->setProperty(QLatin1String("present"), VariantValue::create(false));
    return module;
}

void ModuleLoader::copyGroupsFromModuleToProduct(Item *productItem, const Item *modulePrototype)
{
    for (int i = 0; i < modulePrototype->children().count(); ++i) {
        Item * const child = modulePrototype->children().at(i);
        if (child->typeName() == QLatin1String("Group"))
            Item::addChild(productItem, child->clone());
    }
}

void ModuleLoader::copyGroupsFromModulesToProduct(Item *productItem)
{
    foreach (const Item::Module &module, productItem->modules()) {
        Item *prototype = module.item;
        bool modulePassedValidation;
        while ((modulePassedValidation = prototype->isPresentModule()
                && !prototype->delayedError().hasError())
                && prototype->prototype()) {
            prototype = prototype->prototype();
        }
        if (modulePassedValidation)
            copyGroupsFromModuleToProduct(productItem, prototype);
    }
}

QString ModuleLoaderResult::ProductInfo::Dependency::uniqueName() const
{
    return ResolvedProduct::uniqueName(name, profile);
}

} // namespace Internal
} // namespace qbs
