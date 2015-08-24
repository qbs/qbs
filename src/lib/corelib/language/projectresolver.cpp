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

#include "projectresolver.h"

#include "artifactproperties.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "language.h"
#include "modulemerger.h"
#include "propertymapinternal.h"
#include "resolvedfilecontext.h"
#include "scriptengine.h"
#include "value.h"

#include <jsextensions/moduleproperties.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QDir>
#include <QQueue>

#include <algorithm>
#include <set>

namespace qbs {
namespace Internal {

extern bool debugProperties;

static const FileTag unknownFileTag()
{
    static const FileTag tag("unknown-file-tag");
    return tag;
}

ProjectResolver::ProjectResolver(ModuleLoader *ldr,
        const Logger &logger)
    : m_evaluator(ldr->evaluator())
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

static void checkForDuplicateProductNames(const TopLevelProjectConstPtr &project)
{
    const QList<ResolvedProductPtr> allProducts = project->allProducts();
    for (int i = 0; i < allProducts.count(); ++i) {
        const ResolvedProductConstPtr product1 = allProducts.at(i);
        const QString productName = product1->uniqueName();
        for (int j = i + 1; j < allProducts.count(); ++j) {
            const ResolvedProductConstPtr product2 = allProducts.at(j);
            if (product2->uniqueName() == productName) {
                ErrorInfo error;
                error.append(Tr::tr("Duplicate product name '%1'.").arg(product1->name));
                error.append(Tr::tr("First product defined here."), product1->location);
                error.append(Tr::tr("Second product defined here."), product2->location);
                throw error;
            }
        }
    }
}

TopLevelProjectPtr ProjectResolver::resolve(ModuleLoaderResult &loadResult,
        const SetupProjectParameters &setupParameters)
{
    QBS_CHECK(FileInfo::isAbsolute(setupParameters.buildRoot()));
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[PR] resolving " << loadResult.root->file()->filePath();

    ProjectContext projectContext;
    projectContext.loadResult = &loadResult;
    m_setupParams = setupParameters;
    m_productContext = 0;
    m_moduleContext = 0;
    resolveTopLevelProject(loadResult.root, &projectContext);
    TopLevelProjectPtr top = projectContext.project.staticCast<TopLevelProject>();
    checkForDuplicateProductNames(top);
    top->buildSystemFiles.unite(loadResult.qbsFiles);
    top->profileConfigs = loadResult.profileConfigs;
    return top;
}

void ProjectResolver::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled()) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration %1.")
                    .arg(TopLevelProject::deriveId(m_setupParams.topLevelProfile(),
                                                   m_setupParams.finalBuildConfigurationTree())));
    }
}

QString ProjectResolver::verbatimValue(const ValueConstPtr &value, bool *propertyWasSet) const
{
    QString result;
    if (value && value->type() == Value::JSSourceValueType) {
        const JSSourceValueConstPtr sourceValue = value.staticCast<const JSSourceValue>();
        result = sourceValue->sourceCodeForEvaluation();
        if (propertyWasSet)
            *propertyWasSet = (result != QLatin1String("undefined"));
    } else {
        if (propertyWasSet)
            *propertyWasSet = false;
    }
    return result;
}

QString ProjectResolver::verbatimValue(Item *item, const QString &name, bool *propertyWasSet) const
{
    return verbatimValue(item->property(name), propertyWasSet);
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
    project->buildDirectory = TopLevelProject::deriveBuildDirectory(m_setupParams.buildRoot(),
            TopLevelProject::deriveId(m_setupParams.topLevelProfile(),
                                      m_setupParams.finalBuildConfigurationTree()));
    projectContext->project = project;
    resolveProject(item, projectContext);
    project->setBuildConfiguration(m_setupParams.finalBuildConfigurationTree());
    project->usedEnvironment = m_engine->usedEnvironment();
    project->canonicalFilePathResults = m_engine->canonicalFilePathResults();
    project->fileExistsResults = m_engine->fileExistsResults();
    project->fileLastModifiedResults = m_engine->fileLastModifiedResults();
    project->environment = m_engine->environment();
    project->buildSystemFiles = m_engine->imports();
    makeSubProjectNamesUniqe(project);
    resolveProductDependencies(projectContext);

    foreach (const ResolvedProductPtr &product, project->allProducts()) {
        if (!product->enabled)
            continue;

        applyFileTaggers(product);
        matchArtifactProperties(product, product->allEnabledFiles());

        // Let a positive value of qbs.install imply the file tag "installable".
        foreach (const SourceArtifactPtr &artifact, product->allFiles()) {
            if (artifact->properties->qbsPropertyValue(QLatin1String("install")).toBool())
                artifact->fileTags += "installable";
        }
    }
}

void ProjectResolver::resolveProject(Item *item, ProjectContext *projectContext)
{
    checkCancelation();

    projectContext->project->name = m_evaluator->stringValue(item, QLatin1String("name"));
    projectContext->project->location = item->location();
    if (projectContext->project->name.isEmpty())
        projectContext->project->name = FileInfo::baseName(item->location().filePath()); // FIXME: Must also be changed in item?
    projectContext->project->enabled
            = m_evaluator->boolValue(item, QLatin1String("condition"));
    QVariantMap projectProperties;
    if (!projectContext->project->enabled) {
        projectProperties.insert(QLatin1String("profile"),
                                 m_evaluator->stringValue(item, QLatin1String("profile")));
        projectContext->project->setProjectProperties(projectProperties);
        return;
    }

    projectContext->dummyModule = ResolvedModule::create();

    for (Item::PropertyDeclarationMap::const_iterator it
                = item->propertyDeclarations().constBegin();
            it != item->propertyDeclarations().constEnd(); ++it) {
        if (it.value().flags().testFlag(PropertyDeclaration::PropertyNotAvailableInConfig))
            continue;
        const ValueConstPtr v = item->property(it.key());
        QBS_ASSERT(v && v->type() != Value::ItemValueType, continue);
        projectProperties.insert(it.key(), m_evaluator->value(item, it.key()).toVariant());
    }
    projectContext->project->setProjectProperties(projectProperties);

    ItemFuncMap mapping;
    mapping["Project"] = &ProjectResolver::resolveProject;
    mapping["SubProject"] = &ProjectResolver::resolveSubProject;
    mapping["Product"] = &ProjectResolver::resolveProduct;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Rule"] = &ProjectResolver::resolveRule;
    mapping["PropertyOptions"] = &ProjectResolver::ignoreItem;

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

class ModuleNameEquals
{
    QString m_str;
public:
    ModuleNameEquals(const QString &str)
        : m_str(str)
    {}

    bool operator()(const Item::Module &module)
    {
        return module.name.count() == 1 && module.name.first() == m_str;
    }
};

void ProjectResolver::resolveProduct(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    ProductContext productContext;
    m_productContext = &productContext;
    productContext.item = item;
    ResolvedProductPtr product = ResolvedProduct::create();
    product->project = projectContext->project;
    m_productItemMap.insert(product, item);
    projectContext->project->products += product;
    productContext.product = product;
    product->name = m_evaluator->stringValue(item, QLatin1String("name"));

    // product->buildDirectory() isn't valid yet, because the productProperties map is not ready.
    productContext.buildDirectory = m_evaluator->stringValue(item, QLatin1String("buildDirectory"));
    product->profile = m_evaluator->stringValue(item, QLatin1String("profile"));
    QBS_CHECK(!product->profile.isEmpty());
    m_logger.qbsTrace() << "[PR] resolveProduct " << product->uniqueName();

    if (std::find_if(item->modules().begin(), item->modules().end(),
            ModuleNameEquals(product->name)) != item->modules().end()) {
        throw ErrorInfo(
                    Tr::tr("The product name '%1' collides with a module name.").arg(product->name),
                    item->location());
    }

    product->enabled = m_evaluator->boolValue(item, QLatin1String("condition"));
    product->fileTags = m_evaluator->fileTagsValue(item, QLatin1String("type"));
    product->targetName = m_evaluator->stringValue(item, QLatin1String("targetName"));
    product->sourceDirectory = m_evaluator->stringValue(item, QLatin1String("sourceDirectory"));
    const QString destDirKey = QLatin1String("destinationDirectory");
    product->destinationDirectory = m_evaluator->stringValue(item, destDirKey);

    if (product->destinationDirectory.isEmpty()) {
        product->destinationDirectory = productContext.buildDirectory;
    } else {
        product->destinationDirectory = FileInfo::resolvePath(
                    product->topLevelProject()->buildDirectory,
                    product->destinationDirectory);
    }
    product->location = item->location();
    product->productProperties = createProductConfig();
    product->productProperties.insert(destDirKey, product->destinationDirectory);
    QVariantMap moduleProperties;
    moduleProperties.insert(QLatin1String("modules"),
                            product->productProperties.take(QLatin1String("modules")));
    product->moduleProperties = PropertyMapInternal::create();
    product->moduleProperties->setValue(moduleProperties);
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
        fakeGroup->setupForBuiltinType(m_logger);
        subItems.prepend(fakeGroup);
    }

    ItemFuncMap mapping;
    mapping["Depends"] = &ProjectResolver::ignoreItem;
    mapping["Rule"] = &ProjectResolver::resolveRule;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Transformer"] = &ProjectResolver::resolveTransformer;
    mapping["Group"] = &ProjectResolver::resolveGroup;
    mapping["Export"] = &ProjectResolver::ignoreItem;
    mapping["Probe"] = &ProjectResolver::ignoreItem;
    mapping["PropertyOptions"] = &ProjectResolver::ignoreItem;

    foreach (Item *child, subItems)
        callItemFunction(mapping, child, projectContext);

    resolveModules(item, projectContext);
    product->fileTags += productContext.additionalFileTags;

    foreach (const ResolvedTransformerPtr &transformer, product->transformers)
        matchArtifactProperties(product, transformer->outputs);

    m_productsByName.insert(product->uniqueName(), product);
    foreach (const FileTag &t, product->fileTags)
        m_productsByType[t.toString()] << product;

    m_productContext = 0;
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();
}

void ProjectResolver::resolveModules(const Item *item, ProjectContext *projectContext)
{
    // Breadth first search needed here, because the product might set properties on the cpp module,
    // whose children must be evaluated in that context then.
    QQueue<Item::Module> modules;
    foreach (const Item::Module &m, item->modules())
        modules.enqueue(m);
    QSet<QStringList> seen;
    while (!modules.isEmpty()) {
        const Item::Module m = modules.takeFirst();
        if (seen.contains(m.name))
            continue;
        seen.insert(m.name);
        resolveModule(m.name, m.item, m.isProduct, projectContext);
        foreach (const Item::Module &childModule, m.item->modules())
            modules.enqueue(childModule);
    }
    std::sort(m_productContext->product->modules.begin(), m_productContext->product->modules.end(),
              [](const ResolvedModuleConstPtr &m1, const ResolvedModuleConstPtr &m2) {
                      return m1->name < m2->name;
              });
}

void ProjectResolver::resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                                    ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, QLatin1String("present")))
        return;

    if (m_productContext->product->enabled && item->delayedError().hasError())
        throw item->delayedError();

    ModuleContext * const oldModuleContext = m_moduleContext;
    ModuleContext moduleContext;
    moduleContext.module = ResolvedModule::create();
    m_moduleContext = &moduleContext;

    const ResolvedModulePtr &module = moduleContext.module;
    module->name = moduleName.toString();
    module->setupBuildEnvironmentScript = scriptFunctionValue(item,
                                                            QLatin1String("setupBuildEnvironment"));
    module->setupRunEnvironmentScript = scriptFunctionValue(item,
                                                            QLatin1String("setupRunEnvironment"));

    m_productContext->additionalFileTags +=
            m_evaluator->fileTagsValue(item, QLatin1String("additionalProductTypes"));

    foreach (const Item::Module &m, item->modules()) {
        if (m_evaluator->boolValue(m.item, QLatin1String("present")))
            module->moduleDependencies += m.name.toString();
    }

    if (!isProduct)
        m_productContext->product->modules += module;

    ItemFuncMap mapping;
    mapping["Group"] = &ProjectResolver::ignoreItem;
    mapping["Rule"] = &ProjectResolver::resolveRule;
    mapping["FileTagger"] = &ProjectResolver::resolveFileTagger;
    mapping["Transformer"] = &ProjectResolver::resolveTransformer;
    mapping["Scanner"] = &ProjectResolver::resolveScanner;
    mapping["PropertyOptions"] = &ProjectResolver::ignoreItem;
    mapping["Depends"] = &ProjectResolver::ignoreItem;
    mapping["Probe"] = &ProjectResolver::ignoreItem;
    foreach (Item *child, item->children())
        callItemFunction(mapping, child, projectContext);

    m_moduleContext = oldModuleContext;
}

SourceArtifactPtr ProjectResolver::createSourceArtifact(const ResolvedProductConstPtr &rproduct,
        const PropertyMapPtr &properties, const QString &fileName, const FileTags &fileTags,
        bool overrideTags, QList<SourceArtifactPtr> &artifactList)
{
    SourceArtifactPtr artifact = SourceArtifactInternal::create();
    artifact->absoluteFilePath = FileInfo::resolvePath(rproduct->sourceDirectory, fileName);
    artifact->absoluteFilePath = QDir::cleanPath(artifact->absoluteFilePath); // Potentially necessary for groups with prefixes.
    artifact->fileTags = fileTags;
    artifact->overrideFileTags = overrideTags;
    artifact->properties = properties;
    artifactList += artifact;
    return artifact;
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
    PropertyMapPtr moduleProperties = m_productContext->product->moduleProperties;
    if (isSomeModulePropertySet(item)) {
        moduleProperties = PropertyMapInternal::create();
        m_evaluator->setCachingEnabled(true);
        moduleProperties->setValue(evaluateModuleValues(item));
        m_evaluator->setCachingEnabled(false);
    }

    const bool isEnabled = m_evaluator->boolValue(item, QLatin1String("condition"));
    QStringList files = m_evaluator->stringListValue(item, QLatin1String("files"));
    const QStringList fileTagsFilter
            = m_evaluator->stringListValue(item, QLatin1String("fileTagsFilter"));
    if (!fileTagsFilter.isEmpty()) {
        if (Q_UNLIKELY(!files.isEmpty()))
            throw ErrorInfo(Tr::tr("Group.files and Group.fileTagsFilters are exclusive."),
                        item->location());

        ProductContext::ArtifactPropertiesInfo apinfo
                = m_productContext->artifactPropertiesPerFilter.value(fileTagsFilter);
        if (apinfo.first) {
            if (apinfo.second.filePath() == item->location().filePath()) {
                ErrorInfo error(Tr::tr("Conflicting fileTagsFilter in Group items."));
                error.append(Tr::tr("First item"), apinfo.second);
                error.append(Tr::tr("Second item"), item->location());
                throw error;
            }

            // Discard any Group with the same fileTagsFilter that was defined in a base file.
            m_productContext->product->artifactProperties.removeAll(apinfo.first);
        }
        if (!isEnabled)
            return;
        ArtifactPropertiesPtr aprops = ArtifactProperties::create();
        aprops->setFileTagsFilter(FileTags::fromStringList(fileTagsFilter));
        aprops->setPropertyMapInternal(moduleProperties);
        m_productContext->product->artifactProperties += aprops;
        m_productContext->artifactPropertiesPerFilter.insert(fileTagsFilter,
                                ProductContext::ArtifactPropertiesInfo(aprops, item->location()));
        return;
    }
    if (Q_UNLIKELY(files.isEmpty() && !item->hasProperty(QLatin1String("files")))) {
        // Yield an error if Group without files binding is encountered.
        // An empty files value is OK but a binding must exist.
        throw ErrorInfo(Tr::tr("Group without files is not allowed."),
                    item->location());
    }
    QStringList patterns;
    for (int i = files.count(); --i >= 0;) {
        if (FileInfo::isPattern(files[i]))
            patterns.append(files.takeAt(i));
    }
    GroupPtr group = ResolvedGroup::create();
    group->prefix = m_evaluator->stringValue(item, QLatin1String("prefix"));
    if (!group->prefix.isEmpty()) {
        for (int i = files.count(); --i >= 0;)
                files[i].prepend(group->prefix);
    }
    group->location = item->location();
    group->enabled = isEnabled;
    bool fileTagsSet;
    group->fileTags = m_evaluator->fileTagsValue(item, QLatin1String("fileTags"), &fileTagsSet);
    group->overrideTags = m_evaluator->boolValue(item, QLatin1String("overrideTags"));
    if (group->overrideTags && group->fileTags.isEmpty() && fileTagsSet)
        group->fileTags.insert(unknownFileTag());

    if (!patterns.isEmpty()) {
        SourceWildCards::Ptr wildcards = SourceWildCards::create();
        wildcards->excludePatterns = m_evaluator->stringListValue(item,
                                                                  QLatin1String("excludeFiles"));
        wildcards->prefix = group->prefix;
        wildcards->patterns = patterns;
        QSet<QString> files = wildcards->expandPatterns(group, m_productContext->product->sourceDirectory);
        foreach (const QString &fileName, files)
            createSourceArtifact(m_productContext->product, moduleProperties, fileName,
                                 group->fileTags, group->overrideTags, wildcards->files);
        group->wildcards = wildcards;
    }

    foreach (const QString &fileName, files)
        createSourceArtifact(m_productContext->product, moduleProperties, fileName,
                             group->fileTags, group->overrideTags, group->files);
    ErrorInfo fileError;
    if (group->enabled) {
        const ValuePtr filesValue = item->property(QLatin1String("files"));
        foreach (const SourceArtifactConstPtr &a, group->allFiles()) {
            if (!FileInfo(a->absoluteFilePath).exists()) {
                fileError.append(Tr::tr("File '%1' does not exist.")
                                 .arg(a->absoluteFilePath),
                                 item->property(QLatin1String("files"))->location());
            }
            CodeLocation &loc = m_productContext->sourceArtifactLocations[a->absoluteFilePath];
            if (loc.isValid()) {
                fileError.append(Tr::tr("Duplicate source file '%1' at %2 and %3.")
                                 .arg(a->absoluteFilePath, loc.toString(),
                                      filesValue->location().toString()));
            }
            loc = filesValue->location();
        }
        if (fileError.hasError())
            throw ErrorInfo(fileError);
    }

    group->name = m_evaluator->stringValue(item, QLatin1String("name"));
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(m_productContext->product->groups.count());
    group->properties = moduleProperties;
    m_productContext->product->groups += group;
}

static QString sourceCodeAsFunction(const JSSourceValueConstPtr &value,
        const PropertyDeclaration &decl)
{
    const QString args = decl.functionArgumentNames().join(QLatin1Char(','));
    if (value->hasFunctionForm()) {
        // Insert the argument list.
        QString code = value->sourceCodeForEvaluation();
        code.insert(10, args);
        // Remove the function application "()" that has been
        // added in ItemReaderASTVisitor::visitStatement.
        return code.left(code.length() - 2);
    } else {
        return QLatin1String("(function(") + args + QLatin1String("){return ")
                + value->sourceCode().toString() + QLatin1String(";})");
    }
}

ScriptFunctionPtr ProjectResolver::scriptFunctionValue(Item *item, const QString &name) const
{
    ScriptFunctionPtr script = ScriptFunction::create();
    JSSourceValuePtr value = item->sourceProperty(name);
    if (value) {
        const PropertyDeclaration decl = item->propertyDeclaration(name);
        script->sourceCode = sourceCodeAsFunction(value, decl);
        script->argumentNames = decl.functionArgumentNames();
        script->location = value->location();
        script->fileContext = resolvedFileContext(value->file());
    }
    return script;
}

ResolvedFileContextPtr ProjectResolver::resolvedFileContext(const FileContextConstPtr &ctx) const
{
    ResolvedFileContextPtr &result = m_fileContextMap[ctx];
    if (!result)
        result = ResolvedFileContext::create(*ctx);
    return result;
}

void ProjectResolver::resolveRule(Item *item, ProjectContext *projectContext)
{
    checkCancelation();

    if (!m_evaluator->boolValue(item, QLatin1String("condition")))
        return;

    RulePtr rule = Rule::create();

    // read artifacts
    bool hasArtifactChildren = false;
    foreach (Item *child, item->children()) {
        if (Q_UNLIKELY(child->typeName() != QLatin1String("Artifact")))
            throw ErrorInfo(Tr::tr("'Rule' can only have children of type 'Artifact'."),
                               child->location());

        hasArtifactChildren = true;
        resolveRuleArtifact(rule, child);
    }

    rule->name = m_evaluator->stringValue(item, QLatin1String("name"));
    rule->prepareScript = scriptFunctionValue(item, QLatin1String("prepare"));
    rule->outputArtifactsScript = scriptFunctionValue(item, QLatin1String("outputArtifacts"));
    if (rule->outputArtifactsScript->isValid()) {
        if (hasArtifactChildren)
            throw ErrorInfo(Tr::tr("The Rule.outputArtifacts script is not allowed in rules "
                                   "that contain Artifact items."),
                        item->location());
        rule->outputFileTags = m_evaluator->fileTagsValue(item, QStringLiteral("outputFileTags"));
        if (rule->outputFileTags.isEmpty())
            throw ErrorInfo(Tr::tr("Rule.outputFileTags must be specified if "
                                   "Rule.outputArtifacts is specified."),
                            item->location());
    }

    rule->multiplex = m_evaluator->boolValue(item, QLatin1String("multiplex"));
    rule->inputs = m_evaluator->fileTagsValue(item, QLatin1String("inputs"));
    rule->inputsFromDependencies
            = m_evaluator->fileTagsValue(item, QLatin1String("inputsFromDependencies"));

    // TODO: Remove in 1.5.
    foreach (const FileTag &ft, m_evaluator->fileTagsValue(item, QLatin1String("usings")))
             rule->inputsFromDependencies << ft;

    rule->auxiliaryInputs
            = m_evaluator->fileTagsValue(item, QLatin1String("auxiliaryInputs"));
    rule->excludedAuxiliaryInputs
            = m_evaluator->fileTagsValue(item, QLatin1String("excludedAuxiliaryInputs"));
    rule->explicitlyDependsOn
            = m_evaluator->fileTagsValue(item, QLatin1String("explicitlyDependsOn"));
    rule->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    if (m_productContext)
        m_productContext->product->rules += rule;
    else
        projectContext->rules += rule;
}

class QualifiedIdSet : public std::set<QualifiedId>
{
public:
    typedef std::pair<iterator, bool> InsertResult;
};

void ProjectResolver::resolveRuleArtifact(const RulePtr &rule, Item *item)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts += artifact;
    artifact->location = item->location();

    artifact->filePath = verbatimValue(item, QLatin1String("filePath"));
    artifact->fileTags = m_evaluator->fileTagsValue(item, QLatin1String("fileTags"));
    artifact->alwaysUpdated = m_evaluator->boolValue(item, QLatin1String("alwaysUpdated"));

    QualifiedIdSet seenBindings;
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
                                                 QualifiedIdSet *seenBindings)
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
            const QualifiedIdSet::InsertResult insertResult = seenBindings->insert(name);
            if (!insertResult.second)
                continue;
            JSSourceValuePtr sourceValue = it.value().staticCast<JSSourceValue>();
            RuleArtifact::Binding rab;
            rab.name = name;
            rab.code = sourceValue->sourceCodeForEvaluation();
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
    QList<FileTaggerConstPtr> &fileTaggers = m_productContext
            ? m_productContext->product->fileTaggers
            : projectContext->fileTaggers;
    const QStringList patterns = m_evaluator->stringListValue(item, QLatin1String("patterns"));
    if (patterns.isEmpty())
        throw ErrorInfo(Tr::tr("FileTagger.patterns must be a non-empty list."), item->location());

    const FileTags fileTags = m_evaluator->fileTagsValue(item, QLatin1String("fileTags"));
    if (fileTags.isEmpty())
        throw ErrorInfo(Tr::tr("FileTagger.fileTags must not be empty."), item->location());

    foreach (const QString &pattern, patterns) {
        if (pattern.isEmpty())
            throw ErrorInfo(Tr::tr("A FileTagger pattern must not be empty."), item->location());
    }
    fileTaggers += FileTagger::create(patterns, fileTags);
}

void ProjectResolver::resolveTransformer(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, QLatin1String("condition"))) {
        m_logger.qbsTrace() << "[PR] transformer condition is false";
        return;
    }

    ResolvedTransformerPtr rtrafo = ResolvedTransformer::create();
    rtrafo->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    rtrafo->inputs = m_evaluator->stringListValue(item, QLatin1String("inputs"));
    for (int i = 0; i < rtrafo->inputs.count(); ++i)
        rtrafo->inputs[i] = FileInfo::resolvePath(m_productContext->product->sourceDirectory, rtrafo->inputs.at(i));
    rtrafo->transform = scriptFunctionValue(item, QLatin1String("prepare"));
    rtrafo->explicitlyDependsOn = m_evaluator->fileTagsValue(item,
                                                             QLatin1String("explicitlyDependsOn"));

    foreach (const Item *child, item->children()) {
        if (Q_UNLIKELY(child->typeName() != QLatin1String("Artifact")))
            throw ErrorInfo(Tr::tr("Transformer: wrong child type '%0'.").arg(child->typeName()));
        SourceArtifactPtr artifact = SourceArtifactInternal::create();
        artifact->properties = m_productContext->product->moduleProperties;
        QString filePath = m_evaluator->stringValue(child, QLatin1String("filePath"));
        if (Q_UNLIKELY(filePath.isEmpty()))
            throw ErrorInfo(Tr::tr("Artifact.filePath must not be empty."));
        artifact->absoluteFilePath
                = FileInfo::resolvePath(m_productContext->buildDirectory, filePath);
        artifact->fileTags = m_evaluator->fileTagsValue(child, QLatin1String("fileTags"));
        if (artifact->fileTags.isEmpty())
            artifact->fileTags.insert(unknownFileTag());
        rtrafo->outputs += artifact;
    }

    m_productContext->product->transformers += rtrafo;
}

void ProjectResolver::resolveScanner(Item *item, ProjectResolver::ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, QLatin1String("condition"))) {
        m_logger.qbsTrace() << "[PR] scanner condition is false";
        return;
    }

    ResolvedScannerPtr scanner = ResolvedScanner::create();
    scanner->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    scanner->inputs = m_evaluator->fileTagsValue(item, QLatin1String("inputs"));
    scanner->recursive = m_evaluator->boolValue(item, QLatin1String("recursive"));
    scanner->searchPathsScript = scriptFunctionValue(item, QLatin1String("searchPaths"));
    scanner->scanScript = scriptFunctionValue(item, QLatin1String("scan"));
    m_productContext->product->scanners += scanner;
}

QList<ResolvedProductPtr> ProjectResolver::getProductDependencies(const ResolvedProductConstPtr &product,
        ModuleLoaderResult::ProductInfo *productInfo)
{
    QList<ResolvedProductPtr> usedProducts;
    for (int i = productInfo->usedProducts.count() - 1; i >= 0; --i) {
        const ModuleLoaderResult::ProductInfo::Dependency &dependency
                = productInfo->usedProducts.at(i);
        QBS_CHECK(dependency.name.isEmpty() != dependency.productTypes.isEmpty());
        if (!dependency.productTypes.isEmpty()) {
            foreach (const QString &tag, dependency.productTypes) {
                const QList<ResolvedProductPtr> productsForTag = m_productsByType.value(tag);
                foreach (const ResolvedProductPtr &p, productsForTag) {
                    if (p == product || !p->enabled
                            || (dependency.limitToSubProject && !product->isInParentProject(p))) {
                        continue;
                    }
                    usedProducts << p;
                    ModuleLoaderResult::ProductInfo::Dependency newDependency;
                    newDependency.name = p->name;
                    newDependency.profile = p->profile;
                    productInfo->usedProducts << newDependency;
                }
            }
            productInfo->usedProducts.removeAt(i);
        } else if (dependency.profile == QLatin1String("*")) {
            foreach (const ResolvedProductPtr &p, m_productsByName) {
                if (p->name != dependency.name || p == product || !p->enabled
                        || (dependency.limitToSubProject && !product->isInParentProject(p))) {
                    continue;
                }
                usedProducts << p;
                ModuleLoaderResult::ProductInfo::Dependency newDependency;
                newDependency.name = p->name;
                newDependency.profile = p->profile;
                productInfo->usedProducts << newDependency;
            }
            productInfo->usedProducts.removeAt(i);
        } else {
            const ResolvedProductPtr &usedProduct
                    = m_productsByName.value(dependency.uniqueName());
            QBS_ASSERT(usedProduct, continue);
            usedProducts << usedProduct;
        }
    }
    return usedProducts;
}

void ProjectResolver::matchArtifactProperties(const ResolvedProductPtr &product,
        const QList<SourceArtifactPtr> &artifacts)
{
    foreach (const SourceArtifactPtr &artifact, artifacts) {
        foreach (const ArtifactPropertiesConstPtr &artifactProperties,
                 product->artifactProperties) {
            if (artifact->fileTags.matches(artifactProperties->fileTagsFilter()))
                artifact->properties = artifactProperties->propertyMap();
        }
    }
}

static bool hasDependencyCycle(QSet<ResolvedProduct *> *checked,
                               QSet<ResolvedProduct *> *branch,
                               const ResolvedProductPtr &product,
                               ErrorInfo *error)
{
    if (branch->contains(product.data()))
        return true;
    if (checked->contains(product.data()))
        return false;
    checked->insert(product.data());
    branch->insert(product.data());
    foreach (const ResolvedProductPtr &dep, product->dependencies) {
        if (hasDependencyCycle(checked, branch, dep, error)) {
            error->prepend(dep->name, dep->location);
            return true;
        }
    }
    branch->remove(product.data());
    return false;
}

void ProjectResolver::resolveProductDependencies(ProjectContext *projectContext)
{
    // Resolve all inter-product dependencies.
    QList<ResolvedProductPtr> allProducts = projectContext->project->allProducts();
    foreach (const ResolvedProductPtr &rproduct, allProducts) {
        if (!rproduct->enabled)
            continue;
        Item *productItem = m_productItemMap.value(rproduct);
        ModuleLoaderResult::ProductInfo &productInfo
                = projectContext->loadResult->productInfos[productItem];
        foreach (const ResolvedProductPtr &usedProduct,
                 getProductDependencies(rproduct, &productInfo)) {
            rproduct->dependencies.insert(usedProduct);
        }
    }

    // Check for cyclic dependencies.
    QSet<ResolvedProduct *> checked;
    foreach (const ResolvedProductPtr &rproduct, allProducts) {
        QSet<ResolvedProduct *> branch;
        ErrorInfo error;
        if (hasDependencyCycle(&checked, &branch, rproduct, &error)) {
            error.prepend(rproduct->name, rproduct->location);
            error.prepend(Tr::tr("Cyclic dependencies detected."));
            throw error;
        }
    }
}

void ProjectResolver::postProcess(const ResolvedProductPtr &product,
                                  ProjectContext *projectContext) const
{
    product->fileTaggers += projectContext->fileTaggers;
    foreach (const RulePtr &rule, projectContext->rules)
        product->rules += rule;
}

void ProjectResolver::applyFileTaggers(const ResolvedProductPtr &product) const
{
    foreach (const SourceArtifactPtr &artifact, product->allEnabledFiles())
        applyFileTaggers(artifact, product, m_logger);
}

void ProjectResolver::applyFileTaggers(const SourceArtifactPtr &artifact,
        const ResolvedProductConstPtr &product, const Logger &logger)
{
    if (!artifact->overrideFileTags || artifact->fileTags.isEmpty()) {
        const QString fileName = FileInfo::fileName(artifact->absoluteFilePath);
        const FileTags fileTags = product->fileTagsForFileName(fileName);
        artifact->fileTags.unite(fileTags);
        if (artifact->fileTags.isEmpty())
            artifact->fileTags.insert(unknownFileTag());
        if (logger.traceEnabled())
            logger.qbsTrace() << "[PR] adding file tags " << artifact->fileTags
                       << " to " << fileName;
    }
}

QVariantMap ProjectResolver::evaluateModuleValues(Item *item, bool lookupPrototype) const
{
    QVariantMap moduleValues;
    foreach (const Item::Module &module, item->modules()) {
        const QString fullName = module.name.toString();
        moduleValues[fullName] = evaluateProperties(module.item, lookupPrototype);
    }

    QVariantMap result;
    result[QLatin1String("modules")] = moduleValues;
    return result;
}

QVariantMap ProjectResolver::evaluateProperties(Item *item, bool lookupPrototype) const
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt, lookupPrototype);
}

QVariantMap ProjectResolver::evaluateProperties(Item *item, Item *propertiesContainer,
        const QVariantMap &tmplt, bool lookupPrototype) const
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
            const PropertyDeclaration pd = item->propertyDeclaration(it.key());
            if (pd.type() == PropertyDeclaration::Verbatim
                || pd.flags().testFlag(PropertyDeclaration::PropertyNotAvailableInConfig))
            {
                break;
            }

            const QScriptValue scriptValue = m_evaluator->property(item, it.key());
            if (Q_UNLIKELY(m_evaluator->engine()->hasErrorOrException(scriptValue))) {
                throw ErrorInfo(m_evaluator->engine()->lastErrorString(scriptValue),
                                it.value()->location());
            }

            // NOTE: Loses type information if scriptValue.isUndefined == true,
            //       as such QScriptValues become invalid QVariants.
            QVariant v = scriptValue.toVariant();

            if (pd.type() == PropertyDeclaration::Path)
                v = convertPathProperty(v.toString(),
                                        m_productContext->product->sourceDirectory);
            else if (pd.type() == PropertyDeclaration::PathList)
                v = convertPathListProperty(v.toStringList(),
                                            m_productContext->product->sourceDirectory);
            else if (pd.type() == PropertyDeclaration::StringList)
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
    return lookupPrototype && propertiesContainer->prototype()
            ? evaluateProperties(item, propertiesContainer->prototype(), result, true)
            : result;
}

QVariantMap ProjectResolver::createProductConfig() const
{
    foreach (const Item::Module &module, m_productContext->item->modules()) {
        ModuleMerger merger(m_logger, m_productContext->item, module.item, module.name);
        merger.start();
    }

    m_evaluator->setCachingEnabled(true);
    QVariantMap cfg = evaluateModuleValues(m_productContext->item);
    cfg = evaluateProperties(m_productContext->item, m_productContext->item, cfg);
    m_evaluator->setCachingEnabled(false);
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
    QBS_CHECK(f);
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
