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

#include "projectresolver.h"

#include "artifactproperties.h"
#include "builtindeclarations.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "language.h"
#include "propertymapinternal.h"
#include "resolvedfilecontext.h"
#include "scriptengine.h"
#include "value.h"

#include <jsextensions/jsextensions.h>
#include <jsextensions/moduleproperties.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/joblimits.h>
#include <tools/jsliterals.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/setupprojectparameters.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qregexp.h>

#include <algorithm>
#include <memory>
#include <queue>

namespace qbs {
namespace Internal {

extern bool debugProperties;

static const FileTag unknownFileTag()
{
    static const FileTag tag("unknown-file-tag");
    return tag;
}

struct ProjectResolver::ProjectContext
{
    ProjectContext *parentContext = nullptr;
    ResolvedProjectPtr project;
    std::vector<FileTaggerConstPtr> fileTaggers;
    std::vector<RulePtr> rules;
    JobLimits jobLimits;
    ResolvedModulePtr dummyModule;
};

struct ProjectResolver::ProductContext
{
    ResolvedProductPtr product;
    QString buildDirectory;
    Item *item = nullptr;
    using ArtifactPropertiesInfo = std::pair<ArtifactPropertiesPtr, std::vector<CodeLocation>>;
    QHash<QStringList, ArtifactPropertiesInfo> artifactPropertiesPerFilter;
    ProjectResolver::FileLocations sourceArtifactLocations;
    GroupConstPtr currentGroup;
};

struct ProjectResolver::ModuleContext
{
    ResolvedModulePtr module;
    JobLimits jobLimits;
};

class CancelException { };


ProjectResolver::ProjectResolver(Evaluator *evaluator, ModuleLoaderResult loadResult,
        SetupProjectParameters setupParameters, Logger &logger)
    : m_evaluator(evaluator)
    , m_logger(logger)
    , m_engine(m_evaluator->engine())
    , m_progressObserver(nullptr)
    , m_setupParams(std::move(setupParameters))
    , m_loadResult(std::move(loadResult))
{
    QBS_CHECK(FileInfo::isAbsolute(m_setupParams.buildRoot()));
}

ProjectResolver::~ProjectResolver() = default;

void ProjectResolver::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

static void checkForDuplicateProductNames(const TopLevelProjectConstPtr &project)
{
    const std::vector<ResolvedProductPtr> allProducts = project->allProducts();
    for (size_t i = 0; i < allProducts.size(); ++i) {
        const ResolvedProductConstPtr product1 = allProducts.at(i);
        const QString productName = product1->uniqueName();
        for (size_t j = i + 1; j < allProducts.size(); ++j) {
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

TopLevelProjectPtr ProjectResolver::resolve()
{
    TimedActivityLogger projectResolverTimer(m_logger, Tr::tr("ProjectResolver"),
                                             m_setupParams.logElapsedTime());
    qCDebug(lcProjectResolver) << "resolving" << m_loadResult.root->file()->filePath();

    m_productContext = nullptr;
    m_moduleContext = nullptr;
    m_elapsedTimeModPropEval = m_elapsedTimeAllPropEval = m_elapsedTimeGroups = 0;
    TopLevelProjectPtr tlp;
    try {
        tlp = resolveTopLevelProject();
        printProfilingInfo();
    } catch (const CancelException &) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration '%1'.")
                    .arg(TopLevelProject::deriveId(m_setupParams.finalBuildConfigurationTree())));
    }
    return tlp;
}

void ProjectResolver::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled())
        throw CancelException();
}

QString ProjectResolver::verbatimValue(const ValueConstPtr &value, bool *propertyWasSet) const
{
    QString result;
    if (value && value->type() == Value::JSSourceValueType) {
        const JSSourceValueConstPtr sourceValue = std::static_pointer_cast<const JSSourceValue>(
                    value);
        result = sourceCodeForEvaluation(sourceValue);
        if (propertyWasSet)
            *propertyWasSet = !sourceValue->isBuiltinDefaultValue();
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
    Set<QString> subProjectNames;
    Set<ResolvedProjectPtr> projectsInNeedOfNameChange;
    for (const ResolvedProjectPtr &p : qAsConst(parentProject->subProjects)) {
        if (!subProjectNames.insert(p->name).second)
            projectsInNeedOfNameChange << p;
        makeSubProjectNamesUniqe(p);
    }
    while (!projectsInNeedOfNameChange.empty()) {
        auto it = projectsInNeedOfNameChange.begin();
        while (it != projectsInNeedOfNameChange.end()) {
            const ResolvedProjectPtr p = *it;
            p->name += QLatin1Char('_');
            if (subProjectNames.insert(p->name).second) {
                it = projectsInNeedOfNameChange.erase(it);
            } else {
                ++it;
            }
        }
    }
}

TopLevelProjectPtr ProjectResolver::resolveTopLevelProject()
{
    if (m_progressObserver)
        m_progressObserver->setMaximum(m_loadResult.productInfos.size());
    const TopLevelProjectPtr project = TopLevelProject::create();
    project->buildDirectory = TopLevelProject::deriveBuildDirectory(m_setupParams.buildRoot(),
            TopLevelProject::deriveId(m_setupParams.finalBuildConfigurationTree()));
    project->buildSystemFiles = m_loadResult.qbsFiles;
    project->profileConfigs = m_loadResult.profileConfigs;
    project->probes = m_loadResult.projectProbes;
    project->moduleProviderInfo = m_loadResult.moduleProviderInfo;
    ProjectContext projectContext;
    projectContext.project = project;

    resolveProject(m_loadResult.root, &projectContext);
    ErrorInfo accumulatedErrors;
    for (const ErrorInfo &e : m_queuedErrors)
        appendError(accumulatedErrors, e);
    if (accumulatedErrors.hasError())
        throw accumulatedErrors;

    project->setBuildConfiguration(m_setupParams.finalBuildConfigurationTree());
    project->overriddenValues = m_setupParams.overriddenValues();
    project->canonicalFilePathResults = m_engine->canonicalFilePathResults();
    project->fileExistsResults = m_engine->fileExistsResults();
    project->directoryEntriesResults = m_engine->directoryEntriesResults();
    project->fileLastModifiedResults = m_engine->fileLastModifiedResults();
    project->environment = m_engine->environment();
    project->buildSystemFiles.unite(m_engine->imports());
    makeSubProjectNamesUniqe(project);
    resolveProductDependencies(projectContext);
    collectExportedProductDependencies();
    checkForDuplicateProductNames(project);

    for (const ResolvedProductPtr &product : project->allProducts()) {
        if (!product->enabled)
            continue;

        applyFileTaggers(product);
        matchArtifactProperties(product, product->allEnabledFiles());

        // Let a positive value of qbs.install imply the file tag "installable".
        for (const SourceArtifactPtr &artifact : product->allFiles()) {
            if (artifact->properties->qbsPropertyValue(StringConstants::installProperty()).toBool())
                artifact->fileTags += "installable";
        }
    }
    project->warningsEncountered = m_logger.warnings();
    return project;
}

void ProjectResolver::resolveProject(Item *item, ProjectContext *projectContext)
{
    checkCancelation();

    if (projectContext->parentContext)
        projectContext->project->enabled = projectContext->parentContext->project->enabled;
    projectContext->project->location = item->location();
    try {
        resolveProjectFully(item, projectContext);
    } catch (const ErrorInfo &error) {
        if (!projectContext->project->enabled) {
            qCDebug(lcProjectResolver) << "error resolving project"
                                       << projectContext->project->location << error.toString();
            return;
        }
        if (m_setupParams.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        m_logger.printWarning(error);
    }
}

void ProjectResolver::resolveProjectFully(Item *item, ProjectResolver::ProjectContext *projectContext)
{
    projectContext->project->enabled = projectContext->project->enabled
            && m_evaluator->boolValue(item, StringConstants::conditionProperty());
    projectContext->project->name = m_evaluator->stringValue(item, StringConstants::nameProperty());
    if (projectContext->project->name.isEmpty())
        projectContext->project->name = FileInfo::baseName(item->location().filePath()); // FIXME: Must also be changed in item?
    QVariantMap projectProperties;
    if (!projectContext->project->enabled) {
        projectProperties.insert(StringConstants::profileProperty(),
                                 m_evaluator->stringValue(item,
                                                          StringConstants::profileProperty()));
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

    static const ItemFuncMap mapping = {
        { ItemType::Project, &ProjectResolver::resolveProject },
        { ItemType::SubProject, &ProjectResolver::resolveSubProject },
        { ItemType::Product, &ProjectResolver::resolveProduct },
        { ItemType::Probe, &ProjectResolver::ignoreItem },
        { ItemType::FileTagger, &ProjectResolver::resolveFileTagger },
        { ItemType::JobLimit, &ProjectResolver::resolveJobLimit },
        { ItemType::Rule, &ProjectResolver::resolveRule },
        { ItemType::PropertyOptions, &ProjectResolver::ignoreItem }
    };

    for (Item * const child : item->children()) {
        try {
            callItemFunction(mapping, child, projectContext);
        } catch (const ErrorInfo &e) {
            m_queuedErrors.push_back(e);
        }
    }

    for (const ResolvedProductPtr &product : projectContext->project->products)
        postProcess(product, projectContext);
}

void ProjectResolver::resolveSubProject(Item *item, ProjectResolver::ProjectContext *projectContext)
{
    ProjectContext subProjectContext = createProjectContext(projectContext);

    Item * const projectItem = item->child(ItemType::Project);
    if (projectItem) {
        resolveProject(projectItem, &subProjectContext);
        return;
    }

    // No project item was found, which means the project was disabled.
    subProjectContext.project->enabled = false;
    Item * const propertiesItem = item->child(ItemType::PropertiesInSubProject);
    if (propertiesItem) {
        subProjectContext.project->name
                = m_evaluator->stringValue(propertiesItem, StringConstants::nameProperty());
    }
}

class ProjectResolver::ProductContextSwitcher
{
public:
    ProductContextSwitcher(ProjectResolver *resolver, ProductContext *newContext,
                           ProgressObserver *progressObserver)
        : m_resolver(resolver), m_progressObserver(progressObserver)
    {
        QBS_CHECK(!m_resolver->m_productContext);
        m_resolver->m_productContext = newContext;
    }

    ~ProductContextSwitcher()
    {
        if (m_progressObserver)
            m_progressObserver->incrementProgressValue();
        m_resolver->m_productContext = nullptr;
    }

private:
    ProjectResolver * const m_resolver;
    ProgressObserver * const m_progressObserver;
};

void ProjectResolver::resolveProduct(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    m_evaluator->clearPropertyDependencies();
    ProductContext productContext;
    productContext.item = item;
    ResolvedProductPtr product = ResolvedProduct::create();
    product->enabled = projectContext->project->enabled;
    product->moduleProperties = PropertyMapInternal::create();
    product->project = projectContext->project;
    productContext.product = product;
    product->location = item->location();
    ProductContextSwitcher contextSwitcher(this, &productContext, m_progressObserver);
    try {
        resolveProductFully(item, projectContext);
    } catch (const ErrorInfo &e) {
        QString mainErrorString = !product->name.isEmpty()
                ? Tr::tr("Error while handling product '%1':").arg(product->name)
                : Tr::tr("Error while handling product:");
        ErrorInfo fullError(mainErrorString, item->location());
        appendError(fullError, e);
        if (!product->enabled) {
            qCDebug(lcProjectResolver) << fullError.toString();
            return;
        }
        if (m_setupParams.productErrorMode() == ErrorHandlingMode::Strict)
            throw fullError;
        m_logger.printWarning(fullError);
        m_logger.printWarning(ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                                        .arg(product->name), item->location()));
        product->enabled = false;
    }
}

void ProjectResolver::resolveProductFully(Item *item, ProjectContext *projectContext)
{
    const ResolvedProductPtr product = m_productContext->product;
    m_productItemMap.insert(product, item);
    projectContext->project->products.push_back(product);
    product->name = m_evaluator->stringValue(item, StringConstants::nameProperty());

    // product->buildDirectory() isn't valid yet, because the productProperties map is not ready.
    m_productContext->buildDirectory
            = m_evaluator->stringValue(item, StringConstants::buildDirectoryProperty());
    product->multiplexConfigurationId
            = m_evaluator->stringValue(item, StringConstants::multiplexConfigurationIdProperty());
    qCDebug(lcProjectResolver) << "resolveProduct" << product->uniqueName();
    m_productsByName.insert(product->uniqueName(), product);
    product->enabled = product->enabled
            && m_evaluator->boolValue(item, StringConstants::conditionProperty());
    ModuleLoaderResult::ProductInfo &pi = m_loadResult.productInfos[item];
    if (pi.delayedError.hasError()) {
        ErrorInfo errorInfo;

        // First item is "main error", gets prepended again in the catch clause.
        const QList<ErrorItem> &items = pi.delayedError.items();
        for (int i = 1; i < items.size(); ++i)
            errorInfo.append(items.at(i));

        pi.delayedError.clear();
        throw errorInfo;
    }
    gatherProductTypes(product.get(), item);
    product->targetName = m_evaluator->stringValue(item, StringConstants::targetNameProperty());
    product->sourceDirectory = m_evaluator->stringValue(
                item, StringConstants::sourceDirectoryProperty());
    product->destinationDirectory = m_evaluator->stringValue(
                item, StringConstants::destinationDirProperty());

    if (product->destinationDirectory.isEmpty()) {
        product->destinationDirectory = m_productContext->buildDirectory;
    } else {
        product->destinationDirectory = FileInfo::resolvePath(
                    product->topLevelProject()->buildDirectory,
                    product->destinationDirectory);
    }
    product->probes = pi.probes;
    createProductConfig(product.get());
    product->productProperties.insert(StringConstants::destinationDirProperty(),
                                      product->destinationDirectory);
    ModuleProperties::init(m_evaluator->scriptValue(item), product.get());

    QList<Item *> subItems = item->children();
    const ValuePtr filesProperty = item->property(StringConstants::filesProperty());
    if (filesProperty) {
        Item *fakeGroup = Item::create(item->pool(), ItemType::Group);
        fakeGroup->setFile(item->file());
        fakeGroup->setLocation(item->location());
        fakeGroup->setScope(item);
        fakeGroup->setProperty(StringConstants::nameProperty(), VariantValue::create(product->name));
        fakeGroup->setProperty(StringConstants::filesProperty(), filesProperty);
        fakeGroup->setProperty(StringConstants::excludeFilesProperty(),
                               item->property(StringConstants::excludeFilesProperty()));
        fakeGroup->setProperty(StringConstants::overrideTagsProperty(),
                               VariantValue::falseValue());
        fakeGroup->setupForBuiltinType(m_logger);
        subItems.prepend(fakeGroup);
    }

    static const ItemFuncMap mapping = {
        { ItemType::Depends, &ProjectResolver::ignoreItem },
        { ItemType::Rule, &ProjectResolver::resolveRule },
        { ItemType::FileTagger, &ProjectResolver::resolveFileTagger },
        { ItemType::JobLimit, &ProjectResolver::resolveJobLimit },
        { ItemType::Group, &ProjectResolver::resolveGroup },
        { ItemType::Product, &ProjectResolver::resolveShadowProduct },
        { ItemType::Export, &ProjectResolver::resolveExport },
        { ItemType::Probe, &ProjectResolver::ignoreItem },
        { ItemType::PropertyOptions, &ProjectResolver::ignoreItem }
    };

    for (Item * const child : qAsConst(subItems))
        callItemFunction(mapping, child, projectContext);

    for (const ProjectContext *p = projectContext; p; p = p->parentContext) {
        JobLimits tempLimits = p->jobLimits;
        product->jobLimits = tempLimits.update(product->jobLimits);
    }

    resolveModules(item, projectContext);

    for (const FileTag &t : qAsConst(product->fileTags))
        m_productsByType[t].push_back(product);
}

void ProjectResolver::resolveModules(const Item *item, ProjectContext *projectContext)
{
    JobLimits jobLimits;
    for (const Item::Module &m : item->modules())
        resolveModule(m.name, m.item, m.isProduct, m.parameters, jobLimits, projectContext);
    for (int i = 0; i < jobLimits.count(); ++i) {
        const JobLimit &moduleJobLimit = jobLimits.jobLimitAt(i);
        if (m_productContext->product->jobLimits.getLimit(moduleJobLimit.pool()) == -1)
            m_productContext->product->jobLimits.setJobLimit(moduleJobLimit);
    }
}

void ProjectResolver::resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                                    const QVariantMap &parameters, JobLimits &jobLimits,
                                    ProjectContext *projectContext)
{
    checkCancelation();
    if (!item->isPresentModule())
        return;

    ModuleContext * const oldModuleContext = m_moduleContext;
    ModuleContext moduleContext;
    moduleContext.module = ResolvedModule::create();
    m_moduleContext = &moduleContext;

    const ResolvedModulePtr &module = moduleContext.module;
    module->name = moduleName.toString();
    module->isProduct = isProduct;
    module->product = m_productContext->product.get();
    module->setupBuildEnvironmentScript.initialize(
                scriptFunctionValue(item, StringConstants::setupBuildEnvironmentProperty()));
    module->setupRunEnvironmentScript.initialize(
                scriptFunctionValue(item, StringConstants::setupRunEnvironmentProperty()));

    for (const Item::Module &m : item->modules()) {
        if (m.item->isPresentModule())
            module->moduleDependencies += m.name.toString();
    }

    m_productContext->product->modules.push_back(module);
    if (!parameters.empty())
        m_productContext->product->moduleParameters[module] = parameters;

    static const ItemFuncMap mapping {
        { ItemType::Group, &ProjectResolver::ignoreItem },
        { ItemType::Rule, &ProjectResolver::resolveRule },
        { ItemType::FileTagger, &ProjectResolver::resolveFileTagger },
        { ItemType::JobLimit, &ProjectResolver::resolveJobLimit },
        { ItemType::Scanner, &ProjectResolver::resolveScanner },
        { ItemType::PropertyOptions, &ProjectResolver::ignoreItem },
        { ItemType::Depends, &ProjectResolver::ignoreItem },
        { ItemType::Parameter, &ProjectResolver::ignoreItem },
        { ItemType::Properties, &ProjectResolver::ignoreItem },
        { ItemType::Probe, &ProjectResolver::ignoreItem }
    };
    for (Item *child : item->children())
        callItemFunction(mapping, child, projectContext);
    for (int i = 0; i < moduleContext.jobLimits.count(); ++i) {
        const JobLimit &newJobLimit = moduleContext.jobLimits.jobLimitAt(i);
        const int oldLimit = jobLimits.getLimit(newJobLimit.pool());
        if (oldLimit == -1 || oldLimit > newJobLimit.limit())
            jobLimits.setJobLimit(newJobLimit);
    }

    m_moduleContext = oldModuleContext;
}

void ProjectResolver::gatherProductTypes(ResolvedProduct *product, Item *item)
{
    product->fileTags = m_evaluator->fileTagsValue(item, StringConstants::typeProperty());
    for (const Item::Module &m : item->modules()) {
        if (m.item->isPresentModule()) {
            product->fileTags += m_evaluator->fileTagsValue(m.item,
                    StringConstants::additionalProductTypesProperty());
        }
    }
    item->setProperty(StringConstants::typeProperty(),
                      VariantValue::create(sorted(product->fileTags.toStringList())));
}

SourceArtifactPtr ProjectResolver::createSourceArtifact(const ResolvedProductPtr &rproduct,
        const QString &fileName, const GroupPtr &group, bool wildcard,
        const CodeLocation &filesLocation, FileLocations *fileLocations,
        ErrorInfo *errorInfo)
{
    const QString &baseDir = FileInfo::path(group->location.filePath());
    const QString absFilePath = QDir::cleanPath(FileInfo::resolvePath(baseDir, fileName));
    if (!wildcard && !FileInfo(absFilePath).exists()) {
        if (errorInfo)
            errorInfo->append(Tr::tr("File '%1' does not exist.").arg(absFilePath), filesLocation);
        rproduct->missingSourceFiles << absFilePath;
        return {};
    }
    if (group->enabled && fileLocations) {
        CodeLocation &loc = (*fileLocations)[std::make_pair(group->targetOfModule, absFilePath)];
        if (loc.isValid()) {
            if (errorInfo) {
                errorInfo->append(Tr::tr("Duplicate source file '%1'.").arg(absFilePath));
                errorInfo->append(Tr::tr("First occurrence is here."), loc);
                errorInfo->append(Tr::tr("Next occurrence is here."), filesLocation);
            }
            return {};
        }
        loc = filesLocation;
    }
    SourceArtifactPtr artifact = SourceArtifactInternal::create();
    artifact->absoluteFilePath = absFilePath;
    artifact->fileTags = group->fileTags;
    artifact->overrideFileTags = group->overrideTags;
    artifact->properties = group->properties;
    artifact->targetOfModule = group->targetOfModule;
    (wildcard ? group->wildcards->files : group->files).push_back(artifact);
    return artifact;
}

static QualifiedIdSet propertiesToEvaluate(const QList<QualifiedId> &initialProps,
                                              const PropertyDependencies &deps)
{
    QList<QualifiedId> remainingProps = initialProps;
    QualifiedIdSet allProperties;
    while (!remainingProps.empty()) {
        const QualifiedId prop = remainingProps.takeFirst();
        const auto insertResult = allProperties.insert(prop);
        if (!insertResult.second)
            continue;
        for (const QualifiedId &directDep : deps.value(prop))
            remainingProps.push_back(directDep);
    }
    return allProperties;
}

QVariantMap ProjectResolver::resolveAdditionalModuleProperties(const Item *group,
                                                               const QVariantMap &currentValues)
{
    // Step 1: Retrieve the properties directly set in the group
    const ModulePropertiesPerGroup &mp = m_loadResult.productInfos.value(m_productContext->item)
            .modulePropertiesSetInGroups;
    const auto it = mp.find(group);
    if (it == mp.end())
        return {};
    const QualifiedIdSet &propsSetInGroup = it->second;

    // Step 2: Gather all properties that depend on these properties.
    const QualifiedIdSet &propsToEval
            = propertiesToEvaluate(propsSetInGroup.toList(), m_evaluator->propertyDependencies());

    // Step 3: Evaluate all these properties and replace their values in the map
    QVariantMap modulesMap = currentValues;
    QHash<QString, QStringList> propsPerModule;
    for (auto fullPropName : propsToEval) {
        const QString moduleName
                = QualifiedId(fullPropName.mid(0, fullPropName.size() - 1)).toString();
        propsPerModule[moduleName] << fullPropName.last();
    }
    EvalCacheEnabler cachingEnabler(m_evaluator);
    m_evaluator->setPathPropertiesBaseDir(m_productContext->product->sourceDirectory);
    for (const Item::Module &module : group->modules()) {
        const QString &fullModName = module.name.toString();
        const QStringList propsForModule = propsPerModule.take(fullModName);
        if (propsForModule.empty())
            continue;
        QVariantMap reusableValues = modulesMap.value(fullModName).toMap();
        for (const QString &prop : qAsConst(propsForModule))
            reusableValues.remove(prop);
        modulesMap.insert(fullModName,
                          evaluateProperties(module.item, module.item, reusableValues, true, true));
    }
    m_evaluator->clearPathPropertiesBaseDir();
    return modulesMap;
}

void ProjectResolver::resolveGroup(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    const bool parentEnabled = m_productContext->currentGroup
            ? m_productContext->currentGroup->enabled
            : m_productContext->product->enabled;
    const bool isEnabled = parentEnabled
            && m_evaluator->boolValue(item, StringConstants::conditionProperty());
    try {
        resolveGroupFully(item, projectContext, isEnabled);
    } catch (const ErrorInfo &error) {
        if (!isEnabled) {
            qCDebug(lcProjectResolver) << "error resolving group at" << item->location()
                                       << error.toString();
            return;
        }
        if (m_setupParams.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        m_logger.printWarning(error);
    }
}

void ProjectResolver::resolveGroupFully(Item *item, ProjectResolver::ProjectContext *projectContext,
                                        bool isEnabled)
{
    AccumulatingTimer groupTimer(m_setupParams.logElapsedTime()
                                       ? &m_elapsedTimeGroups : nullptr);

    const auto getGroupPropertyMap = [this, item](const ArtifactProperties *existingProps) {
        PropertyMapPtr moduleProperties;
        bool newPropertyMapRequired = false;
        if (existingProps)
            moduleProperties = existingProps->propertyMap();
        if (!moduleProperties) {
            newPropertyMapRequired = true;
            moduleProperties = m_productContext->currentGroup
                    ? m_productContext->currentGroup->properties
                    : m_productContext->product->moduleProperties;
        }
        const QVariantMap newModuleProperties
                = resolveAdditionalModuleProperties(item, moduleProperties->value());
        if (!newModuleProperties.empty()) {
            if (newPropertyMapRequired)
                moduleProperties = PropertyMapInternal::create();
            moduleProperties->setValue(newModuleProperties);
        }
        return moduleProperties;
    };

    QStringList files = m_evaluator->stringListValue(item, StringConstants::filesProperty());
    bool fileTagsSet;
    const FileTags fileTags = m_evaluator->fileTagsValue(item, StringConstants::fileTagsProperty(),
                                                         &fileTagsSet);
    const QStringList fileTagsFilter
            = m_evaluator->stringListValue(item, StringConstants::fileTagsFilterProperty());
    if (!fileTagsFilter.empty()) {
        if (Q_UNLIKELY(!files.empty()))
            throw ErrorInfo(Tr::tr("Group.files and Group.fileTagsFilters are exclusive."),
                        item->location());

        if (!isEnabled)
            return;

        ProductContext::ArtifactPropertiesInfo &apinfo
                = m_productContext->artifactPropertiesPerFilter[fileTagsFilter];
        if (apinfo.first) {
            const auto it = std::find_if(apinfo.second.cbegin(), apinfo.second.cend(),
                                         [item](const CodeLocation &loc) {
                return item->location().filePath() == loc.filePath();
            });
            if (it != apinfo.second.cend()) {
                ErrorInfo error(Tr::tr("Conflicting fileTagsFilter in Group items."));
                error.append(Tr::tr("First item"), *it);
                error.append(Tr::tr("Second item"), item->location());
                throw error;
            }
        } else {
            apinfo.first = ArtifactProperties::create();
            apinfo.first->setFileTagsFilter(FileTags::fromStringList(fileTagsFilter));
            m_productContext->product->artifactProperties.push_back(apinfo.first);
        }
        apinfo.second.push_back(item->location());
        apinfo.first->setPropertyMapInternal(getGroupPropertyMap(apinfo.first.get()));
        apinfo.first->addExtraFileTags(fileTags);
        return;
    }
    QStringList patterns;
    for (int i = files.size(); --i >= 0;) {
        if (FileInfo::isPattern(files[i]))
            patterns.push_back(files.takeAt(i));
    }
    GroupPtr group = ResolvedGroup::create();
    bool prefixWasSet = false;
    group->prefix = m_evaluator->stringValue(item, StringConstants::prefixProperty(), QString(),
                                             &prefixWasSet);
    if (!prefixWasSet && m_productContext->currentGroup)
        group->prefix = m_productContext->currentGroup->prefix;
    if (!group->prefix.isEmpty()) {
        for (auto it = files.rbegin(), end = files.rend(); it != end; ++it)
            it->prepend(group->prefix);
    }
    group->location = item->location();
    group->enabled = isEnabled;
    group->properties = getGroupPropertyMap(nullptr);
    group->fileTags = fileTags;
    group->overrideTags = m_evaluator->boolValue(item, StringConstants::overrideTagsProperty());
    if (group->overrideTags && fileTagsSet) {
        if (group->fileTags.empty() )
            group->fileTags.insert(unknownFileTag());
    } else if (m_productContext->currentGroup) {
        group->fileTags.unite(m_productContext->currentGroup->fileTags);
    }

    const CodeLocation filesLocation = item->property(StringConstants::filesProperty())->location();
    const VariantValueConstPtr moduleProp = item->variantProperty(
                StringConstants::modulePropertyInternal());
    if (moduleProp)
        group->targetOfModule = moduleProp->value().toString();
    ErrorInfo fileError;
    if (!patterns.empty()) {
        group->wildcards = std::make_unique<SourceWildCards>();
        SourceWildCards *wildcards = group->wildcards.get();
        wildcards->group = group.get();
        wildcards->excludePatterns = m_evaluator->stringListValue(
                    item, StringConstants::excludeFilesProperty());
        wildcards->patterns = patterns;
        const Set<QString> files = wildcards->expandPatterns(group,
                FileInfo::path(item->file()->filePath()),
                projectContext->project->topLevelProject()->buildDirectory);
        for (const QString &fileName : files)
            createSourceArtifact(m_productContext->product, fileName, group, true, filesLocation,
                                 &m_productContext->sourceArtifactLocations, &fileError);
    }

    for (const QString &fileName : qAsConst(files)) {
        createSourceArtifact(m_productContext->product, fileName, group, false, filesLocation,
                             &m_productContext->sourceArtifactLocations, &fileError);
    }
    if (fileError.hasError()) {
        if (group->enabled) {
            if (m_setupParams.productErrorMode() == ErrorHandlingMode::Strict)
                throw ErrorInfo(fileError);
            m_logger.printWarning(fileError);
        } else {
            qCDebug(lcProjectResolver) << "error for disabled group:" << fileError.toString();
        }
    }
    group->name = m_evaluator->stringValue(item, StringConstants::nameProperty());
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(m_productContext->product->groups.size());
    m_productContext->product->groups.push_back(group);

    class GroupContextSwitcher {
    public:
        GroupContextSwitcher(ProductContext &context, const GroupConstPtr &newGroup)
            : m_context(context), m_oldGroup(context.currentGroup) {
            m_context.currentGroup = newGroup;
        }
        ~GroupContextSwitcher() { m_context.currentGroup = m_oldGroup; }
    private:
        ProductContext &m_context;
        const GroupConstPtr m_oldGroup;
    };
    GroupContextSwitcher groupSwitcher(*m_productContext, group);
    for (Item * const childItem : item->children())
        resolveGroup(childItem, projectContext);
}

void ProjectResolver::adaptExportedPropertyValues(const Item *shadowProductItem)
{
    ExportedModule &m = m_productContext->product->exportedModule;
    const QVariantList prefixList = m.propertyValues.take(
                StringConstants::prefixMappingProperty()).toList();
    const QString shadowProductName = m_evaluator->stringValue(
                shadowProductItem, StringConstants::nameProperty());
    const QString shadowProductBuildDir = m_evaluator->stringValue(
                shadowProductItem, StringConstants::buildDirectoryProperty());
    QVariantMap prefixMap;
    for (const QVariant &v : prefixList) {
        const QVariantMap o = v.toMap();
        prefixMap.insert(o.value(QStringLiteral("prefix")).toString(),
                         o.value(QStringLiteral("replacement")).toString());
    }
    const auto valueRefersToImportingProduct
            = [shadowProductName, shadowProductBuildDir](const QString &value) {
        return value.toLower().contains(shadowProductName.toLower())
                || value.contains(shadowProductBuildDir);
    };
    static const auto stringMapper = [](const QVariantMap &mappings, const QString &value)
            -> QString {
        for (auto it = mappings.cbegin(); it != mappings.cend(); ++it) {
            if (value.startsWith(it.key()))
                return it.value().toString() + value.mid(it.key().size());
        }
        return value;
    };
    const auto stringListMapper = [&valueRefersToImportingProduct](
            const QVariantMap &mappings, const QStringList &value)  -> QStringList {
        QStringList result;
        result.reserve(value.size());
        for (const QString &s : value) {
            if (!valueRefersToImportingProduct(s))
                result.push_back(stringMapper(mappings, s));
        }
        return result;
    };
    const std::function<QVariant(const QVariantMap &, const QVariant &)> mapper
            = [&stringListMapper, &mapper](
            const QVariantMap &mappings, const QVariant &value) -> QVariant {
        switch (static_cast<QMetaType::Type>(value.type())) {
        case QMetaType::QString:
            return stringMapper(mappings, value.toString());
        case QMetaType::QStringList:
            return stringListMapper(mappings, value.toStringList());
        case QMetaType::QVariantMap: {
            QVariantMap m = value.toMap();
            for (auto it = m.begin(); it != m.end(); ++it)
                it.value() = mapper(mappings, it.value());
            return m;
        }
        default:
            return value;
        }
    };
    for (auto it = m.propertyValues.begin(); it != m.propertyValues.end(); ++it)
        it.value() = mapper(prefixMap, it.value());
    for (auto it = m.modulePropertyValues.begin(); it != m.modulePropertyValues.end(); ++it)
        it.value() = mapper(prefixMap, it.value());
    for (ExportedModuleDependency &dep : m.moduleDependencies) {
        for (auto it = dep.moduleProperties.begin(); it != dep.moduleProperties.end(); ++it)
            it.value() = mapper(prefixMap, it.value());
    }
}

void ProjectResolver::collectExportedProductDependencies()
{
    ResolvedProductPtr dummyProduct = ResolvedProduct::create();
    dummyProduct->enabled = false;
    for (const auto &exportingProductInfo : qAsConst(m_productExportInfo)) {
        const ResolvedProductPtr exportingProduct = exportingProductInfo.first;
        if (!exportingProduct->enabled)
            continue;
        Item * const importingProductItem = exportingProductInfo.second;
        std::vector<QString> directDepNames;
        for (const Item::Module &m : importingProductItem->modules()) {
            if (m.name.toString() == exportingProduct->name) {
                for (const Item::Module &dep : m.item->modules()) {
                    if (dep.isProduct)
                        directDepNames.push_back(dep.name.toString());
                }
                break;
            }
        }
        const ModuleLoaderResult::ProductInfo &importingProductInfo
                = m_loadResult.productInfos.value(importingProductItem);
        const ProductDependencyInfos &depInfos
                = getProductDependencies(dummyProduct, importingProductInfo);
        for (const auto &dep : depInfos.dependencies) {
            if (dep.product == exportingProduct)
                continue;

            // Filter out indirect dependencies.
            // TODO: Depends items using "profile" or "productTypes" will not work.
            if (!contains(directDepNames, dep.product->name))
                continue;

            if (!contains(exportingProduct->exportedModule.productDependencies,
                          dep.product->uniqueName())) {
                exportingProduct->exportedModule.productDependencies.push_back(
                            dep.product->uniqueName());
            }
            if (!dep.parameters.isEmpty()) {
                exportingProduct->exportedModule.dependencyParameters.insert(dep.product,
                                                                             dep.parameters);
            }
        }
        auto &productDeps = exportingProduct->exportedModule.productDependencies;
        std::sort(productDeps.begin(), productDeps.end());
    }
}

void ProjectResolver::resolveShadowProduct(Item *item, ProjectResolver::ProjectContext *)
{
    if (!m_productContext->product->enabled)
        return;
    for (const auto &m : item->modules()) {
        if (m.name.toString() != m_productContext->product->name)
            continue;
        collectPropertiesForExportItem(m.item);
        for (const auto &dep : m.item->modules())
            collectPropertiesForModuleInExportItem(dep);
        break;
    }
    try {
        adaptExportedPropertyValues(item);
    } catch (const ErrorInfo &) {}
    m_productExportInfo.emplace_back(m_productContext->product, item);
}

void ProjectResolver::setupExportedProperties(const Item *item, const QString &namePrefix,
                                              std::vector<ExportedProperty> &properties)
{
    const auto &props = item->properties();
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        const QString qualifiedName = namePrefix.isEmpty()
                ? it.key() : namePrefix + QLatin1Char('.') + it.key();
        if ((item->type() == ItemType::Export || item->type() == ItemType::Properties)
                && qualifiedName == StringConstants::prefixMappingProperty()) {
            continue;
        }
        const ValuePtr &v = it.value();
        if (v->type() == Value::ItemValueType) {
            setupExportedProperties(std::static_pointer_cast<ItemValue>(v)->item(),
                                    qualifiedName, properties);
            continue;
        }
        ExportedProperty exportedProperty;
        exportedProperty.fullName = qualifiedName;
        exportedProperty.type = item->propertyDeclaration(it.key()).type();
        if (v->type() == Value::VariantValueType) {
            exportedProperty.sourceCode = toJSLiteral(
                        std::static_pointer_cast<VariantValue>(v)->value());
        } else {
            QBS_CHECK(v->type() == Value::JSSourceValueType);
            const JSSourceValue * const sv = static_cast<JSSourceValue *>(v.get());
            exportedProperty.sourceCode = sv->sourceCode().toString();
        }
        const ItemDeclaration itemDecl
                = BuiltinDeclarations::instance().declarationsForType(item->type());
        PropertyDeclaration propertyDecl;
        const auto itemProperties = itemDecl.properties();
        for (const PropertyDeclaration &decl : itemProperties) {
            if (decl.name() == it.key()) {
                propertyDecl = decl;
                exportedProperty.isBuiltin = true;
                break;
            }
        }

        // Do not add built-in properties that were left at their default value.
        if (!exportedProperty.isBuiltin || m_evaluator->isNonDefaultValue(item, it.key()))
            properties.push_back(exportedProperty);
    }

    // Order the list of properties, so the output won't look so random.
    static const auto less = [](const ExportedProperty &p1, const ExportedProperty &p2) -> bool {
        const int p1ComponentCount = p1.fullName.count(QLatin1Char('.'));
        const int p2ComponentCount = p2.fullName.count(QLatin1Char('.'));
        if (p1.isBuiltin && !p2.isBuiltin)
            return true;
        if (!p1.isBuiltin && p2.isBuiltin)
            return false;
        if (p1ComponentCount < p2ComponentCount)
            return true;
        if (p1ComponentCount > p2ComponentCount)
            return false;
        return p1.fullName < p2.fullName;
    };
    std::sort(properties.begin(), properties.end(), less);
}

static bool usesImport(const ExportedProperty &prop, const QRegExp &regex)
{
    return regex.indexIn(prop.sourceCode) != -1;
}

static bool usesImport(const ExportedItem &item, const QRegExp &regex)
{
    return any_of(item.properties,
                  [regex](const ExportedProperty &p) { return usesImport(p, regex); })
            || any_of(item.children,
                      [regex](const ExportedItemPtr &child) { return usesImport(*child, regex); });
}

static bool usesImport(const ExportedModule &module, const QString &name)
{
    // Imports are used in three ways:
    // (1) var f = new TextFile(...);
    // (2) var path = FileInfo.joinPaths(...)
    // (3) var obj = DataCollection;
    const QString pattern = QStringLiteral("\\b%1\\b");

    const QRegExp regex(pattern.arg(name)); // std::regex is much slower
    return any_of(module.m_properties,
                  [regex](const ExportedProperty &p) { return usesImport(p, regex); })
            || any_of(module.children,
                      [regex](const ExportedItemPtr &child) { return usesImport(*child, regex); });
}

static QString getLineAtLocation(const CodeLocation &loc, const QString &content)
{
    int pos = 0;
    int currentLine = 1;
    while (currentLine < loc.line()) {
        while (content.at(pos++) != QLatin1Char('\n'))
            ;
        ++currentLine;
    }
    const int eolPos = content.indexOf(QLatin1Char('\n'), pos);
    return content.mid(pos, eolPos - pos);
}

void ProjectResolver::resolveExport(Item *exportItem, ProjectContext *)
{
    ExportedModule &exportedModule = m_productContext->product->exportedModule;
    setupExportedProperties(exportItem, QString(), exportedModule.m_properties);
    static const auto cmpFunc = [](const ExportedProperty &p1, const ExportedProperty &p2) {
        return p1.fullName < p2.fullName;
    };
    std::sort(exportedModule.m_properties.begin(), exportedModule.m_properties.end(), cmpFunc);
    for (const Item * const child : exportItem->children())
        exportedModule.children.push_back(resolveExportChild(child, exportedModule));
    for (const JsImport &jsImport : exportItem->file()->jsImports()) {
        if (usesImport(exportedModule, jsImport.scopeName)) {
            exportedModule.importStatements << getLineAtLocation(jsImport.location,
                                                                 exportItem->file()->content());
        }
    }
    const auto builtInImports = JsExtensions::extensionNames();
    for (const QString &builtinImport: builtInImports) {
        if (usesImport(exportedModule, builtinImport))
            exportedModule.importStatements << QStringLiteral("import qbs.") + builtinImport;
    }
    exportedModule.importStatements.sort();
}

// TODO: This probably wouldn't be necessary if we had item serialization.
std::unique_ptr<ExportedItem> ProjectResolver::resolveExportChild(const Item *item,
                                                                  const ExportedModule &module)
{
    std::unique_ptr<ExportedItem> exportedItem(new ExportedItem);

    // This is the type of the built-in base item. It may turn out that we need to support
    // derived items under Export. In that case, we probably need a new Item member holding
    // the original type name.
    exportedItem->name = item->typeName();

    for (const Item * const child : item->children())
        exportedItem->children.push_back(resolveExportChild(child, module));
    setupExportedProperties(item, QString(), exportedItem->properties);
    return exportedItem;
}


QString ProjectResolver::sourceCodeAsFunction(const JSSourceValueConstPtr &value,
                                              const PropertyDeclaration &decl) const
{
    QString &scriptFunction = m_scriptFunctions[std::make_pair(value->sourceCode(),
                                                          decl.functionArgumentNames())];
    if (!scriptFunction.isNull())
        return scriptFunction;
    const QString args = decl.functionArgumentNames().join(QLatin1Char(','));
    if (value->hasFunctionForm()) {
        // Insert the argument list.
        scriptFunction = value->sourceCodeForEvaluation();
        scriptFunction.insert(10, args);
        // Remove the function application "()" that has been
        // added in ItemReaderASTVisitor::visitStatement.
        scriptFunction.chop(2);
    } else {
        scriptFunction = QLatin1String("(function(") + args + QLatin1String("){return ")
                + value->sourceCode().toString() + QLatin1String(";})");
    }
    return scriptFunction;
}

QString ProjectResolver::sourceCodeForEvaluation(const JSSourceValueConstPtr &value) const
{
    QString &code = m_sourceCode[value->sourceCode()];
    if (!code.isNull())
        return code;
    code = value->sourceCodeForEvaluation();
    return code;
}

ScriptFunctionPtr ProjectResolver::scriptFunctionValue(Item *item, const QString &name) const
{
    JSSourceValuePtr value = item->sourceProperty(name);
    ScriptFunctionPtr &script = m_scriptFunctionMap[value ? value->location() : CodeLocation()];
    if (!script.get()) {
        script = ScriptFunction::create();
        const PropertyDeclaration decl = item->propertyDeclaration(name);
        script->sourceCode = sourceCodeAsFunction(value, decl);
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

    if (!m_evaluator->boolValue(item, StringConstants::conditionProperty()))
        return;

    RulePtr rule = Rule::create();

    // read artifacts
    bool hasArtifactChildren = false;
    for (Item * const child : item->children()) {
        if (Q_UNLIKELY(child->type() != ItemType::Artifact)) {
            throw ErrorInfo(Tr::tr("'Rule' can only have children of type 'Artifact'."),
                               child->location());
        }
        hasArtifactChildren = true;
        resolveRuleArtifact(rule, child);
    }

    rule->name = m_evaluator->stringValue(item, StringConstants::nameProperty());
    rule->prepareScript.initialize(scriptFunctionValue(item, StringConstants::prepareProperty()));
    rule->outputArtifactsScript.initialize(scriptFunctionValue(
                                               item, StringConstants::outputArtifactsProperty()));
    rule->outputFileTags = m_evaluator->fileTagsValue(
                item, StringConstants::outputFileTagsProperty());
    if (rule->outputArtifactsScript.isValid()) {
        if (hasArtifactChildren)
            throw ErrorInfo(Tr::tr("The Rule.outputArtifacts script is not allowed in rules "
                                   "that contain Artifact items."),
                        item->location());
    }
    if (!hasArtifactChildren && rule->outputFileTags.empty()) {
        throw ErrorInfo(Tr::tr("A rule needs to have Artifact items or a non-empty "
                               "outputFileTags property."), item->location());
    }
    rule->multiplex = m_evaluator->boolValue(item, StringConstants::multiplexProperty());
    rule->alwaysRun = m_evaluator->boolValue(item, StringConstants::alwaysRunProperty());
    rule->inputs = m_evaluator->fileTagsValue(item, StringConstants::inputsProperty());
    rule->inputsFromDependencies
            = m_evaluator->fileTagsValue(item, StringConstants::inputsFromDependenciesProperty());
    bool requiresInputsSet = false;
    rule->requiresInputs = m_evaluator->boolValue(item, StringConstants::requiresInputsProperty(),
                                                  &requiresInputsSet);
    if (!requiresInputsSet)
        rule->requiresInputs = rule->declaresInputs();
    rule->auxiliaryInputs
            = m_evaluator->fileTagsValue(item, StringConstants::auxiliaryInputsProperty());
    rule->excludedInputs
            = m_evaluator->fileTagsValue(item, StringConstants::excludedInputsProperty());
    if (rule->excludedInputs.empty()) {
        rule->excludedInputs = m_evaluator->fileTagsValue(
                    item, StringConstants::excludedAuxiliaryInputsProperty());
    }
    rule->explicitlyDependsOn
            = m_evaluator->fileTagsValue(item, StringConstants::explicitlyDependsOnProperty());
    rule->explicitlyDependsOnFromDependencies = m_evaluator->fileTagsValue(
                item, StringConstants::explicitlyDependsOnFromDependenciesProperty());
    rule->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    if (!rule->multiplex && !rule->declaresInputs()) {
        throw ErrorInfo(Tr::tr("Rule has no inputs, but is not a multiplex rule."),
                        item->location());
    }
    if (!rule->multiplex && !rule->requiresInputs) {
        throw ErrorInfo(Tr::tr("Rule.requiresInputs is false for non-multiplex rule."),
                        item->location());
    }
    if (!rule->declaresInputs() && rule->requiresInputs) {
        throw ErrorInfo(Tr::tr("Rule.requiresInputs is true, but the rule "
                               "does not declare any input tags."), item->location());
    }
    if (m_productContext) {
        rule->product = m_productContext->product.get();
        m_productContext->product->rules.push_back(rule);
    } else {
        projectContext->rules.push_back(rule);
    }
}

void ProjectResolver::resolveRuleArtifact(const RulePtr &rule, Item *item)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts.push_back(artifact);
    artifact->location = item->location();

    if (const auto sourceProperty = item->sourceProperty(StringConstants::filePathProperty()))
        artifact->filePathLocation = sourceProperty->location();

    artifact->filePath = verbatimValue(item, StringConstants::filePathProperty());
    artifact->fileTags = m_evaluator->fileTagsValue(item, StringConstants::fileTagsProperty());
    artifact->alwaysUpdated = m_evaluator->boolValue(item,
                                                     StringConstants::alwaysUpdatedProperty());

    QualifiedIdSet seenBindings;
    for (Item *obj = item; obj; obj = obj->prototype()) {
        for (QMap<QString, ValuePtr>::const_iterator it = obj->properties().constBegin();
             it != obj->properties().constEnd(); ++it)
        {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            resolveRuleArtifactBinding(artifact,
                                       std::static_pointer_cast<ItemValue>(it.value())->item(),
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
                                       std::static_pointer_cast<ItemValue>(it.value())->item(), name,
                                       seenBindings);
        } else if (it.value()->type() == Value::JSSourceValueType) {
            const auto insertResult = seenBindings->insert(name);
            if (!insertResult.second)
                continue;
            JSSourceValuePtr sourceValue = std::static_pointer_cast<JSSourceValue>(it.value());
            RuleArtifact::Binding rab;
            rab.name = name;
            rab.code = sourceCodeForEvaluation(sourceValue);
            rab.location = sourceValue->location();
            ruleArtifact->bindings.push_back(rab);
        } else {
            QBS_ASSERT(!"unexpected value type", continue);
        }
    }
}

void ProjectResolver::resolveFileTagger(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, StringConstants::conditionProperty()))
        return;
    std::vector<FileTaggerConstPtr> &fileTaggers = m_productContext
            ? m_productContext->product->fileTaggers
            : projectContext->fileTaggers;
    const QStringList patterns = m_evaluator->stringListValue(item,
                                                              StringConstants::patternsProperty());
    if (patterns.empty())
        throw ErrorInfo(Tr::tr("FileTagger.patterns must be a non-empty list."), item->location());

    const FileTags fileTags = m_evaluator->fileTagsValue(item, StringConstants::fileTagsProperty());
    if (fileTags.empty())
        throw ErrorInfo(Tr::tr("FileTagger.fileTags must not be empty."), item->location());

    for (const QString &pattern : patterns) {
        if (pattern.isEmpty())
            throw ErrorInfo(Tr::tr("A FileTagger pattern must not be empty."), item->location());
    }

    const int priority = m_evaluator->intValue(item, StringConstants::priorityProperty());
    fileTaggers.push_back(FileTagger::create(patterns, fileTags, priority));
}

void ProjectResolver::resolveJobLimit(Item *item, ProjectResolver::ProjectContext *projectContext)
{
    if (!m_evaluator->boolValue(item, StringConstants::conditionProperty()))
        return;
    const QString jobPool = m_evaluator->stringValue(item, StringConstants::jobPoolProperty());
    if (jobPool.isEmpty())
        throw ErrorInfo(Tr::tr("A JobLimit item needs to have a non-empty '%1' property.")
                        .arg(StringConstants::jobPoolProperty()), item->location());
    bool jobCountWasSet;
    const int jobCount = m_evaluator->intValue(item, StringConstants::jobCountProperty(), -1,
                                               &jobCountWasSet);
    if (!jobCountWasSet) {
        throw ErrorInfo(Tr::tr("A JobLimit item needs to have a '%1' property.")
                        .arg(StringConstants::jobCountProperty()), item->location());
    }
    if (jobCount < 0) {
        throw ErrorInfo(Tr::tr("A JobLimit item must have a non-negative '%1' property.")
                        .arg(StringConstants::jobCountProperty()), item->location());
    }
    JobLimits &jobLimits = m_moduleContext
            ? m_moduleContext->jobLimits
            : m_productContext ? m_productContext->product->jobLimits
            : projectContext->jobLimits;
    JobLimit jobLimit(jobPool, jobCount);
    const int oldLimit = jobLimits.getLimit(jobPool);
    if (oldLimit == -1 || oldLimit > jobCount)
        jobLimits.setJobLimit(jobLimit);
}

void ProjectResolver::resolveScanner(Item *item, ProjectResolver::ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, StringConstants::conditionProperty())) {
        qCDebug(lcProjectResolver) << "scanner condition is false";
        return;
    }

    ResolvedScannerPtr scanner = ResolvedScanner::create();
    scanner->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    scanner->inputs = m_evaluator->fileTagsValue(item, StringConstants::inputsProperty());
    scanner->recursive = m_evaluator->boolValue(item, StringConstants::recursiveProperty());
    scanner->searchPathsScript.initialize(scriptFunctionValue(
                                              item, StringConstants::searchPathsProperty()));
    scanner->scanScript.initialize(scriptFunctionValue(item, StringConstants::scanProperty()));
    m_productContext->product->scanners.push_back(scanner);
}

ProjectResolver::ProductDependencyInfos ProjectResolver::getProductDependencies(
        const ResolvedProductConstPtr &product, const ModuleLoaderResult::ProductInfo &productInfo)
{
    ProductDependencyInfos result;
    result.dependencies.reserve(productInfo.usedProducts.size());
    for (const auto &dependency : productInfo.usedProducts) {
        QBS_CHECK(!dependency.name.isEmpty());
        if (dependency.profile == StringConstants::star()) {
            for (const ResolvedProductPtr &p : qAsConst(m_productsByName)) {
                if (p->name != dependency.name || p == product || !p->enabled
                        || (dependency.limitToSubProject && !product->isInParentProject(p))) {
                    continue;
                }
                result.dependencies.emplace_back(p, dependency.parameters);
            }
        } else {
            ResolvedProductPtr usedProduct = m_productsByName.value(dependency.uniqueName());
            const QString depDisplayName = ResolvedProduct::fullDisplayName(dependency.name,
                    dependency.multiplexConfigurationId);
            if (!usedProduct) {
                throw ErrorInfo(Tr::tr("Product '%1' depends on '%2', which does not exist.")
                                .arg(product->fullDisplayName(), depDisplayName),
                                product->location);
            }
            if (!dependency.profile.isEmpty() && usedProduct->profile() != dependency.profile) {
                usedProduct.reset();
                for (const ResolvedProductPtr &p : qAsConst(m_productsByName)) {
                    if (p->name == dependency.name && p->profile() == dependency.profile) {
                        usedProduct = p;
                        break;
                    }
                }
                if (!usedProduct) {
                    throw ErrorInfo(Tr::tr("Product '%1' depends on '%2', which does not exist "
                                           "for the requested profile '%3'.")
                                    .arg(product->fullDisplayName(), depDisplayName,
                                         dependency.profile),
                                    product->location);
                }
            }
            if (!usedProduct->enabled) {
                if (!dependency.isRequired)
                    continue;
                ErrorInfo e;
                e.append(Tr::tr("Product '%1' depends on '%2',")
                         .arg(product->name, usedProduct->name), product->location);
                e.append(Tr::tr("but product '%1' is disabled.").arg(usedProduct->name),
                             usedProduct->location);
                if (m_setupParams.productErrorMode() == ErrorHandlingMode::Strict)
                    throw e;
                result.hasDisabledDependency = true;
            }
            result.dependencies.emplace_back(usedProduct, dependency.parameters);
        }
    }
    return result;
}

void ProjectResolver::matchArtifactProperties(const ResolvedProductPtr &product,
        const std::vector<SourceArtifactPtr> &artifacts)
{
    for (const SourceArtifactPtr &artifact : artifacts) {
        for (const auto &artifactProperties : product->artifactProperties) {
            if (!artifact->isTargetOfModule()
                    && artifact->fileTags.intersects(artifactProperties->fileTagsFilter())) {
                artifact->properties = artifactProperties->propertyMap();
            }
        }
    }
}

void ProjectResolver::printProfilingInfo()
{
    if (!m_setupParams.logElapsedTime())
        return;
    m_logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("All property evaluation took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimeAllPropEval));
    m_logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("Module property evaluation took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimeModPropEval));
    m_logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Resolving groups (without module property "
                                                "evaluation) took %1.")
                                         .arg(elapsedTimeString(m_elapsedTimeGroups));
}

class TempScopeSetter
{
public:
    TempScopeSetter(Item * item, Item *newScope) : m_item(item), m_oldScope(item->scope())
    {
        item->setScope(newScope);
    }
    ~TempScopeSetter() { m_item->setScope(m_oldScope); }
private:
    Item * const m_item;
    Item * const m_oldScope;
};

void ProjectResolver::collectPropertiesForExportItem(Item *productModuleInstance)
{
    if (!productModuleInstance->isPresentModule())
        return;
    Item * const exportItem = productModuleInstance->prototype();
    QBS_CHECK(exportItem && exportItem->type() == ItemType::Export);
    TempScopeSetter tempScopeSetter(exportItem, productModuleInstance->scope());
    const ItemDeclaration::Properties exportDecls = BuiltinDeclarations::instance()
            .declarationsForType(ItemType::Export).properties();
    ExportedModule &exportedModule = m_productContext->product->exportedModule;
    const auto &props = exportItem->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        const auto match
                = [it](const PropertyDeclaration &decl) { return decl.name() == it.key(); };
        if (it.key() != StringConstants::prefixMappingProperty() &&
                std::find_if(exportDecls.begin(), exportDecls.end(), match) != exportDecls.end()) {
            continue;
        }
        if (it.value()->type() == Value::ItemValueType) {
            collectPropertiesForExportItem(it.key(), it.value(), productModuleInstance,
                                           exportedModule.modulePropertyValues);
        } else {
            evaluateProperty(exportItem, it.key(), it.value(), exportedModule.propertyValues,
                             false);
        }
    }
}

// Collects module properties assigned to in other (higher-level) modules.
void ProjectResolver::collectPropertiesForModuleInExportItem(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    ExportedModule &exportedModule = m_productContext->product->exportedModule;
    if (module.isProduct || module.name.first() == StringConstants::qbsModule())
        return;
    const auto checkName = [module](const ExportedModuleDependency &d) {
        return module.name.toString() == d.name;
    };
    if (any_of(exportedModule.moduleDependencies, checkName))
        return;

    Item *modulePrototype = module.item->prototype();
    while (modulePrototype && modulePrototype->type() != ItemType::Module)
        modulePrototype = modulePrototype->prototype();
    if (!modulePrototype) // Can happen for broken products in relaxed mode.
        return;
    TempScopeSetter tempScopeSetter(modulePrototype, module.item->scope());
    const Item::PropertyMap &props = modulePrototype->properties();
    ExportedModuleDependency dep;
    dep.name = module.name.toString();
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (it.value()->type() == Value::ItemValueType)
            collectPropertiesForExportItem(it.key(), it.value(), module.item, dep.moduleProperties);
    }
    exportedModule.moduleDependencies.push_back(dep);

    for (const auto &dep : module.item->modules())
        collectPropertiesForModuleInExportItem(dep);
}

static bool hasDependencyCycle(Set<ResolvedProduct *> *checked,
                               Set<ResolvedProduct *> *branch,
                               const ResolvedProductPtr &product,
                               ErrorInfo *error)
{
    if (branch->contains(product.get()))
        return true;
    if (checked->contains(product.get()))
        return false;
    checked->insert(product.get());
    branch->insert(product.get());
    for (const ResolvedProductPtr &dep : qAsConst(product->dependencies)) {
        if (hasDependencyCycle(checked, branch, dep, error)) {
            error->prepend(dep->name, dep->location);
            return true;
        }
    }
    branch->remove(product.get());
    return false;
}

using DependencyMap = QHash<ResolvedProduct *, Set<ResolvedProduct *>>;
void gatherDependencies(ResolvedProduct *product, DependencyMap &dependencies)
{
    if (dependencies.contains(product))
        return;
    Set<ResolvedProduct *> &productDeps = dependencies[product];
    for (const ResolvedProductPtr &dep : qAsConst(product->dependencies)) {
        productDeps << dep.get();
        gatherDependencies(dep.get(), dependencies);
        productDeps += dependencies.value(dep.get());
    }
}



static DependencyMap allDependencies(const std::vector<ResolvedProductPtr> &products)
{
    DependencyMap dependencies;
    for (const ResolvedProductPtr &product : products)
        gatherDependencies(product.get(), dependencies);
    return dependencies;
}

void ProjectResolver::resolveProductDependencies(const ProjectContext &projectContext)
{
    // Resolve all inter-product dependencies.
    const std::vector<ResolvedProductPtr> allProducts = projectContext.project->allProducts();
    bool disabledDependency = false;
    for (const ResolvedProductPtr &rproduct : allProducts) {
        if (!rproduct->enabled)
            continue;
        Item *productItem = m_productItemMap.value(rproduct);
        const ModuleLoaderResult::ProductInfo &productInfo
                = m_loadResult.productInfos.value(productItem);
        const ProductDependencyInfos &depInfos = getProductDependencies(rproduct, productInfo);
        if (depInfos.hasDisabledDependency)
            disabledDependency = true;
        for (const auto &dep : depInfos.dependencies) {
            if (!contains(rproduct->dependencies, dep.product))
                rproduct->dependencies.push_back(dep.product);
            if (!dep.parameters.empty())
                rproduct->dependencyParameters.insert(dep.product, dep.parameters);
        }
    }

    // Check for cyclic dependencies.
    Set<ResolvedProduct *> checked;
    for (const ResolvedProductPtr &rproduct : allProducts) {
        Set<ResolvedProduct *> branch;
        ErrorInfo error;
        if (hasDependencyCycle(&checked, &branch, rproduct, &error)) {
            error.prepend(rproduct->name, rproduct->location);
            error.prepend(Tr::tr("Cyclic dependencies detected."));
            throw error;
        }
    }

    // Mark all products as disabled that have a disabled dependency.
    if (disabledDependency && m_setupParams.productErrorMode() == ErrorHandlingMode::Relaxed) {
        const DependencyMap allDeps = allDependencies(allProducts);
        DependencyMap allDepsReversed;
        for (auto it = allDeps.constBegin(); it != allDeps.constEnd(); ++it) {
            for (ResolvedProduct *dep : qAsConst(it.value()))
                allDepsReversed[dep] << it.key();
        }
        for (auto it = allDepsReversed.constBegin(); it != allDepsReversed.constEnd(); ++it) {
            if (it.key()->enabled)
                continue;
            for (ResolvedProduct * const dependingProduct : qAsConst(it.value())) {
                if (dependingProduct->enabled) {
                    m_logger.qbsWarning() << Tr::tr("Disabling product '%1', because it depends on "
                                                    "disabled product '%2'.")
                                             .arg(dependingProduct->name, it.key()->name);
                    dependingProduct->enabled = false;
                }
            }
        }
    }
}

void ProjectResolver::postProcess(const ResolvedProductPtr &product,
                                  ProjectContext *projectContext) const
{
    product->fileTaggers << projectContext->fileTaggers;
    std::sort(std::begin(product->fileTaggers), std::end(product->fileTaggers),
              [] (const FileTaggerConstPtr &a, const FileTaggerConstPtr &b) {
        return a->priority() > b->priority();
    });
    for (const RulePtr &rule : projectContext->rules) {
        RulePtr clonedRule = rule->clone();
        clonedRule->product = product.get();
        product->rules.push_back(clonedRule);
    }
}

void ProjectResolver::applyFileTaggers(const ResolvedProductPtr &product) const
{
    for (const SourceArtifactPtr &artifact : product->allEnabledFiles())
        applyFileTaggers(artifact, product);
}

void ProjectResolver::applyFileTaggers(const SourceArtifactPtr &artifact,
        const ResolvedProductConstPtr &product)
{
    if (!artifact->overrideFileTags || artifact->fileTags.empty()) {
        const QString fileName = FileInfo::fileName(artifact->absoluteFilePath);
        const FileTags fileTags = product->fileTagsForFileName(fileName);
        artifact->fileTags.unite(fileTags);
        if (artifact->fileTags.empty())
            artifact->fileTags.insert(unknownFileTag());
        qCDebug(lcProjectResolver) << "adding file tags" << artifact->fileTags
                                   << "to" << fileName;
    }
}

QVariantMap ProjectResolver::evaluateModuleValues(Item *item, bool lookupPrototype)
{
    AccumulatingTimer modPropEvalTimer(m_setupParams.logElapsedTime()
                                       ? &m_elapsedTimeModPropEval : nullptr);
    QVariantMap moduleValues;
    for (const Item::Module &module : item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        const QString fullName = module.name.toString();
        moduleValues[fullName] = evaluateProperties(module.item, lookupPrototype, true);
    }

    return moduleValues;
}

QVariantMap ProjectResolver::evaluateProperties(Item *item, bool lookupPrototype, bool checkErrors)
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt, lookupPrototype, checkErrors);
}

QVariantMap ProjectResolver::evaluateProperties(const Item *item, const Item *propertiesContainer,
        const QVariantMap &tmplt, bool lookupPrototype, bool checkErrors)
{
    AccumulatingTimer propEvalTimer(m_setupParams.logElapsedTime()
                                    ? &m_elapsedTimeAllPropEval : nullptr);
    QVariantMap result = tmplt;
    for (QMap<QString, ValuePtr>::const_iterator it = propertiesContainer->properties().begin();
         it != propertiesContainer->properties().end(); ++it) {
        checkCancelation();
        evaluateProperty(item, it.key(), it.value(), result, checkErrors);
    }
    return lookupPrototype && propertiesContainer->prototype()
            ? evaluateProperties(item, propertiesContainer->prototype(), result, true, checkErrors)
            : result;
}

void ProjectResolver::evaluateProperty(const Item *item, const QString &propName,
        const ValuePtr &propValue, QVariantMap &result, bool checkErrors)
{
    switch (propValue->type()) {
    case Value::ItemValueType:
    {
        // Ignore items. Those point to module instances
        // and are handled in evaluateModuleValues().
        break;
    }
    case Value::JSSourceValueType:
    {
        if (result.contains(propName))
            break;
        const PropertyDeclaration pd = item->propertyDeclaration(propName);
        if (pd.flags().testFlag(PropertyDeclaration::PropertyNotAvailableInConfig)) {
            break;
        }
        const QScriptValue scriptValue = m_evaluator->property(item, propName);
        if (checkErrors && Q_UNLIKELY(m_evaluator->engine()->hasErrorOrException(scriptValue))) {
            throw ErrorInfo(m_evaluator->engine()->lastError(scriptValue,
                                                             propValue->location()));
        }

        // NOTE: Loses type information if scriptValue.isUndefined == true,
        //       as such QScriptValues become invalid QVariants.
        QVariant v;
        if (scriptValue.isFunction()) {
            v = scriptValue.toString();
        } else {
            v = scriptValue.toVariant();
            QVariantMap m = v.toMap();
            if (m.contains(StringConstants::importScopeNamePropertyInternal())) {
                QVariantMap tmp = m;
                m = scriptValue.prototype().toVariant().toMap();
                for (auto it = tmp.begin(); it != tmp.end(); ++it)
                    m.insert(it.key(), it.value());
                v = m;
            }
        }

        if (pd.type() == PropertyDeclaration::Path && v.isValid()) {
            v = v.toString();
        } else if (pd.type() == PropertyDeclaration::PathList
                   || pd.type() == PropertyDeclaration::StringList) {
            v = v.toStringList();
        } else if (pd.type() == PropertyDeclaration::VariantList) {
            v = v.toList();
        }
        result[propName] = v;
        break;
    }
    case Value::VariantValueType:
    {
        if (result.contains(propName))
            break;
        VariantValuePtr vvp = std::static_pointer_cast<VariantValue>(propValue);
        QVariant v = vvp->value();

        if (v.isNull() && !item->propertyDeclaration(propName).isScalar()) // QTBUG-51237
            v = QStringList();

        result[propName] = v;
        break;
    }
    }
}

void ProjectResolver::collectPropertiesForExportItem(const QualifiedId &moduleName,
         const ValuePtr &value, Item *moduleInstance, QVariantMap &moduleProps)
{
    QBS_CHECK(value->type() == Value::ItemValueType);
    Item * const itemValueItem = std::static_pointer_cast<ItemValue>(value)->item();
    if (itemValueItem->type() == ItemType::ModuleInstance) {
        struct EvalPreparer {
            EvalPreparer(Item *valueItem, Item *moduleInstance, const QualifiedId &moduleName)
                : valueItem(valueItem), oldScope(valueItem->scope()),
                  hadName(!!valueItem->variantProperty(StringConstants::nameProperty()))
            {
                valueItem->setScope(moduleInstance);
                if (!hadName) {
                    // EvaluatorScriptClass expects a name here.
                    valueItem->setProperty(StringConstants::nameProperty(),
                                           VariantValue::create(moduleName.toString()));
                }
            }
            ~EvalPreparer()
            {
                valueItem->setScope(oldScope);
                if (!hadName)
                    valueItem->setProperty(StringConstants::nameProperty(), VariantValuePtr());
            }
            Item * const valueItem;
            Item * const oldScope;
            const bool hadName;
        };
        EvalPreparer ep(itemValueItem, moduleInstance, moduleName);
        moduleProps.insert(moduleName.toString(), evaluateProperties(itemValueItem, false, false));
        return;
    }
    QBS_CHECK(itemValueItem->type() == ItemType::ModulePrefix);
    const Item::PropertyMap &props = itemValueItem->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        QualifiedId fullModuleName = moduleName;
        fullModuleName << it.key();
        collectPropertiesForExportItem(fullModuleName, it.value(), moduleInstance, moduleProps);
    }
}

void ProjectResolver::createProductConfig(ResolvedProduct *product)
{
    EvalCacheEnabler cachingEnabler(m_evaluator);
    m_evaluator->setPathPropertiesBaseDir(m_productContext->product->sourceDirectory);
    product->moduleProperties->setValue(evaluateModuleValues(m_productContext->item));
    product->productProperties = evaluateProperties(m_productContext->item, m_productContext->item,
                                                    QVariantMap(), true, true);
    m_evaluator->clearPathPropertiesBaseDir();
}

void ProjectResolver::callItemFunction(const ItemFuncMap &mappings, Item *item,
                                       ProjectContext *projectContext)
{
    const ItemFuncPtr f = mappings.value(item->type());
    QBS_CHECK(f);
    if (item->type() == ItemType::Project) {
        ProjectContext subProjectContext = createProjectContext(projectContext);
        (this->*f)(item, &subProjectContext);
    } else {
        (this->*f)(item, projectContext);
    }
}

ProjectResolver::ProjectContext ProjectResolver::createProjectContext(ProjectContext *parentProjectContext) const
{
    ProjectContext subProjectContext;
    subProjectContext.parentContext = parentProjectContext;
    subProjectContext.project = ResolvedProject::create();
    parentProjectContext->project->subProjects.push_back(subProjectContext.project);
    subProjectContext.project->parentProject = parentProjectContext->project;
    return subProjectContext;
}

} // namespace Internal
} // namespace qbs
