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
#include <QSet>
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
    , m_builtins(builtins)
    , m_logger(logger)
    , m_engine(m_evaluator->engine())
    , m_progressObserver(0)
{
}

ProjectResolver::~ProjectResolver()
{
}

void ProjectResolver::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

TopLevelProjectPtr ProjectResolver::resolve(ModuleLoaderResult &loadResult,
        const QString &buildRoot, const QVariantMap &overriddenProperties,
        const QVariantMap &buildConfiguration)
{
    QBS_ASSERT(FileInfo::isAbsolute(buildRoot), return TopLevelProjectPtr());
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[PR] resolving " << loadResult.root->file()->filePath();

    ProjectContext projectContext;
    projectContext.loadResult = &loadResult;
    m_buildRoot = buildRoot;
    m_overriddenProperties = overriddenProperties;
    m_buildConfiguration = buildConfiguration;
    m_productContext = 0;
    m_moduleContext = 0;
    resolveTopLevelProject(loadResult.root, &projectContext);
    TopLevelProjectPtr top = projectContext.project.staticCast<TopLevelProject>();
    top->buildSystemFiles.unite(loadResult.qbsFiles);
    return top;
}

void ProjectResolver::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled()) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration %1.")
                    .arg(TopLevelProject::deriveId(m_buildConfiguration)));
    }
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

QString ProjectResolver::verbatimValue(Item *item, const QString &name) const
{
    return verbatimValue(item->property(name));
}

void ProjectResolver::ignoreItem(Item *item, ProjectContext *projectContext)
{
    Q_UNUSED(item);
    Q_UNUSED(projectContext);
}

static void makeSubProjectNamesUniqe(const ResolvedProjectPtr &parentProject)
{
    QSet<QString> subProjectNames;
    QSet<ResolvedProjectPtr> projectsInNeedOfNameChange;
    foreach (const ResolvedProjectPtr &p, parentProject->subProjects) {
        if (subProjectNames.contains(p->name))
            projectsInNeedOfNameChange << p;
        else
            subProjectNames << p->name;
        makeSubProjectNamesUniqe(p);
    }
    while (!projectsInNeedOfNameChange.isEmpty()) {
        QSet<ResolvedProjectPtr>::Iterator it = projectsInNeedOfNameChange.begin();
        while (it != projectsInNeedOfNameChange.end()) {
            const ResolvedProjectPtr p = *it;
            p->name += QLatin1Char('_');
            if (!subProjectNames.contains(p->name)) {
                subProjectNames << p->name;
                it = projectsInNeedOfNameChange.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ProjectResolver::resolveTopLevelProject(Item *item, ProjectContext *projectContext)
{
    if (m_progressObserver)
        m_progressObserver->setMaximum(projectContext->loadResult->productInfos.count());
    const TopLevelProjectPtr project = TopLevelProject::create();
    project->setBuildConfiguration(m_buildConfiguration);
    project->buildDirectory = TopLevelProject::deriveBuildDirectory(m_buildRoot, project->id());
    projectContext->project = project;
    resolveProject(item, projectContext);
    project->usedEnvironment = m_engine->usedEnvironment();
    project->fileExistsResults = m_engine->fileExistsResults();
    project->environment = m_engine->environment();
    project->buildSystemFiles = m_engine->imports();
    makeSubProjectNamesUniqe(project);
    resolveProductDependencies(projectContext);
}

void ProjectResolver::resolveProject(Item *item, ProjectContext *projectContext)
{
    checkCancelation();

    projectContext->project->name = m_evaluator->stringValue(item, QLatin1String("name"));
    projectContext->project->location = item->location();
    if (projectContext->project->name.isEmpty())
        projectContext->project->name = FileInfo::baseName(item->location().fileName()); // FIXME: Must also be changed in item?
    projectContext->project->enabled
            = m_evaluator->boolValue(item, QLatin1String("condition"));
    if (!projectContext->project->enabled)
        return;

    projectContext->dummyModule = ResolvedModule::create();
    projectContext->dummyModule->jsImports = item->file()->jsImports();

    QVariantMap projectProperties;
    for (QMap<QString, PropertyDeclaration>::const_iterator it
                = item->propertyDeclarations().constBegin();
            it != item->propertyDeclarations().constEnd(); ++it) {
        if (it.value().flags.testFlag(PropertyDeclaration::PropertyNotAvailableInConfig))
            continue;
        const ValueConstPtr v = item->property(it.key());
        QBS_ASSERT(v && v->type() != Value::ItemValueType, continue);
        projectProperties.insert(it.key(), m_evaluator->property(item, it.key()).toVariant());
    }
    projectContext->project->setProjectProperties(projectProperties);

    ItemFuncMap mapping;
    mapping["Project"] = &ProjectResolver::resolveProject;
    mapping["SubProject"] = &ProjectResolver::resolveSubProject;
    mapping["Product"] = &ProjectResolver::resolveProduct;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Rule"] = &ProjectResolver::resolveRule;

    foreach (Item *child, item->children())
        callItemFunction(mapping, child, projectContext);

    foreach (const ResolvedProductPtr &product, projectContext->project->products)
        postProcess(product, projectContext);
}

void ProjectResolver::resolveSubProject(Item *item, ProjectResolver::ProjectContext *projectContext)
{
    ProjectContext subProjectContext = createProjectContext(projectContext);

    Item * const projectItem = item->child(QLatin1String("Project"));
    if (projectItem) {
        resolveProject(projectItem, &subProjectContext);
        return;
    }

    // No project item was found, which means the project was disabled.
    subProjectContext.project->enabled = false;
    Item * const propertiesItem = item->child(QLatin1String("Properties"));
    if (propertiesItem) {
        subProjectContext.project->name
                = m_evaluator->stringValue(propertiesItem, QLatin1String("name"));
    }
}

void ProjectResolver::resolveProduct(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    ProductContext productContext;
    m_productContext = &productContext;
    productContext.item = item;
    const QString productSourceDirectory = QFileInfo(item->file()->filePath()).absolutePath();
    item->setProperty(QLatin1String("sourceDirectory"),
                      VariantValue::create(productSourceDirectory));
    item->setProperty(QLatin1String("buildDirectory"), VariantValue::create(projectContext
            ->project->topLevelProject()->buildDirectory));
    ResolvedProductPtr product = ResolvedProduct::create();
    product->project = projectContext->project;
    m_productItemMap.insert(product, item);
    projectContext->project->products += product;
    productContext.product = product;
    product->name = m_evaluator->stringValue(item, QLatin1String("name"));
    if (product->name.isEmpty()) {
        product->name = FileInfo::completeBaseName(item->file()->filePath());
        item->setProperty("name", VariantValue::create(product->name));
    }
    m_logger.qbsTrace() << "[PR] resolveProduct " << product->name;
    ModuleLoader::overrideItemProperties(item, product->name, m_overriddenProperties);
    m_productsByName.insert(product->name, product);
    product->enabled = m_evaluator->boolValue(item, QLatin1String("condition"));
    product->additionalFileTags
            = m_evaluator->fileTagsValue(item, QLatin1String("additionalFileTags"));
    product->fileTags = m_evaluator->fileTagsValue(item, QLatin1String("type"));
    product->targetName = m_evaluator->stringValue(item, QLatin1String("targetName"));
    product->sourceDirectory = productSourceDirectory;
    product->destinationDirectory
            = m_evaluator->stringValue(item, QLatin1String("destinationDirectory"));
    product->location = item->location();
    product->properties = PropertyMapInternal::create();
    product->properties->setValue(createProductConfig());
    ModuleProperties::init(m_evaluator->scriptValue(item), product);

    QList<Item *> subItems = item->children();
    const ValuePtr filesProperty = item->property(QLatin1String("files"));
    if (filesProperty) {
        Item *fakeGroup = Item::create(item->pool());
        fakeGroup->setFile(item->file());
        fakeGroup->setLocation(item->location());
        fakeGroup->setScope(item);
        fakeGroup->setTypeName(QLatin1String("Group"));
        fakeGroup->setProperty(QLatin1String("name"), VariantValue::create(product->name));
        fakeGroup->setProperty(QLatin1String("files"), filesProperty);
        fakeGroup->setProperty(QLatin1String("excludeFiles"),
                               item->property(QLatin1String("excludeFiles")));
        fakeGroup->setProperty(QLatin1String("overrideTags"), VariantValue::create(false));
        m_builtins->setupItemForBuiltinType(fakeGroup);
        subItems.prepend(fakeGroup);
    }

    ItemFuncMap mapping;
    mapping["Depends"] = &ProjectResolver::ignoreItem;
    mapping["Rule"] = &ProjectResolver::resolveRule;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Transformer"] = &ProjectResolver::resolveTransformer;
    mapping["Group"] = &ProjectResolver::resolveGroup;
    mapping["Export"] = &ProjectResolver::resolveExport;
    mapping["Probe"] = &ProjectResolver::ignoreItem;

    foreach (Item *child, subItems)
        callItemFunction(mapping, child, projectContext);

    foreach (const Item::Module &module, item->modules())
        resolveModule(module.name, module.item, projectContext);

    m_productContext = 0;
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();
}

void ProjectResolver::resolveModule(const QStringList &moduleName, Item *item,
                                    ProjectContext *projectContext)
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
            += m_evaluator->fileTagsValue(item, "additionalProductFileTags");

    // TODO: instead of jsImports, we need the file context for both setup scripts separately.
    //       ATM the setup scripts must be in the first file of the inheritance chain.
    module->jsImports = item->file()->jsImports();
    module->jsExtensions = item->file()->jsExtensions();

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
    foreach (Item *child, item->children())
        callItemFunction(mapping, child, projectContext);

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

static bool isSomeModulePropertySet(Item *group)
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

void ProjectResolver::resolveGroup(Item *item, ProjectContext *projectContext)
{
    Q_UNUSED(projectContext);
    checkCancelation();
    PropertyMapPtr properties = m_productContext->product->properties;
    if (isSomeModulePropertySet(item)) {
        properties = PropertyMapInternal::create();
        properties->setValue(evaluateModuleValues(item));
    }

    QStringList files = m_evaluator->stringListValue(item, QLatin1String("files"));
    const QStringList fileTagsFilter
            = m_evaluator->stringListValue(item, QLatin1String("fileTagsFilter"));
    if (!fileTagsFilter.isEmpty()) {
        if (Q_UNLIKELY(!files.isEmpty()))
            throw ErrorInfo(Tr::tr("Group.files and Group.fileTagsFilters are exclusive."),
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
        throw ErrorInfo(Tr::tr("Group without files is not allowed."),
                    item->location());
    }
    QStringList patterns;
    QString prefix;
    for (int i = files.count(); --i >= 0;) {
        if (FileInfo::isPattern(files[i]))
            patterns.append(files.takeAt(i));
    }
    prefix = m_evaluator->stringValue(item, QLatin1String("prefix"));
    if (!prefix.isEmpty()) {
        for (int i = files.count(); --i >= 0;)
                files[i].prepend(prefix);
    }
    FileTags fileTags = m_evaluator->fileTagsValue(item, QLatin1String("fileTags"));
    bool overrideTags = m_evaluator->boolValue(item, QLatin1String("overrideTags"));

    GroupPtr group = ResolvedGroup::create();
    group->location = item->location();
    group->enabled = m_evaluator->boolValue(item, QLatin1String("condition"));

    if (!patterns.isEmpty()) {
        SourceWildCards::Ptr wildcards = SourceWildCards::create();
        wildcards->excludePatterns = m_evaluator->stringListValue(item, "excludeFiles");
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
    ErrorInfo fileError;
    foreach (const SourceArtifactConstPtr &a, group->files) {
        if (!FileInfo(a->absoluteFilePath).exists()) {
            fileError.append(Tr::tr("File '%1' does not exist.")
                         .arg(a->absoluteFilePath), item->property("files")->location());
        }
    }
    if (fileError.hasError())
        throw ErrorInfo(fileError);

    group->name = m_evaluator->stringValue(item, "name");
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

void ProjectResolver::resolveRule(Item *item, ProjectContext *projectContext)
{
    checkCancelation();

    if (!m_evaluator->boolValue(item, QLatin1String("condition")))
        return;

    RulePtr rule = Rule::create();

    // read artifacts
    bool hasAlwaysUpdatedArtifact = false;
    foreach (Item *child, item->children()) {
        if (Q_UNLIKELY(child->typeName() != QLatin1String("Artifact")))
            throw ErrorInfo(Tr::tr("'Rule' can only have children of type 'Artifact'."),
                               child->location());

        resolveRuleArtifact(rule, child, &hasAlwaysUpdatedArtifact);
    }

    if (Q_UNLIKELY(!hasAlwaysUpdatedArtifact))
        throw ErrorInfo(Tr::tr("At least one output artifact of a rule "
                           "must have alwaysUpdated set to true."),
                    item->location());

    const PrepareScriptPtr prepareScript = PrepareScript::create();
    JSSourceValuePtr value = item->sourceProperty(QLatin1String("prepare"));
    if (value) {
        prepareScript->script = sourceCodeAsFunction(value);
        prepareScript->location = value->location();
    }

    rule->jsImports = item->file()->jsImports();
    rule->jsExtensions = item->file()->jsExtensions();
    rule->script = prepareScript;
    rule->multiplex = m_evaluator->boolValue(item, QLatin1String("multiplex"));
    rule->inputs = m_evaluator->fileTagsValue(item, "inputs");
    rule->usings = m_evaluator->fileTagsValue(item, "usings");
    rule->auxiliaryInputs
            = m_evaluator->fileTagsValue(item, QLatin1String("auxiliaryInputs"));
    rule->explicitlyDependsOn = m_evaluator->fileTagsValue(item, "explicitlyDependsOn");
    rule->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    if (m_productContext)
        m_productContext->product->rules += rule;
    else
        projectContext->rules += rule;
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

void ProjectResolver::resolveRuleArtifact(const RulePtr &rule, Item *item,
                                          bool *hasAlwaysUpdatedArtifact)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts += artifact;
    artifact->fileName = verbatimValue(item, "fileName");
    artifact->fileTags = m_evaluator->fileTagsValue(item, "fileTags");
    artifact->alwaysUpdated = m_evaluator->boolValue(item, "alwaysUpdated");
    if (artifact->alwaysUpdated)
        *hasAlwaysUpdatedArtifact = true;

    StringListSet seenBindings;
    for (Item *obj = item; obj; obj = obj->prototype()) {
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
                                                 Item *item,
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

void ProjectResolver::resolveFileTagger(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    QSet<FileTaggerConstPtr> &fileTaggers = m_productContext
            ? m_productContext->product->fileTaggers : projectContext->fileTaggers;
    fileTaggers += FileTagger::create(QRegExp(m_evaluator->stringValue(item,"pattern")),
            m_evaluator->fileTagsValue(item, "fileTags"));
}

void ProjectResolver::resolveTransformer(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, "condition")) {
        m_logger.qbsTrace() << "[PR] transformer condition is false";
        return;
    }

    ResolvedTransformer::Ptr rtrafo = ResolvedTransformer::create();
    rtrafo->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    rtrafo->jsImports = item->file()->jsImports();
    rtrafo->jsExtensions = item->file()->jsExtensions();
    rtrafo->inputs = m_evaluator->stringListValue(item, "inputs");
    for (int i = 0; i < rtrafo->inputs.count(); ++i)
        rtrafo->inputs[i] = FileInfo::resolvePath(m_productContext->product->sourceDirectory, rtrafo->inputs.at(i));
    const PrepareScriptPtr transform = PrepareScript::create();
    JSSourceValueConstPtr value = item->sourceProperty(QLatin1String("prepare"));
    transform->script = sourceCodeAsFunction(value);
    transform->location = value ? value->location() : item->location();
    rtrafo->transform = transform;

    foreach (const Item *child, item->children()) {
        if (Q_UNLIKELY(child->typeName() != QLatin1String("Artifact")))
            throw ErrorInfo(Tr::tr("Transformer: wrong child type '%0'.").arg(child->typeName()));
        SourceArtifactPtr artifact = SourceArtifact::create();
        artifact->properties = m_productContext->product->properties;
        QString fileName = m_evaluator->stringValue(child, "fileName");
        if (Q_UNLIKELY(fileName.isEmpty()))
            throw ErrorInfo(Tr::tr("Artifact fileName must not be empty."));
        artifact->absoluteFilePath = FileInfo::resolvePath(m_productContext->product->topLevelProject()->buildDirectory,
                                                           fileName);
        artifact->fileTags = m_evaluator->fileTagsValue(child, "fileTags");
        if (artifact->fileTags.isEmpty())
            artifact->fileTags.insert(unknownFileTag());
        rtrafo->outputs += artifact;
    }

    m_productContext->product->transformers += rtrafo;
}

void ProjectResolver::resolveExport(Item *item, ProjectContext *projectContext)
{
    Q_UNUSED(projectContext);
    checkCancelation();
    const QString &productName = m_productContext->product->name;
    m_exports[productName] = evaluateModuleValues(item);
}

static void insertExportedConfig(const QString &usedProductName,
        const QVariantMap &exportedConfig,
        const PropertyMapPtr &propertyMap)
{
    QVariantMap properties = propertyMap->value();
    QVariant &modulesEntry = properties[QLatin1String("modules")];
    QVariantMap modules = modulesEntry.toMap();
    modules.insert(usedProductName, exportedConfig);
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
             usedProductInfo.usedProductsFromExportItem) {
        if (!usedProductNames.contains(usedProduct.name))
            productInfo->usedProducts  += usedProduct;
    }
    *productsAdded = (oldCount != productInfo->usedProducts.count());
}

void ProjectResolver::resolveProductDependencies(ProjectContext *projectContext)
{
    // Collect product dependencies from Export items.
    bool productDependenciesAdded;
    QList<ResolvedProductPtr> allProducts = projectContext->project->allProducts();
    do {
        productDependenciesAdded = false;
        foreach (const ResolvedProductPtr &rproduct, allProducts) {
            if (!rproduct->enabled)
                continue;
            Item *productItem = m_productItemMap.value(rproduct);
            ModuleLoaderResult::ProductInfo &productInfo
                    = projectContext->loadResult->productInfos[productItem];
            foreach (const ModuleLoaderResult::ProductInfo::Dependency &dependency,
                        productInfo.usedProducts) {
                ResolvedProductPtr usedProduct
                        = m_productsByName.value(dependency.name);
                if (Q_UNLIKELY(!usedProduct))
                    throw ErrorInfo(Tr::tr("Product dependency '%1' not found.").arg(dependency.name),
                                productItem->location());
                Item *usedProductItem = m_productItemMap.value(usedProduct);
                const ModuleLoaderResult::ProductInfo usedProductInfo
                        = projectContext->loadResult->productInfos.value(usedProductItem);
                bool added;
                addUsedProducts(&productInfo, usedProductInfo, &added);
                if (added)
                    productDependenciesAdded = true;
            }
        }
    } while (productDependenciesAdded);

    // Resolve all inter-product dependencies.
    foreach (const ResolvedProductPtr &rproduct, allProducts) {
        if (!rproduct->enabled)
            continue;
        Item *productItem = m_productItemMap.value(rproduct);
        foreach (const ModuleLoaderResult::ProductInfo::Dependency &dependency,
                 projectContext->loadResult->productInfos.value(productItem).usedProducts) {
            const QString &usedProductName = dependency.name;
            ResolvedProductPtr usedProduct = m_productsByName.value(usedProductName);
            if (Q_UNLIKELY(!usedProduct))
                throw ErrorInfo(Tr::tr("Product dependency '%1' not found.").arg(usedProductName),
                            productItem->location());
            rproduct->dependencies.insert(usedProduct);

            // insert the configuration of the Export item into the product's configuration
            const QVariantMap exportedConfig = m_exports.value(usedProductName);
            if (exportedConfig.isEmpty())
                continue;

            insertExportedConfig(usedProductName, exportedConfig, rproduct->properties);

            // insert the configuration of the Export item into the artifact configurations
            foreach (SourceArtifactPtr artifact, rproduct->allEnabledFiles()) {
                if (artifact->properties != rproduct->properties)
                    insertExportedConfig(usedProductName, exportedConfig,
                                              artifact->properties);
            }
        }
    }
}

void ProjectResolver::postProcess(const ResolvedProductPtr &product,
                                  ProjectContext *projectContext) const
{
    product->fileTaggers += projectContext->fileTaggers;
    foreach (const RulePtr &rule, projectContext->rules)
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

QVariantMap ProjectResolver::evaluateModuleValues(Item *item) const
{
    QVariantMap modules;
    evaluateModuleValues(item, &modules);
    QVariantMap result;
    result[QLatin1String("modules")] = modules;
    return result;
}

void ProjectResolver::evaluateModuleValues(Item *item, QVariantMap *modulesMap) const
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

QVariantMap ProjectResolver::evaluateProperties(Item *item) const
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt);
}

QVariantMap ProjectResolver::evaluateProperties(Item *item,
                                                Item *propertiesContainer,
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
            for (Item *obj = item; obj; obj = obj->prototype()) {
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
                throw ErrorInfo(scriptValue.toString(), it.value()->location());
            QVariant v = scriptValue.toVariant();
            if (pd.type == PropertyDeclaration::Path)
                v = convertPathProperty(v.toString(),
                                        m_productContext->product->sourceDirectory);
            else if (pd.type == PropertyDeclaration::PathList)
                v = convertPathListProperty(v.toStringList(),
                                            m_productContext->product->sourceDirectory);
            else if (pd.type == PropertyDeclaration::StringList)
                v = v.toStringList();
            result[it.key()] = v;
            break;
        }
        case Value::VariantValueType:
        {
            if (result.contains(it.key()))
                break;
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

void ProjectResolver::callItemFunction(const ItemFuncMap &mappings, Item *item,
                                       ProjectContext *projectContext)
{
    const QByteArray typeName = item->typeName().toLocal8Bit();
    ItemFuncPtr f = mappings.value(typeName);
    if (Q_UNLIKELY(!f)) {
        const QString msg = Tr::tr("Unexpected item type '%1'.");
        throw ErrorInfo(msg.arg(item->typeName()), item->location());
    }
    if (typeName == "Project") {
        ProjectContext subProjectContext = createProjectContext(projectContext);
        (this->*f)(item, &subProjectContext);
    } else {
        (this->*f)(item, projectContext);
    }
}

ProjectResolver::ProjectContext ProjectResolver::createProjectContext(ProjectContext *parentProjectContext) const
{
    ProjectContext subProjectContext;
    subProjectContext.project = ResolvedProject::create();
    parentProjectContext->project->subProjects += subProjectContext.project;
    subProjectContext.project->parentProject = parentProjectContext->project;
    subProjectContext.loadResult = parentProjectContext->loadResult;
    return subProjectContext;
}

} // namespace Internal
} // namespace qbs
