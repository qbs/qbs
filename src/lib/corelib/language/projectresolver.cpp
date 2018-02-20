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
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "language.h"
#include "propertymapinternal.h"
#include "resolvedfilecontext.h"
#include "scriptengine.h"
#include "value.h"

#include <jsextensions/moduleproperties.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>

#include <algorithm>
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
    QList<FileTaggerConstPtr> fileTaggers;
    QList<RulePtr> rules;
    ResolvedModulePtr dummyModule;
};

struct ProjectResolver::ProductContext
{
    ResolvedProductPtr product;
    QString buildDirectory;
    Item *item;
    typedef std::pair<ArtifactPropertiesPtr, CodeLocation> ArtifactPropertiesInfo;
    QHash<QStringList, ArtifactPropertiesInfo> artifactPropertiesPerFilter;
    QHash<QString, CodeLocation> sourceArtifactLocations;
    GroupConstPtr currentGroup;
};

struct ProjectResolver::ModuleContext
{
    ResolvedModulePtr module;
};

class CancelException { };


ProjectResolver::ProjectResolver(Evaluator *evaluator, const ModuleLoaderResult &loadResult,
        const SetupProjectParameters &setupParameters, Logger &logger)
    : m_evaluator(evaluator)
    , m_logger(logger)
    , m_engine(m_evaluator->engine())
    , m_progressObserver(nullptr)
    , m_setupParams(setupParameters)
    , m_loadResult(loadResult)
{
    QBS_CHECK(FileInfo::isAbsolute(m_setupParams.buildRoot()));
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
    for (int i = 0; i < allProducts.size(); ++i) {
        const ResolvedProductConstPtr product1 = allProducts.at(i);
        const QString productName = product1->uniqueName();
        for (int j = i + 1; j < allProducts.size(); ++j) {
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
    } catch (const ErrorInfo &errorInfo) {
        ErrorInfo e;
        for (auto it = m_loadResult.productInfos.cbegin(); it != m_loadResult.productInfos.cend();
             ++it) {
            const auto &productInfo = it.value();
            if (productInfo.delayedError.hasError())
                appendError(e, productInfo.delayedError);
        }
        appendError(e, errorInfo);
        throw e;
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
    ProjectContext projectContext;
    projectContext.project = project;
    resolveProject(m_loadResult.root, &projectContext);
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
        { ItemType::Rule, &ProjectResolver::resolveRule },
        { ItemType::PropertyOptions, &ProjectResolver::ignoreItem }
    };

    for (Item * const child : item->children())
        callItemFunction(mapping, child, projectContext);

    for (const ResolvedProductPtr &product : qAsConst(projectContext->project->products))
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
    product->profile = m_evaluator->stringValue(item, StringConstants::profileProperty());
    QBS_CHECK(!product->profile.isEmpty());
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
        { ItemType::Group, &ProjectResolver::resolveGroup },
        { ItemType::Export, &ProjectResolver::ignoreItem },
        { ItemType::Probe, &ProjectResolver::ignoreItem },
        { ItemType::PropertyOptions, &ProjectResolver::ignoreItem }
    };

    for (Item * const child : qAsConst(subItems))
        callItemFunction(mapping, child, projectContext);

    resolveModules(item, projectContext);

    for (const FileTag &t : qAsConst(product->fileTags))
        m_productsByType[t].push_back(product);
}

void ProjectResolver::resolveModules(const Item *item, ProjectContext *projectContext)
{
    for (const Item::Module &m : item->modules())
        resolveModule(m.name, m.item, m.isProduct, m.parameters, projectContext);
}

void ProjectResolver::resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                                    const QVariantMap &parameters, ProjectContext *projectContext)
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
        { ItemType::Scanner, &ProjectResolver::resolveScanner },
        { ItemType::PropertyOptions, &ProjectResolver::ignoreItem },
        { ItemType::Depends, &ProjectResolver::ignoreItem },
        { ItemType::Parameter, &ProjectResolver::ignoreItem },
        { ItemType::Probe, &ProjectResolver::ignoreItem }
    };
    for (Item *child : item->children())
        callItemFunction(mapping, child, projectContext);

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
                      VariantValue::create(product->fileTags.toStringList()));
}

SourceArtifactPtr ProjectResolver::createSourceArtifact(const ResolvedProductPtr &rproduct,
        const QString &fileName, const GroupPtr &group, bool wildcard,
        const CodeLocation &filesLocation, QHash<QString, CodeLocation> *fileLocations,
        ErrorInfo *errorInfo)
{
    const QString &baseDir = FileInfo::path(group->location.filePath());
    const QString absFilePath = QDir::cleanPath(FileInfo::resolvePath(baseDir, fileName));
    if (!wildcard && !FileInfo(absFilePath).exists()) {
        if (errorInfo)
            errorInfo->append(Tr::tr("File '%1' does not exist.").arg(absFilePath), filesLocation);
        rproduct->missingSourceFiles << absFilePath;
        return SourceArtifactPtr();
    }
    if (group->enabled && fileLocations) {
        CodeLocation &loc = (*fileLocations)[absFilePath];
        if (loc.isValid()) {
            if (errorInfo) {
                errorInfo->append(Tr::tr("Duplicate source file '%1'.").arg(absFilePath));
                errorInfo->append(Tr::tr("First occurrence is here."), loc);
                errorInfo->append(Tr::tr("Next occurrence is here."), filesLocation);
            }
            return SourceArtifactPtr();
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
        return QVariantMap();
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
                          evaluateProperties(module.item, module.item, reusableValues, true));
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
    PropertyMapPtr moduleProperties = m_productContext->currentGroup
            ? m_productContext->currentGroup->properties
            : m_productContext->product->moduleProperties;
    const QVariantMap newModuleProperties
            = resolveAdditionalModuleProperties(item, moduleProperties->value());
    if (!newModuleProperties.empty()) {
        moduleProperties = PropertyMapInternal::create();
        moduleProperties->setValue(newModuleProperties);
    }

    AccumulatingTimer groupTimer(m_setupParams.logElapsedTime()
                                       ? &m_elapsedTimeGroups : nullptr);

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
        aprops->setExtraFileTags(fileTags);
        aprops->setPropertyMapInternal(moduleProperties);
        m_productContext->product->artifactProperties.push_back(aprops);
        m_productContext->artifactPropertiesPerFilter.insert(fileTagsFilter,
                                ProductContext::ArtifactPropertiesInfo(aprops, item->location()));
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
    group->properties = moduleProperties;
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
        group->wildcards = std::unique_ptr<SourceWildCards>(new SourceWildCards);
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

    for (const QString &fileName : files) {
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
    if (rule->outputArtifactsScript.isValid()) {
        if (hasArtifactChildren)
            throw ErrorInfo(Tr::tr("The Rule.outputArtifacts script is not allowed in rules "
                                   "that contain Artifact items."),
                        item->location());
        rule->outputFileTags = m_evaluator->fileTagsValue(
                    item, StringConstants::outputFileTagsProperty());
        if (rule->outputFileTags.empty())
            throw ErrorInfo(Tr::tr("Rule.outputFileTags must be specified if "
                                   "Rule.outputArtifacts is specified."),
                            item->location());
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
    rule->excludedAuxiliaryInputs
            = m_evaluator->fileTagsValue(item, StringConstants::excludedAuxiliaryInputsProperty());
    rule->explicitlyDependsOn
            = m_evaluator->fileTagsValue(item, StringConstants::explicitlyDependsOnProperty());
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
    QList<FileTaggerConstPtr> &fileTaggers = m_productContext
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
        QBS_CHECK(dependency.name.isEmpty() != dependency.productTypes.empty());
        if (!dependency.productTypes.empty()) {
            for (const FileTag &tag : dependency.productTypes) {
                const QList<ResolvedProductPtr> productsForTag = m_productsByType.value(tag);
                for (const ResolvedProductPtr &p : productsForTag) {
                    if (p == product || !p->enabled
                            || (dependency.limitToSubProject && !product->isInParentProject(p))) {
                        continue;
                    }
                    result.dependencies.emplace_back(p, dependency.parameters);
                }
            }
        } else if (dependency.profile == StringConstants::star()) {
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
            if (!dependency.profile.isEmpty() && usedProduct->profile != dependency.profile) {
                usedProduct.reset();
                for (const ResolvedProductPtr &p : qAsConst(m_productsByName)) {
                    if (p->name == dependency.name && p->profile == dependency.profile) {
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
        const QList<SourceArtifactPtr> &artifacts)
{
    for (const SourceArtifactPtr &artifact : artifacts) {
        for (const ArtifactPropertiesConstPtr &artifactProperties :
                 qAsConst(product->artifactProperties)) {
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



static DependencyMap allDependencies(const QList<ResolvedProductPtr> &products)
{
    DependencyMap dependencies;
    for (const ResolvedProductPtr &product : products)
        gatherDependencies(product.get(), dependencies);
    return dependencies;
}

void ProjectResolver::resolveProductDependencies(const ProjectContext &projectContext)
{
    // Resolve all inter-product dependencies.
    const QList<ResolvedProductPtr> allProducts = projectContext.project->allProducts();
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
            rproduct->dependencies.insert(dep.product);
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
    for (const RulePtr &rule : qAsConst(projectContext->rules)) {
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
        const QString fullName = module.name.toString();
        moduleValues[fullName] = evaluateProperties(module.item, lookupPrototype);
    }

    return moduleValues;
}

QVariantMap ProjectResolver::evaluateProperties(Item *item, bool lookupPrototype)
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt, lookupPrototype);
}

QVariantMap ProjectResolver::evaluateProperties(const Item *item, const Item *propertiesContainer,
        const QVariantMap &tmplt, bool lookupPrototype)
{
    AccumulatingTimer propEvalTimer(m_setupParams.logElapsedTime()
                                    ? &m_elapsedTimeAllPropEval : nullptr);
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
            if (pd.flags().testFlag(PropertyDeclaration::PropertyNotAvailableInConfig)) {
                break;
            }

            const QScriptValue scriptValue = m_evaluator->property(item, it.key());
            if (Q_UNLIKELY(m_evaluator->engine()->hasErrorOrException(scriptValue))) {
                throw ErrorInfo(m_evaluator->engine()->lastError(scriptValue,
                                                                 it.value()->location()));
            }

            // NOTE: Loses type information if scriptValue.isUndefined == true,
            //       as such QScriptValues become invalid QVariants.
            QVariant v = scriptValue.toVariant();
            if (pd.type() == PropertyDeclaration::Path && v.isValid()) {
                v = v.toString();
            } else if (pd.type() == PropertyDeclaration::PathList
                       || pd.type() == PropertyDeclaration::StringList) {
                v = v.toStringList();
            }
            result[it.key()] = v;
            break;
        }
        case Value::VariantValueType:
        {
            if (result.contains(it.key()))
                break;
            VariantValuePtr vvp = std::static_pointer_cast<VariantValue>(it.value());
            QVariant v = vvp->value();

            if (v.isNull() && !item->propertyDeclaration(it.key()).isScalar()) // QTBUG-51237
                v = QStringList();

            result[it.key()] = v;
            break;
        }
        }
    }
    return lookupPrototype && propertiesContainer->prototype()
            ? evaluateProperties(item, propertiesContainer->prototype(), result, true)
            : result;
}

void ProjectResolver::createProductConfig(ResolvedProduct *product)
{
    EvalCacheEnabler cachingEnabler(m_evaluator);
    m_evaluator->setPathPropertiesBaseDir(m_productContext->product->sourceDirectory);
    product->moduleProperties->setValue(evaluateModuleValues(m_productContext->item));
    product->productProperties = evaluateProperties(m_productContext->item, m_productContext->item,
                                                    QVariantMap());
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
