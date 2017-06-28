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
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/setupprojectparameters.h>

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
    ResolvedProjectPtr project;
    QList<FileTaggerConstPtr> fileTaggers;
    QList<RulePtr> rules;
    ResolvedModulePtr dummyModule;
};

struct ProjectResolver::ProductContext
{
    ResolvedProductPtr product;
    QString buildDirectory;
    FileTags additionalFileTags;
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


ProjectResolver::ProjectResolver(Evaluator *evaluator, const ModuleLoaderResult &loadResult,
        const SetupProjectParameters &setupParameters, Logger &logger)
    : m_evaluator(evaluator)
    , m_logger(logger)
    , m_engine(m_evaluator->engine())
    , m_progressObserver(0)
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

TopLevelProjectPtr ProjectResolver::resolve()
{
    TimedActivityLogger projectResolverTimer(m_logger, Tr::tr("ProjectResolver"),
                                             m_setupParams.logElapsedTime());
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[PR] resolving " << m_loadResult.root->file()->filePath();

    m_productContext = 0;
    m_moduleContext = 0;
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
            if (productInfo.delayedError.hasError()) {
                try {
                    QString name = m_evaluator->stringValue(it.key(), QStringLiteral("name"));
                    e.append(Tr::tr("Errors in product '%1':").arg(name), it.key()->location());
                } catch (const ErrorInfo &/* ignore */) {
                    // The name cannot be determined because of other errors.
                    e.append(Tr::tr("Errors in product:"), it.key()->location());
                }
                appendError(e, productInfo.delayedError);
            }
        }
        appendError(e, errorInfo);
        throw e;
    }
    return tlp;
}

void ProjectResolver::checkCancelation() const
{
    if (m_progressObserver && m_progressObserver->canceled()) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration %1.")
                    .arg(TopLevelProject::deriveId(m_setupParams.finalBuildConfigurationTree())));
    }
}

QString ProjectResolver::verbatimValue(const ValueConstPtr &value, bool *propertyWasSet) const
{
    QString result;
    if (value && value->type() == Value::JSSourceValueType) {
        const JSSourceValueConstPtr sourceValue = std::static_pointer_cast<const JSSourceValue>(
                    value);
        result = sourceCodeForEvaluation(sourceValue);
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
    Set<QString> subProjectNames;
    Set<ResolvedProjectPtr> projectsInNeedOfNameChange;
    for (const ResolvedProjectPtr &p : qAsConst(parentProject->subProjects)) {
        if (!subProjectNames.insert(p->name).second)
            projectsInNeedOfNameChange << p;
        makeSubProjectNamesUniqe(p);
    }
    while (!projectsInNeedOfNameChange.isEmpty()) {
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
        m_progressObserver->setMaximum(m_loadResult.productInfos.count());
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
    project->usedEnvironment = m_engine->usedEnvironment();
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
            if (artifact->properties->qbsPropertyValue(QLatin1String("install")).toBool())
                artifact->fileTags += "installable";
        }
    }
    project->warningsEncountered = m_logger.warnings();
    return project;
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
                = m_evaluator->stringValue(propertiesItem, QLatin1String("name"));
    }
}

void ProjectResolver::resolveProduct(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    m_evaluator->clearPropertyDependencies();
    ProductContext productContext;
    m_productContext = &productContext;
    productContext.item = item;
    ResolvedProductPtr product = ResolvedProduct::create();
    product->moduleProperties = PropertyMapInternal::create();
    product->project = projectContext->project;
    m_productItemMap.insert(product, item);
    projectContext->project->products += product;
    productContext.product = product;
    product->name = m_evaluator->stringValue(item, QLatin1String("name"));
    product->location = item->location();

    // product->buildDirectory() isn't valid yet, because the productProperties map is not ready.
    productContext.buildDirectory = m_evaluator->stringValue(item, QLatin1String("buildDirectory"));
    product->profile = m_evaluator->stringValue(item, QLatin1String("profile"));
    QBS_CHECK(!product->profile.isEmpty());
    product->multiplexConfigurationId
            = m_evaluator->stringValue(item, QLatin1String("multiplexConfigurationId"));
    m_logger.qbsTrace() << "[PR] resolveProduct " << product->uniqueName();
    m_productsByName.insert(product->uniqueName(), product);
    product->enabled = m_evaluator->boolValue(item, QLatin1String("condition"));
    ModuleLoaderResult::ProductInfo &pi = m_loadResult.productInfos[item];
    if (pi.delayedError.hasError()) {
        if (product->enabled) {
            switch (m_setupParams.productErrorMode()) {
            case ErrorHandlingMode::Relaxed:
                m_logger.printWarning(pi.delayedError);
                m_logger.printWarning(ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                                                .arg(product->name), item->location()));
                product->enabled = false;
                break;
            case ErrorHandlingMode::Strict:
                {
                    ErrorInfo errorInfo;
                    std::swap(pi.delayedError, errorInfo);
                    throw errorInfo;
                }
            }
        }
        pi.delayedError.clear();
        return;
    }
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
    product->probes = pi.probes;
    product->productProperties = createProductConfig();
    product->productProperties.insert(destDirKey, product->destinationDirectory);
    QVariantMap moduleProperties;
    moduleProperties.insert(QLatin1String("modules"),
                            product->productProperties.take(QLatin1String("modules")));
    product->moduleProperties->setValue(moduleProperties);
    ModuleProperties::init(m_evaluator->scriptValue(item), product);

    QList<Item *> subItems = item->children();
    const ValuePtr filesProperty = item->property(QLatin1String("files"));
    if (filesProperty) {
        Item *fakeGroup = Item::create(item->pool(), ItemType::Group);
        fakeGroup->setFile(item->file());
        fakeGroup->setLocation(item->location());
        fakeGroup->setScope(item);
        fakeGroup->setProperty(QLatin1String("name"), VariantValue::create(product->name));
        fakeGroup->setProperty(QLatin1String("files"), filesProperty);
        fakeGroup->setProperty(QLatin1String("excludeFiles"),
                               item->property(QLatin1String("excludeFiles")));
        fakeGroup->setProperty(QLatin1String("overrideTags"), VariantValue::create(false));
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
    product->fileTags += productContext.additionalFileTags;

    for (const FileTag &t : qAsConst(product->fileTags))
        m_productsByType[t] << product;

    m_productContext = 0;
    if (m_progressObserver)
        m_progressObserver->incrementProgressValue();
}

void ProjectResolver::resolveModules(const Item *item, ProjectContext *projectContext)
{
    // Breadth first search needed here, because the product might set properties on the cpp module,
    // whose children must be evaluated in that context then.
    std::queue<Item::Module> modules;
    for (const Item::Module &m : item->modules())
        modules.push(m);
    Set<QualifiedId> seen;
    while (!modules.empty()) {
        const Item::Module m = modules.front();
        modules.pop();
        if (!seen.insert(m.name).second)
            continue;
        resolveModule(m.name, m.item, m.isProduct, m.parameters, projectContext);
        for (const Item::Module &childModule : m.item->modules())
            modules.push(childModule);
    }
    std::sort(m_productContext->product->modules.begin(), m_productContext->product->modules.end(),
              [](const ResolvedModuleConstPtr &m1, const ResolvedModuleConstPtr &m2) {
                      return m1->name < m2->name;
              });
}

void ProjectResolver::resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                                    const QVariantMap &parameters, ProjectContext *projectContext)
{
    checkCancelation();
    if (!m_evaluator->boolValue(item, QLatin1String("present")))
        return;

    ModuleContext * const oldModuleContext = m_moduleContext;
    ModuleContext moduleContext;
    moduleContext.module = ResolvedModule::create();
    m_moduleContext = &moduleContext;

    const ResolvedModulePtr &module = moduleContext.module;
    module->name = moduleName.toString();
    module->isProduct = isProduct;
    module->setupBuildEnvironmentScript = scriptFunctionValue(item,
                                                            QLatin1String("setupBuildEnvironment"));
    module->setupRunEnvironmentScript = scriptFunctionValue(item,
                                                            QLatin1String("setupRunEnvironment"));

    m_productContext->additionalFileTags +=
            m_evaluator->fileTagsValue(item, QLatin1String("additionalProductTypes"));

    for (const Item::Module &m : item->modules()) {
        if (m_evaluator->boolValue(m.item, QLatin1String("present")))
            module->moduleDependencies += m.name.toString();
    }

    m_productContext->product->modules += module;
    if (!parameters.isEmpty())
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
    (wildcard ? group->wildcards->files : group->files) += artifact;
    return artifact;
}

static QualifiedIdSet propertiesToEvaluate(const QList<QualifiedId> &initialProps,
                                              const PropertyDependencies &deps)
{
    QList<QualifiedId> remainingProps = initialProps;
    QualifiedIdSet allProperties;
    while (!remainingProps.isEmpty()) {
        const QualifiedId prop = remainingProps.takeFirst();
        const auto insertResult = allProperties.insert(prop);
        if (!insertResult.second)
            continue;
        for (const QualifiedId &directDep : deps.value(prop))
            remainingProps << directDep;
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
    QVariantMap modulesMap = currentValues.value(QLatin1String("modules")).toMap();
    QHash<QString, QStringList> propsPerModule;
    for (auto it = propsToEval.cbegin(); it != propsToEval.cend(); ++it) {
        const QualifiedId fullPropName = *it;
        const QString moduleName
                = QualifiedId(fullPropName.mid(0, fullPropName.count() - 1)).toString();
        propsPerModule[moduleName] << fullPropName.last();
    }
    EvalCacheEnabler cachingEnabler(m_evaluator);
    for (const Item::Module &module : group->modules()) {
        const QString &fullModName = module.name.toString();
        const QStringList propsForModule = propsPerModule.take(fullModName);
        if (propsForModule.isEmpty())
            continue;
        QVariantMap reusableValues = modulesMap.value(fullModName).toMap();
        for (const QString &prop : qAsConst(propsForModule))
            reusableValues.remove(prop);
        modulesMap.insert(fullModName,
                          evaluateProperties(module.item, module.item, reusableValues, true));
    }
    QVariantMap newValues = currentValues;
    newValues.insert(QLatin1String("modules"), modulesMap);
    return newValues;
}

void ProjectResolver::resolveGroup(Item *item, ProjectContext *projectContext)
{
    Q_UNUSED(projectContext);
    checkCancelation();
    PropertyMapPtr moduleProperties = m_productContext->currentGroup
            ? m_productContext->currentGroup->properties
            : m_productContext->product->moduleProperties;
    const QVariantMap newModuleProperties
            = resolveAdditionalModuleProperties(item, moduleProperties->value());
    if (!newModuleProperties.isEmpty()) {
        moduleProperties = PropertyMapInternal::create();
        moduleProperties->setValue(newModuleProperties);
    }

    AccumulatingTimer groupTimer(m_setupParams.logElapsedTime()
                                       ? &m_elapsedTimeGroups : nullptr);

    bool isEnabled = m_evaluator->boolValue(item, QLatin1String("condition"));
    if (m_productContext->currentGroup)
        isEnabled = isEnabled && m_productContext->currentGroup->enabled;
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
    group->properties = moduleProperties;
    bool fileTagsSet;
    group->fileTags = m_evaluator->fileTagsValue(item, QLatin1String("fileTags"), &fileTagsSet);
    group->overrideTags = m_evaluator->boolValue(item, QLatin1String("overrideTags"));
    if (group->overrideTags && fileTagsSet) {
        if (group->fileTags.isEmpty() )
            group->fileTags.insert(unknownFileTag());
    } else if (m_productContext->currentGroup) {
        group->fileTags.unite(m_productContext->currentGroup->fileTags);
    }

    const CodeLocation filesLocation = item->property(QLatin1String("files"))->location();
    ErrorInfo fileError;
    if (!patterns.isEmpty()) {
        SourceWildCards::Ptr wildcards = SourceWildCards::create();
        wildcards->excludePatterns = m_evaluator->stringListValue(item,
                                                                  QLatin1String("excludeFiles"));
        wildcards->prefix = group->prefix;
        wildcards->patterns = patterns;
        group->wildcards = wildcards;
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
            m_logger.qbsDebug() << fileError.toString();
        }
    }
    group->name = m_evaluator->stringValue(item, QLatin1String("name"));
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(m_productContext->product->groups.count());
    m_productContext->product->groups += group;

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
    for (Item * const child : item->children()) {
        if (Q_UNLIKELY(child->type() != ItemType::Artifact)) {
            throw ErrorInfo(Tr::tr("'Rule' can only have children of type 'Artifact'."),
                               child->location());
        }
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
    rule->alwaysRun = m_evaluator->boolValue(item, QLatin1String("alwaysRun"));
    rule->inputs = m_evaluator->fileTagsValue(item, QLatin1String("inputs"));
    rule->inputsFromDependencies
            = m_evaluator->fileTagsValue(item, QLatin1String("inputsFromDependencies"));
    bool requiresInputsSet = false;
    rule->requiresInputs = m_evaluator->boolValue(item, QLatin1String("requiresInputs"), true,
                                                  &requiresInputsSet);
    if (!requiresInputsSet)
        rule->requiresInputs = rule->declaresInputs();
    rule->auxiliaryInputs
            = m_evaluator->fileTagsValue(item, QLatin1String("auxiliaryInputs"));
    rule->excludedAuxiliaryInputs
            = m_evaluator->fileTagsValue(item, QLatin1String("excludedAuxiliaryInputs"));
    rule->explicitlyDependsOn
            = m_evaluator->fileTagsValue(item, QLatin1String("explicitlyDependsOn"));
    rule->module = m_moduleContext ? m_moduleContext->module : projectContext->dummyModule;
    if (!rule->multiplex && !rule->declaresInputs()) {
        handleError(ErrorInfo(Tr::tr("Rule has no inputs, but is not a multiplex rule."),
                              item->location()));
        return;
    }
    if (!rule->multiplex && !rule->requiresInputs) {
        handleError(ErrorInfo(Tr::tr("Rule.requiresInputs is false for non-multiplex rule.")
                              , item->location()));
        return;
    }
    if (!rule->declaresInputs() && rule->requiresInputs) {
        handleError(ErrorInfo(Tr::tr("Rule.requiresInputs is true, but the rule "
                                     "does not declare any input tags."), item->location()));
        return;
    }
    if (m_productContext)
        m_productContext->product->rules += rule;
    else
        projectContext->rules += rule;
}

void ProjectResolver::resolveRuleArtifact(const RulePtr &rule, Item *item)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts += artifact;
    artifact->location = item->location();

    if (const auto sourceProperty = item->sourceProperty(QStringLiteral("filePath")))
        artifact->filePathLocation = sourceProperty->location();

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
    if (!m_evaluator->boolValue(item, QLatin1String("condition")))
        return;
    QList<FileTaggerConstPtr> &fileTaggers = m_productContext
            ? m_productContext->product->fileTaggers
            : projectContext->fileTaggers;
    const QStringList patterns = m_evaluator->stringListValue(item, QLatin1String("patterns"));
    if (patterns.isEmpty())
        throw ErrorInfo(Tr::tr("FileTagger.patterns must be a non-empty list."), item->location());

    const FileTags fileTags = m_evaluator->fileTagsValue(item, QLatin1String("fileTags"));
    if (fileTags.isEmpty())
        throw ErrorInfo(Tr::tr("FileTagger.fileTags must not be empty."), item->location());

    for (const QString &pattern : patterns) {
        if (pattern.isEmpty())
            throw ErrorInfo(Tr::tr("A FileTagger pattern must not be empty."), item->location());
    }
    fileTaggers += FileTagger::create(patterns, fileTags);
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

static ModuleLoaderResult::ProductInfo::Dependency extractDependency(
        const ResolvedProductConstPtr &product)
{
    ModuleLoaderResult::ProductInfo::Dependency dependency;
    dependency.name = product->name;
    dependency.profile = product->profile;
    dependency.multiplexConfigurationId = product->multiplexConfigurationId;
    return dependency;
}

ProjectResolver::ProductDependencyInfos ProjectResolver::getProductDependencies(
        const ResolvedProductConstPtr &product, const ModuleLoaderResult::ProductInfo &productInfo)
{
    ProductDependencyInfos result;
    result.dependencies.reserve(productInfo.usedProducts.size());
    QList<ModuleLoaderResult::ProductInfo::Dependency> dependencies = productInfo.usedProducts;
    for (int i = dependencies.count() - 1; i >= 0; --i) {
        const ModuleLoaderResult::ProductInfo::Dependency &dependency = dependencies.at(i);
        QBS_CHECK(dependency.name.isEmpty() != dependency.productTypes.isEmpty());
        if (!dependency.productTypes.isEmpty()) {
            for (const FileTag &tag : dependency.productTypes) {
                const QList<ResolvedProductPtr> productsForTag = m_productsByType.value(tag);
                for (const ResolvedProductPtr &p : productsForTag) {
                    if (p == product || !p->enabled
                            || (dependency.limitToSubProject && !product->isInParentProject(p))) {
                        continue;
                    }
                    result.dependencies.emplace_back(p, dependency.parameters);
                    dependencies << extractDependency(p);
                }
            }
            dependencies.removeAt(i);
        } else if (dependency.profile == QLatin1String("*")) {
            for (const ResolvedProductPtr &p : qAsConst(m_productsByName)) {
                if (p->name != dependency.name || p == product || !p->enabled
                        || (dependency.limitToSubProject && !product->isInParentProject(p))) {
                    continue;
                }
                result.dependencies.emplace_back(p, dependency.parameters);
                dependencies << extractDependency(p);
            }
            dependencies.removeAt(i);
        } else {
            ResolvedProductPtr usedProduct;
            if (!dependency.multiplexConfigurationId.isEmpty()) {
                usedProduct = m_productsByName.value(dependency.uniqueName());
            } else {
                for (const ResolvedProductPtr &p : qAsConst(m_productsByName)) {
                    if (p->name == dependency.name && p->profile == dependency.profile) {
                        usedProduct = p;
                        break;
                    }
                }
            }
            if (!usedProduct) {
                const ResolvedProductConstPtr p = m_productsByName.value(dependency.uniqueName());
                ErrorInfo e(Tr::tr("Product '%1' depends on '%2', which does not exist.")
                            .arg(product->name, dependency.uniqueName()), product->location);
                if (p)
                    e.append(Tr::tr("Requested profile was '%1'.").arg(dependency.profile));
                throw e;
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
            if (artifact->fileTags.intersects(artifactProperties->fileTagsFilter()))
                artifact->properties = artifactProperties->propertyMap();
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

void ProjectResolver::handleError(const ErrorInfo &error)
{
    if (m_setupParams.productErrorMode() == ErrorHandlingMode::Strict)
        throw error;
    m_logger.printWarning(error);
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
            if (!dep.parameters.isEmpty())
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
    product->fileTaggers += projectContext->fileTaggers;
    for (const RulePtr &rule : qAsConst(projectContext->rules))
        product->rules += rule;
}

void ProjectResolver::applyFileTaggers(const ResolvedProductPtr &product) const
{
    for (const SourceArtifactPtr &artifact : product->allEnabledFiles())
        applyFileTaggers(artifact, product, m_logger);
}

void ProjectResolver::applyFileTaggers(const SourceArtifactPtr &artifact,
        const ResolvedProductConstPtr &product, Logger &logger)
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

QVariantMap ProjectResolver::evaluateModuleValues(Item *item, bool lookupPrototype)
{
    AccumulatingTimer modPropEvalTimer(m_setupParams.logElapsedTime()
                                       ? &m_elapsedTimeModPropEval : nullptr);
    QVariantMap moduleValues;
    for (const Item::Module &module : item->modules()) {
        const QString fullName = module.name.toString();
        moduleValues[fullName] = evaluateProperties(module.item, lookupPrototype);
    }

    QVariantMap result;
    result[QLatin1String("modules")] = moduleValues;
    return result;
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
            if (pd.type() == PropertyDeclaration::Path && v.isValid())
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

QVariantMap ProjectResolver::createProductConfig()
{
    EvalCacheEnabler cachingEnabler(m_evaluator);
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
    for (const QString &path : paths)
        result += convertPathProperty(path, dirPath);
    return result;
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
    subProjectContext.project = ResolvedProject::create();
    parentProjectContext->project->subProjects += subProjectContext.project;
    subProjectContext.project->parentProject = parentProjectContext->project;
    return subProjectContext;
}

} // namespace Internal
} // namespace qbs
