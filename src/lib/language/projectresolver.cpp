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

#include "projectresolver.h"

#include "artifactproperties.h"
#include "builtindeclarations.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "moduleloader.h"
#include "propertymapinternal.h"
#include "scriptengine.h"
#include <jsextensions/moduleproperties.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QFileInfo>
#include <QDir>
#include <set>

namespace qbs {
namespace Internal {

extern bool debugProperties;

static const FileTag unknownFileTag()
{
    static const FileTag tag("unknown-file-tag");
    return tag;
}

ProjectResolver::ProjectResolver(ModuleLoader *ldr, const BuiltinDeclarations *builtins,
                                 const Logger &logger)
    : m_evaluator(ldr->evaluator())
    , m_logger(logger)
    , m_engine(m_evaluator->engine())
    , m_progressObserver(0)
{
    foreach (const PropertyDeclaration &pd, builtins->declarationsForType("Group"))
        m_groupPropertyDeclarations += pd.name;
}

ProjectResolver::~ProjectResolver()
{
}

void ProjectResolver::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

ResolvedProjectPtr ProjectResolver::resolve(ModuleLoaderResult &loadResult,
                                            const QString &buildRoot,
                                            const QVariantMap &buildConfiguration)
{
    QBS_ASSERT(FileInfo::isAbsolute(buildRoot), return ResolvedProjectPtr());
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[PR] resolving " << loadResult.root->file()->filePath();

    ProjectContext projectContext;
    projectContext.loadResult = &loadResult;
    m_buildRoot = buildRoot;
    m_buildConfiguration = buildConfiguration;
    m_projectContext = &projectContext;
    m_productContext = 0;
    m_moduleContext = 0;
    resolveProject(loadResult.root);
    m_projectContext = 0;
    projectContext.project->usedEnvironment = m_engine->usedEnvironment();
    return projectContext.project;
}

void ProjectResolver::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled())
        throw Error(Tr::tr("Loading canceled."));
}

bool ProjectResolver::boolValue(const ItemConstPtr &item, const QString &name, bool defaultValue) const
{
    QScriptValue v = m_evaluator->property(item, name);
    if (Q_UNLIKELY(v.isError())) {
        ValuePtr value = item->property(name);
        throw Error(v.toString(), value ? value->location() : CodeLocation());
    }
    if (!v.isValid() || v.isUndefined())
        return defaultValue;
    return v.toBool();
}

FileTags ProjectResolver::fileTagsValue(const ItemConstPtr &item, const QString &name) const
{
    return FileTags::fromStringList(stringListValue(item, name));
}

QString ProjectResolver::stringValue(const ItemConstPtr &item, const QString &name,
                                     const QString &defaultValue) const
{
    QScriptValue v = m_evaluator->property(item, name);
    if (Q_UNLIKELY(v.isError())) {
        ValuePtr value = item->property(name);
        throw Error(v.toString(), value ? value->location() : CodeLocation());
    }
    if (!v.isValid() || v.isUndefined())
        return defaultValue;
    return v.toString();
}

QStringList ProjectResolver::stringListValue(const ItemConstPtr &item, const QString &name) const
{
    QScriptValue v = m_evaluator->property(item, name);
    if (Q_UNLIKELY(v.isError())) {
        ValuePtr value = item->property(name);
        throw Error(v.toString(), value ? value->location() : CodeLocation());
    }
    return toStringList(v);
}

QString ProjectResolver::verbatimValue(const ValueConstPtr &value) const
{
    QString result;
    if (value && value->type() == Value::JSSourceValueType) {
        const JSSourceValueConstPtr sourceValue = value.staticCast<const JSSourceValue>();
        result = sourceValue->sourceCode();
    }
    return result;
}

QString ProjectResolver::verbatimValue(const ItemPtr &item, const QString &name) const
{
    return verbatimValue(item->property(name));
}

void ProjectResolver::ignoreItem(const ItemPtr &item)
{
    Q_UNUSED(item);
}

void ProjectResolver::resolveProject(const ItemPtr &item)
{
    checkCancelation();
    ResolvedProjectPtr project = ResolvedProject::create();
    m_projectContext->project = project;
    m_projectContext->dummyModule = ResolvedModule::create();
    m_projectContext->dummyModule->jsImports = item->file()->jsImports();
    project->location = item->location();
    project->setBuildConfiguration(m_buildConfiguration);
    project->buildDirectory = ResolvedProject::deriveBuildDirectory(m_buildRoot, project->id());

    ItemFuncMap mapping;
    mapping["Product"] = &ProjectResolver::resolveProduct;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Rule"] = &ProjectResolver::resolveRule;

    if (m_progressObserver)
        m_progressObserver->setMaximum(item->children().count() + 2);
    foreach (const ItemPtr &child, item->children()) {
        callItemFunction(mapping, child);
        if (m_progressObserver)
            m_progressObserver->incrementProgressValue();
    }

    foreach (const ResolvedProductPtr &product, project->products)
        postProcess(product);
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();

    resolveProductDependencies();
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();
}

void ProjectResolver::resolveProduct(const ItemPtr &item)
{
    checkCancelation();
    ProductContext productContext;
    m_productContext = &productContext;
    productContext.item = item;
    item->setProperty(QLatin1String("buildDirectory"),
                      VariantValue::create(m_projectContext->project->buildDirectory));
    ResolvedProductPtr product = ResolvedProduct::create();
    m_projectContext->productItemMap.insert(product, item);
    m_projectContext->project->products += product;
    productContext.product = product;
    product->name = stringValue(item, QLatin1String("name"));
    if (product->name.isEmpty()) {
        product->name = FileInfo::completeBaseName(item->file()->filePath());
        item->setProperty("name", VariantValue::create(product->name));
    }
    m_logger.qbsTrace() << "[PR] resolveProduct " << product->name;
    m_projectContext->productsByName.insert(product->name.toLower(), product);
    product->enabled = boolValue(item, QLatin1String("condition"), true);
    product->additionalFileTags = fileTagsValue(item, QLatin1String("additionalFileTags"));
    product->fileTags = fileTagsValue(item, QLatin1String("type"));
    product->targetName = stringValue(item, QLatin1String("targetName"));
    product->sourceDirectory = QFileInfo(item->file()->filePath()).absolutePath();
    product->destinationDirectory = stringValue(item, QLatin1String("destinationDirectory"));
    product->location = item->location();
    product->project = m_projectContext->project;
    product->properties = PropertyMapInternal::create();
    product->properties->setValue(createProductConfig());
    ModuleProperties::init(m_evaluator->scriptValue(item), product);

    QList<ItemPtr> subItems = item->children();
    const ValuePtr filesProperty = item->property(QLatin1String("files"));
    if (filesProperty) {
        ItemPtr fakeGroup = Item::create();
        fakeGroup->setFile(item->file());
        fakeGroup->setLocation(item->location());
        fakeGroup->setScope(item);
        fakeGroup->setTypeName(QLatin1String("Group"));
        fakeGroup->setProperty(QLatin1String("name"), VariantValue::create(product->name));
        fakeGroup->setProperty(QLatin1String("files"), filesProperty);
        fakeGroup->setProperty(QLatin1String("excludeFiles"),
                               item->property(QLatin1String("excludeFiles")));
        fakeGroup->setProperty(QLatin1String("overrideTags"), VariantValue::create(false));
        subItems.prepend(fakeGroup);
    }

    ItemFuncMap mapping;
    mapping["Depends"] = &ProjectResolver::ignoreItem;
    mapping["Rule"] = &ProjectResolver::resolveRule;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Transformer"] = &ProjectResolver::resolveTransformer;
    mapping["Group"] = &ProjectResolver::resolveGroup;
    mapping["ProductModule"] = &ProjectResolver::resolveProductModule;
    mapping["Probe"] = &ProjectResolver::ignoreItem;

    foreach (const ItemPtr &child, subItems)
        callItemFunction(mapping, child);

    foreach (const Item::Module &module, item->modules())
        resolveModule(module.name, module.item);

    m_productContext = 0;
}

void ProjectResolver::resolveModule(const QStringList &moduleName, const ItemPtr &item)
{
    checkCancelation();
    ModuleContext moduleContext;
    moduleContext.module = ResolvedModule::create();
    m_moduleContext = &moduleContext;

    const ResolvedModulePtr &module = moduleContext.module;
    module->name = ModuleLoader::fullModuleName(moduleName);
    module->setupBuildEnvironmentScript = verbatimValue(item, "setupBuildEnvironment");
    module->setupRunEnvironmentScript = verbatimValue(item, "setupRunEnvironment");

    m_productContext->product->additionalFileTags
            += fileTagsValue(item, "additionalProductFileTags");

    // TODO: instead of jsImports, we need the file context for both setup scripts separately.
    //       ATM the setup scripts must be in the first file of the inheritance chain.
    module->jsImports = item->file()->jsImports();

    foreach (const Item::Module &m, item->modules())
        module->moduleDependencies += ModuleLoader::fullModuleName(m.name);

    m_productContext->product->modules += module;

    ItemFuncMap mapping;
    mapping["Rule"] = &ProjectResolver::resolveRule;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Transformer"] = &ProjectResolver::resolveTransformer;
    mapping["PropertyOptions"] = &ProjectResolver::ignoreItem;
    mapping["Depends"] = &ProjectResolver::ignoreItem;
    mapping["Probe"] = &ProjectResolver::ignoreItem;
    foreach (const ItemPtr &child, item->children())
        callItemFunction(mapping, child);

    m_moduleContext = 0;
}

static void createSourceArtifact(const ResolvedProductConstPtr &rproduct,
                                 const PropertyMapPtr &properties,
                                 const QString &fileName,
                                 const FileTags &fileTags,
                                 bool overrideTags,
                                 QList<SourceArtifactPtr> &artifactList)
{
    SourceArtifactPtr artifact = SourceArtifact::create();
    artifact->absoluteFilePath = FileInfo::resolvePath(rproduct->sourceDirectory, fileName);
    artifact->fileTags = fileTags;
    artifact->overrideFileTags = overrideTags;
    artifact->properties = properties;
    artifactList += artifact;
}

static bool isSomeModulePropertySet(const ItemPtr &group)
{
    for (QMap<QString, ValuePtr>::const_iterator it = group->properties().constBegin();
         it != group->properties().constEnd(); ++it)
    {
        if (it.value()->type() == Value::ItemValueType) {
            // An item value is a module value in this case.
            ItemValuePtr iv = it.value().staticCast<ItemValue>();
            foreach (ValuePtr ivv, iv->item()->properties()) {
                if (ivv->type() == Value::JSSourceValueType)
                    return true;
            }
        }
    }
    return false;
}

void ProjectResolver::resolveGroup(const ItemPtr &item)
{
    checkCancelation();
    PropertyMapPtr properties = m_productContext->product->properties;
    if (isSomeModulePropertySet(item)) {
        properties = PropertyMapInternal::create();
        properties->setValue(evaluateModuleValues(item));
    }

    QStringList files = stringListValue(item, QLatin1String("files"));
    const QStringList fileTagsFilter = stringListValue(item, QLatin1String("fileTagsFilter"));
    if (!fileTagsFilter.isEmpty()) {
        if (Q_UNLIKELY(!files.isEmpty()))
            throw Error(Tr::tr("Group.files and Group.fileTagsFilters are exclusive."),
                        item->location());
        ArtifactPropertiesPtr aprops = ArtifactProperties::create();
        aprops->setFileTagsFilter(FileTags::fromStringList(fileTagsFilter));
        PropertyMapPtr cfg = PropertyMapInternal::create();
        cfg->setValue(evaluateModuleValues(item));
        aprops->setPropertyMapInternal(cfg);
        m_productContext->product->artifactProperties += aprops;
        return;
    }
    if (Q_UNLIKELY(files.isEmpty() && !item->hasProperty(QLatin1String("files")))) {
        // Yield an error if Group without files binding is encountered.
        // An empty files value is OK but a binding must exist.
        throw Error(Tr::tr("Group without files is not allowed."),
                    item->location());
    }
    QStringList patterns;
    QString prefix;
    for (int i = files.count(); --i >= 0;) {
        if (FileInfo::isPattern(files[i]))
            patterns.append(files.takeAt(i));
    }
    prefix = stringValue(item, QLatin1String("prefix"));
    if (!prefix.isEmpty()) {
        for (int i = files.count(); --i >= 0;)
                files[i].prepend(prefix);
    }
    FileTags fileTags = fileTagsValue(item, QLatin1String("fileTags"));
    bool overrideTags = boolValue(item, QLatin1String("overrideTags"), true);

    GroupPtr group = ResolvedGroup::create();
    group->location = item->location();
    group->enabled = boolValue(item, QLatin1String("condition"), true);

    if (!patterns.isEmpty()) {
        SourceWildCards::Ptr wildcards = SourceWildCards::create();
        wildcards->excludePatterns = stringListValue(item, "excludeFiles");
        wildcards->prefix = prefix;
        wildcards->patterns = patterns;
        QSet<QString> files = wildcards->expandPatterns(group, m_productContext->product->sourceDirectory);
        foreach (const QString &fileName, files)
            createSourceArtifact(m_productContext->product, properties, fileName,
                                 fileTags, overrideTags, wildcards->files);
        group->wildcards = wildcards;
    }

    foreach (const QString &fileName, files)
        createSourceArtifact(m_productContext->product, properties, fileName,
                             fileTags, overrideTags, group->files);

    group->name = stringValue(item, "name");
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(m_productContext->product->groups.count());
    group->properties = properties;
    m_productContext->product->groups += group;
}

static QString sourceCodeAsFunction(const JSSourceValueConstPtr &value)
{
    if (value->hasFunctionForm()) {
        // Remove the function application "()" that has been
        // added in ItemReaderASTVisitor::visitStatement.
        const QString &code = value->sourceCode();
        return code.left(code.length() - 2);
    } else {
        return QLatin1String("(function(){return ") + value->sourceCode() + QLatin1String(";})");
    }
}

void ProjectResolver::resolveRule(const ItemPtr &item)
{
    checkCancelation();

    if (!boolValue(item, QLatin1String("condition"), true))
        return;

    RulePtr rule = Rule::create();

    // read artifacts
    bool hasAlwaysUpdatedArtifact = false;
    foreach (const ItemPtr &child, item->children()) {
        if (Q_UNLIKELY(child->typeName() != QLatin1String("Artifact")))
            throw Error(Tr::tr("'Rule' can only have children of type 'Artifact'."),
                               child->location());

        resolveRuleArtifact(rule, child, &hasAlwaysUpdatedArtifact);
    }

    if (Q_UNLIKELY(!hasAlwaysUpdatedArtifact))
        throw Error(Tr::tr("At least one output artifact of a rule "
                           "must have alwaysUpdated set to true."),
                    item->location());

    const PrepareScriptPtr prepareScript = PrepareScript::create();
    JSSourceValuePtr value = item->sourceProperty(QLatin1String("prepare"));
    if (value) {
        prepareScript->script = sourceCodeAsFunction(value);
        prepareScript->location = value->location();
    }

    rule->jsImports = item->file()->jsImports();
    rule->script = prepareScript;
    rule->multiplex = boolValue(item, "multiplex", false);
    rule->inputs = fileTagsValue(item, "inputs");
    rule->usings = fileTagsValue(item, "usings");
    rule->explicitlyDependsOn = fileTagsValue(item, "explicitlyDependsOn");
    rule->module = m_moduleContext ? m_moduleContext->module : m_projectContext->dummyModule;
    if (m_productContext)
        m_productContext->product->rules += rule;
    else
        m_projectContext->rules += rule;
}

class StringListLess
{
public:
    bool operator()(const QStringList &lhs, const QStringList &rhs)
    {
        const int c = qMin(lhs.count(), rhs.count());
        for (int i = 0; i < c; ++i) {
            int n = lhs.at(i).compare(rhs.at(i));
            if (n < 0)
                return true;
            if (n > 0)
                return false;
        }
        return lhs.count() < rhs.count();
    }
};

class StringListSet : public std::set<QStringList, StringListLess>
{
public:
    typedef std::pair<iterator, bool> InsertResult;
};

void ProjectResolver::resolveRuleArtifact(const RulePtr &rule, const ItemPtr &item,
                                          bool *hasAlwaysUpdatedArtifact)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts += artifact;
    artifact->fileName = verbatimValue(item, "fileName");
    artifact->fileTags = fileTagsValue(item, "fileTags");
    artifact->alwaysUpdated = boolValue(item, "alwaysUpdated", true);
    if (artifact->alwaysUpdated)
        *hasAlwaysUpdatedArtifact = true;

    StringListSet seenBindings;
    for (ItemPtr obj = item; obj; obj = obj->prototype()) {
        for (QMap<QString, ValuePtr>::const_iterator it = obj->properties().constBegin();
             it != obj->properties().constEnd(); ++it)
        {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            resolveRuleArtifactBinding(artifact, it.value().staticCast<ItemValue>()->item(),
                 QStringList(it.key()), &seenBindings);
        }
    }
}

void ProjectResolver::resolveRuleArtifactBinding(const RuleArtifactPtr &ruleArtifact,
                                                 const ItemPtr &item,
                                                 const QStringList &namePrefix,
                                                 StringListSet *seenBindings)
{
    for (QMap<QString, ValuePtr>::const_iterator it = item->properties().constBegin();
         it != item->properties().constEnd(); ++it)
    {
        const QStringList name = QStringList(namePrefix) << it.key();
        if (it.value()->type() == Value::ItemValueType) {
            resolveRuleArtifactBinding(ruleArtifact,
                                       it.value().staticCast<ItemValue>()->item(), name,
                                       seenBindings);
        } else if (it.value()->type() == Value::JSSourceValueType) {
            const StringListSet::InsertResult insertResult = seenBindings->insert(name);
            if (!insertResult.second)
                continue;
            JSSourceValuePtr sourceValue = it.value().staticCast<JSSourceValue>();
            RuleArtifact::Binding rab;
            rab.name = name;
            rab.code = sourceValue->sourceCode();
            rab.location = sourceValue->location();
            ruleArtifact->bindings += rab;
        } else {
            QBS_ASSERT(!"unexpected value type", continue);
        }
    }
}

void ProjectResolver::resolveFileTagger(const ItemPtr &item)
{
    checkCancelation();
    QSet<FileTaggerConstPtr> &fileTaggers = m_productContext
            ? m_productContext->product->fileTaggers : m_projectContext->fileTaggers;
    fileTaggers += FileTagger::create(QRegExp(stringValue(item,"pattern")),
            fileTagsValue(item, "fileTags"));
}

void ProjectResolver::resolveTransformer(const ItemPtr &item)
{
    checkCancelation();
    if (!boolValue(item, "condition", true)) {
        m_logger.qbsTrace() << "[PR] transformer condition is false";
        return;
    }

    ResolvedTransformer::Ptr rtrafo = ResolvedTransformer::create();
    rtrafo->module = m_moduleContext ? m_moduleContext->module : m_projectContext->dummyModule;
    rtrafo->jsImports = item->file()->jsImports();
    rtrafo->inputs = stringListValue(item, "inputs");
    for (int i = 0; i < rtrafo->inputs.count(); ++i)
        rtrafo->inputs[i] = FileInfo::resolvePath(m_productContext->product->sourceDirectory, rtrafo->inputs.at(i));
    const PrepareScriptPtr transform = PrepareScript::create();
    JSSourceValueConstPtr value = item->sourceProperty(QLatin1String("prepare"));
    transform->script = sourceCodeAsFunction(value);
    transform->location = value ? value->location() : item->location();
    rtrafo->transform = transform;

    foreach (const ItemConstPtr &child, item->children()) {
        if (Q_UNLIKELY(child->typeName() != QLatin1String("Artifact")))
            throw Error(Tr::tr("Transformer: wrong child type '%0'.").arg(child->typeName()));
        SourceArtifactPtr artifact = SourceArtifact::create();
        artifact->properties = m_productContext->product->properties;
        QString fileName = stringValue(child, "fileName");
        if (Q_UNLIKELY(fileName.isEmpty()))
            throw Error(Tr::tr("Artifact fileName must not be empty."));
        artifact->absoluteFilePath = FileInfo::resolvePath(m_productContext->product->project->buildDirectory,
                                                           fileName);
        artifact->fileTags = fileTagsValue(child, "fileTags");
        if (artifact->fileTags.isEmpty())
            artifact->fileTags.insert(unknownFileTag());
        rtrafo->outputs += artifact;
    }

    m_productContext->product->transformers += rtrafo;
}

void ProjectResolver::resolveProductModule(const ItemPtr &item)
{
    checkCancelation();
    const QString &productName = m_productContext->product->name.toLower();
    m_projectContext->productModules[productName] = evaluateModuleValues(item);
}

static void insertProductModuleConfig(const QString &usedProductName,
                                      const QVariantMap &productModuleConfig,
                                      const PropertyMapPtr &propertyMap)
{
    QVariantMap properties = propertyMap->value();
    QVariant &modulesEntry = properties[QLatin1String("modules")];
    QVariantMap modules = modulesEntry.toMap();
    modules.insert(usedProductName, productModuleConfig);
    modulesEntry = modules;
    propertyMap->setValue(properties);
}

static void addUsedProducts(ModuleLoaderResult::ProductInfo *productInfo,
                            const ModuleLoaderResult::ProductInfo &usedProductInfo,
                            bool *productsAdded)
{
    int oldCount = productInfo->usedProducts.count();
    QSet<QString> usedProductNames;
    foreach (const ModuleLoaderResult::ProductInfo::Dependency &usedProduct,
            productInfo->usedProducts)
        usedProductNames += usedProduct.name;
    foreach (const ModuleLoaderResult::ProductInfo::Dependency &usedProduct,
             usedProductInfo.usedProductsFromProductModule) {
        if (!usedProductNames.contains(usedProduct.name))
            productInfo->usedProducts  += usedProduct;
    }
    *productsAdded = (oldCount != productInfo->usedProducts.count());
}

void ProjectResolver::resolveProductDependencies()
{
    // Collect product dependencies from ProductModules.
    bool productDependenciesAdded;
    do {
        productDependenciesAdded = false;
        foreach (ResolvedProductPtr rproduct, m_projectContext->project->products) {
            const ItemPtr productItem = m_projectContext->productItemMap.value(rproduct);
            ModuleLoaderResult::ProductInfo &productInfo
                    = m_projectContext->loadResult->productInfos[productItem];
            foreach (const ModuleLoaderResult::ProductInfo::Dependency &dependency,
                        productInfo.usedProducts) {
                ResolvedProductPtr usedProduct
                        = m_projectContext->productsByName.value(dependency.name);
                if (Q_UNLIKELY(!usedProduct))
                    throw Error(Tr::tr("Product dependency '%1' not found.").arg(dependency.name),
                                productItem->location());
                const ItemPtr usedProductItem = m_projectContext->productItemMap.value(usedProduct);
                const ModuleLoaderResult::ProductInfo usedProductInfo
                        = m_projectContext->loadResult->productInfos.value(usedProductItem);
                bool added;
                addUsedProducts(&productInfo, usedProductInfo, &added);
                if (added)
                    productDependenciesAdded = true;
            }
        }
    } while (productDependenciesAdded);

    // Resolve all inter-product dependencies.
    foreach (ResolvedProductPtr rproduct, m_projectContext->project->products) {
        const ItemPtr productItem = m_projectContext->productItemMap.value(rproduct);
        foreach (const ModuleLoaderResult::ProductInfo::Dependency &dependency,
                 m_projectContext->loadResult->productInfos.value(productItem).usedProducts) {
            const QString &usedProductName = dependency.name;
            ResolvedProductPtr usedProduct
                    = m_projectContext->productsByName.value(usedProductName);
            if (Q_UNLIKELY(!usedProduct))
                throw Error(Tr::tr("Product dependency '%1' not found.").arg(usedProductName),
                            productItem->location());
            rproduct->dependencies.insert(usedProduct);

            // insert the configuration of the ProductModule into the product's configuration
            const QVariantMap productModuleConfig
                    = m_projectContext->productModules.value(usedProductName);
            if (productModuleConfig.isEmpty())
                continue;

            insertProductModuleConfig(usedProductName, productModuleConfig, rproduct->properties);

            // insert the configuration of the ProductModule into the artifact configurations
            foreach (SourceArtifactPtr artifact, rproduct->allEnabledFiles()) {
                if (artifact->properties != rproduct->properties)
                    insertProductModuleConfig(usedProductName, productModuleConfig,
                                              artifact->properties);
            }
        }
    }
}

void ProjectResolver::postProcess(const ResolvedProductPtr &product) const
{
    product->fileTaggers += m_projectContext->fileTaggers;
    foreach (const RulePtr &rule, m_projectContext->rules)
        product->rules += rule;
    applyFileTaggers(product);
}

void ProjectResolver::applyFileTaggers(const ResolvedProductPtr &product) const
{
    foreach (const SourceArtifactPtr &artifact, product->allEnabledFiles())
        applyFileTaggers(artifact, product);
}

void ProjectResolver::applyFileTaggers(const SourceArtifactPtr &artifact,
                                       const ResolvedProductConstPtr &product) const
{
    if (!artifact->overrideFileTags || artifact->fileTags.isEmpty()) {
        const FileTags fileTags = product->fileTagsForFileName(artifact->absoluteFilePath);
        artifact->fileTags.unite(fileTags);
        if (artifact->fileTags.isEmpty())
            artifact->fileTags.insert(unknownFileTag());
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "[PR] adding file tags " << artifact->fileTags
                       << " to " << FileInfo::fileName(artifact->absoluteFilePath);
    }
}

QVariantMap ProjectResolver::evaluateModuleValues(const ItemPtr &item) const
{
    QVariantMap modules;
    evaluateModuleValues(item, &modules);
    QVariantMap result;
    result[QLatin1String("modules")] = modules;
    return result;
}

void ProjectResolver::evaluateModuleValues(const ItemPtr &item, QVariantMap *modulesMap) const
{
    checkCancelation();
    for (Item::Modules::const_iterator it = item->modules().constBegin();
         it != item->modules().constEnd(); ++it)
    {
        QVariantMap depmods;
        const Item::Module &module = *it;
        evaluateModuleValues(module.item, &depmods);
        QVariantMap dep = evaluateProperties(module.item);
        dep.insert("modules", depmods);
        modulesMap->insert(ModuleLoader::fullModuleName(module.name), dep);
    }
}

QVariantMap ProjectResolver::evaluateProperties(const ItemPtr &item) const
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt);
}

QVariantMap ProjectResolver::evaluateProperties(const ItemPtr &item,
                                                const ItemPtr &propertiesContainer,
                                                const QVariantMap &tmplt) const
{
    QVariantMap result = tmplt;
    for (QMap<QString, ValuePtr>::const_iterator it = propertiesContainer->properties().begin();
         it != propertiesContainer->properties().end(); ++it)
    {
        checkCancelation();
        switch (it.value()->type()) {
        case Value::ItemValueType:
        {
            // Ignore items. Those point to module instances
            // and are handled in evaluateModuleValues().
            break;
        }
        case Value::JSSourceValueType:
        {
            if (result.contains(it.key()))
                break;
            PropertyDeclaration pd;
            for (ItemPtr obj = item; obj; obj = obj->prototype()) {
                pd = obj->propertyDeclarations().value(it.key());
                if (pd.isValid())
                    break;
            }
            if (pd.type == PropertyDeclaration::Verbatim
                || pd.flags.testFlag(PropertyDeclaration::PropertyNotAvailableInConfig))
            {
                break;
            }
            const QScriptValue scriptValue = m_evaluator->property(item, it.key());
            if (Q_UNLIKELY(scriptValue.isError()))
                throw Error(scriptValue.toString(), it.value()->location());
            QVariant v = scriptValue.toVariant();
            if (pd.type == PropertyDeclaration::Path)
                v = convertPathProperty(v.toString(),
                                        m_productContext->product->sourceDirectory);
            else if (pd.type == PropertyDeclaration::PathList)
                v = convertPathListProperty(v.toStringList(),
                                            m_productContext->product->sourceDirectory);
            result[it.key()] = v;
            break;
        }
        case Value::VariantValueType:
        {
            VariantValuePtr vvp = it.value().staticCast<VariantValue>();
            result[it.key()] = vvp->value();
            break;
        }
        case Value::BuiltinValueType:
            // ignore
            break;
        }
    }
    return propertiesContainer->prototype()
            ? evaluateProperties(item, propertiesContainer->prototype(), result)
            : result;
}

QVariantMap ProjectResolver::createProductConfig() const
{
    QVariantMap cfg = evaluateModuleValues(m_productContext->item);
    cfg = evaluateProperties(m_productContext->item, m_productContext->item, cfg);
    return cfg;
}

QString ProjectResolver::convertPathProperty(const QString &path, const QString &dirPath) const
{
    return path.isEmpty() ? path : QDir::cleanPath(FileInfo::resolvePath(dirPath, path));
}

QStringList ProjectResolver::convertPathListProperty(const QStringList &paths,
                                                     const QString &dirPath) const
{
    QStringList result;
    foreach (const QString &path, paths)
        result += convertPathProperty(path, dirPath);
    return result;
}

void ProjectResolver::callItemFunction(const ItemFuncMap &mappings,
                                       const ItemPtr &item)
{
    const QByteArray typeName = item->typeName().toLocal8Bit();
    ItemFuncPtr f = mappings.value(typeName);
    if (Q_UNLIKELY(!f)) {
        const QString msg = Tr::tr("Unexpected item type '%1'.");
        throw Error(msg.arg(item->typeName()), item->location());
    }
    (this->*f)(item);
}

} // namespace Internal
} // namespace qbs
