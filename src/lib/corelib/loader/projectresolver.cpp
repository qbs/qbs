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

#include "projecttreebuilder.h"

#include <jsextensions/jsextensions.h>
#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/filetags.h>
#include <language/item.h>
#include <language/itempool.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/logger.h>
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
#include <tools/set.h>
#include <tools/setupprojectparameters.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <quickjs.h>

#include <QtCore/qdir.h>
#include <QtCore/qregularexpression.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

namespace qbs {
namespace Internal {

extern bool debugProperties;

static const FileTag unknownFileTag()
{
    static const FileTag tag("unknown-file-tag");
    return tag;
}

struct ProjectContext
{
    ProjectContext *parentContext = nullptr;
    ResolvedProjectPtr project;
    std::vector<FileTaggerConstPtr> fileTaggers;
    std::vector<RulePtr> rules;
    JobLimits jobLimits;
    ResolvedModulePtr dummyModule;
};

using FileLocations = QHash<std::pair<QString, QString>, CodeLocation>;
struct ProductContext
{
    ResolvedProductPtr product;
    QString buildDirectory;
    Item *item = nullptr;
    using ArtifactPropertiesInfo = std::pair<ArtifactPropertiesPtr, std::vector<CodeLocation>>;
    QHash<QStringList, ArtifactPropertiesInfo> artifactPropertiesPerFilter;
    FileLocations sourceArtifactLocations;
    GroupConstPtr currentGroup;
};

struct ModuleContext
{
    ResolvedModulePtr module;
    JobLimits jobLimits;
};

class CancelException { };

class ProjectResolver::Private
{
public:
    Private(const SetupProjectParameters &setupParameters,
            ScriptEngine *engine, Logger &logger)
        : setupParams(setupParameters), engine(engine), evaluator(engine),
          logger(logger)
    {}

    static void applyFileTaggers(const SourceArtifactPtr &artifact,
                                 const ResolvedProductConstPtr &product);
    static SourceArtifactPtr createSourceArtifact(
        const ResolvedProductPtr &rproduct, const QString &fileName, const GroupPtr &group,
        bool wildcard, const CodeLocation &filesLocation = CodeLocation(),
        FileLocations *fileLocations = nullptr, ErrorInfo *errorInfo = nullptr);
    void checkCancelation() const;
    QString verbatimValue(const ValueConstPtr &value, bool *propertyWasSet = nullptr) const;
    QString verbatimValue(Item *item, const QString &name, bool *propertyWasSet = nullptr) const;
    ScriptFunctionPtr scriptFunctionValue(Item *item, const QString &name) const;
    ResolvedFileContextPtr resolvedFileContext(const FileContextConstPtr &ctx) const;
    void ignoreItem(Item *item, ProjectContext *projectContext);
    TopLevelProjectPtr resolveTopLevelProject();
    void resolveProject(Item *item, ProjectContext *projectContext);
    void resolveProjectFully(Item *item, ProjectContext *projectContext);
    void resolveSubProject(Item *item, ProjectContext *projectContext);
    void resolveProduct(Item *item, ProjectContext *projectContext);
    void resolveProductFully(Item *item, ProjectContext *projectContext);
    void resolveModules(const Item *item, ProjectContext *projectContext);
    void resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                       const QVariantMap &parameters, JobLimits &jobLimits,
                       ProjectContext *projectContext);
    void gatherProductTypes(ResolvedProduct *product, Item *item);
    QVariantMap resolveAdditionalModuleProperties(const Item *group,
                                                  const QVariantMap &currentValues);
    void resolveGroup(Item *item, ProjectContext *projectContext);
    void resolveGroupFully(Item *item, ProjectContext *projectContext, bool isEnabled);
    void resolveShadowProduct(Item *item, ProjectContext *);
    void resolveExport(Item *exportItem, ProjectContext *);
    std::unique_ptr<ExportedItem> resolveExportChild(const Item *item,
                                                     const ExportedModule &module);
    void resolveRule(Item *item, ProjectContext *projectContext);
    void resolveRuleArtifact(const RulePtr &rule, Item *item);
    void resolveRuleArtifactBinding(const RuleArtifactPtr &ruleArtifact, Item *item,
                                    const QStringList &namePrefix,
                                    QualifiedIdSet *seenBindings);
    void resolveFileTagger(Item *item, ProjectContext *projectContext);
    void resolveJobLimit(Item *item, ProjectContext *projectContext);
    void resolveScanner(Item *item, ProjectContext *projectContext);
    void resolveProductDependencies();
    void postProcess(const ResolvedProductPtr &product, ProjectContext *projectContext) const;
    void applyFileTaggers(const ResolvedProductPtr &product) const;
    QVariantMap evaluateModuleValues(Item *item, bool lookupPrototype = true);
    QVariantMap evaluateProperties(Item *item, bool lookupPrototype, bool checkErrors);
    QVariantMap evaluateProperties(const Item *item, const Item *propertiesContainer,
                                   const QVariantMap &tmplt, bool lookupPrototype,
                                   bool checkErrors);
    void evaluateProperty(const Item *item, const QString &propName, const ValuePtr &propValue,
                          QVariantMap &result, bool checkErrors);
    void checkAllowedValues(
        const QVariant &v, const CodeLocation &loc, const PropertyDeclaration &decl,
        const QString &key) const;
    void createProductConfig(ResolvedProduct *product);
    ProjectContext createProjectContext(ProjectContext *parentProjectContext) const;
    void adaptExportedPropertyValues(const Item *shadowProductItem);
    void collectExportedProductDependencies();

    struct ProductDependencyInfo
    {
        ProductDependencyInfo(ResolvedProductPtr product,
                              QVariantMap parameters = QVariantMap())
            : product(std::move(product)), parameters(std::move(parameters))
        {
        }

        ResolvedProductPtr product;
        QVariantMap parameters;
    };

    QString sourceCodeAsFunction(const JSSourceValueConstPtr &value,
                                 const PropertyDeclaration &decl) const;
    QString sourceCodeForEvaluation(const JSSourceValueConstPtr &value) const;
    static void matchArtifactProperties(const ResolvedProductPtr &product,
                                        const std::vector<SourceArtifactPtr> &artifacts);
    void printProfilingInfo();

    void collectPropertiesForExportItem(Item *productModuleInstance);
    void collectPropertiesForModuleInExportItem(const Item::Module &module);

    void collectPropertiesForExportItem(const QualifiedId &moduleName,
                                        const ValuePtr &value, Item *moduleInstance, QVariantMap &moduleProps);
    void setupExportedProperties(const Item *item, const QString &namePrefix,
                                 std::vector<ExportedProperty> &properties);

    using ItemFuncPtr = void (ProjectResolver::Private::*)(Item *item,
                                                           ProjectContext *projectContext);
    using ItemFuncMap = QMap<ItemType, ItemFuncPtr>;
    void callItemFunction(const ItemFuncMap &mappings, Item *item, ProjectContext *projectContext);

    const SetupProjectParameters &setupParams;
    ProjectTreeBuilder::Result loadResult;
    ScriptEngine * const engine;
    Evaluator evaluator;
    Logger &logger;
    ItemPool itemPool;
    ProgressObserver *progressObserver = nullptr;
    ProductContext *productContext = nullptr;
    ModuleContext *moduleContext = nullptr;
    QHash<Item *, ResolvedProductPtr> productsByItem;
    QHash<FileTag, QList<ResolvedProductPtr>> productsByType;
    QHash<ResolvedProductPtr, Item *> productItemMap;
    mutable QHash<FileContextConstPtr, ResolvedFileContextPtr> fileContextMap;
    mutable QHash<CodeLocation, ScriptFunctionPtr> scriptFunctionMap;
    mutable QHash<std::pair<QStringView, QStringList>, QString> scriptFunctions;
    mutable QHash<QStringView, QString> sourceCode;
    Set<CodeLocation> groupLocationWarnings;
    std::vector<std::pair<ResolvedProductPtr, Item *>> productExportInfo;
    std::vector<ErrorInfo> queuedErrors;
    std::vector<ProbeConstPtr> oldProjectProbes;
    QHash<QString, std::vector<ProbeConstPtr>> oldProductProbes;
    StoredModuleProviderInfo storedModuleProviderInfo;
    QVariantMap storedProfiles;
    FileTime lastResolveTime;
    qint64 elapsedTimeModPropEval = 0;
    qint64 elapsedTimeAllPropEval = 0;
    qint64 elapsedTimeGroups = 0;
};


ProjectResolver::ProjectResolver(const SetupProjectParameters &setupParameters,
                                 ScriptEngine *engine, Logger &logger)
    : d(new Private(setupParameters, engine, logger))
{
    QBS_CHECK(FileInfo::isAbsolute(d->setupParams.buildRoot()));
}

ProjectResolver::~ProjectResolver()
{
    delete d;
}

void ProjectResolver::setProgressObserver(ProgressObserver *observer)
{
    d->progressObserver = observer;
}

void ProjectResolver::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    d->oldProjectProbes = oldProbes;
}

void ProjectResolver::setOldProductProbes(
    const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    d->oldProductProbes = oldProbes;
}

void ProjectResolver::setLastResolveTime(const FileTime &time)
{
    d->lastResolveTime = time;
}

void ProjectResolver::setStoredProfiles(const QVariantMap &profiles)
{
    d->storedProfiles = profiles;
}

void ProjectResolver::setStoredModuleProviderInfo(const StoredModuleProviderInfo &providerInfo)
{
    d->storedModuleProviderInfo = providerInfo;
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
    TimedActivityLogger projectResolverTimer(d->logger, Tr::tr("ProjectResolver"),
                                             d->setupParams.logElapsedTime());
    qCDebug(lcProjectResolver) << "resolving" << d->setupParams.projectFilePath();

    ProjectTreeBuilder projectTreeBuilder(d->setupParams, d->itemPool, d->evaluator, d->logger);
    projectTreeBuilder.setProgressObserver(d->progressObserver);
    projectTreeBuilder.setOldProjectProbes(d->oldProjectProbes);
    projectTreeBuilder.setOldProductProbes(d->oldProductProbes);
    projectTreeBuilder.setLastResolveTime(d->lastResolveTime);
    projectTreeBuilder.setStoredProfiles(d->storedProfiles);
    projectTreeBuilder.setStoredModuleProviderInfo(d->storedModuleProviderInfo);
    d->loadResult = projectTreeBuilder.load();

    TopLevelProjectPtr tlp;
    try {
        tlp = d->resolveTopLevelProject();
        d->printProfilingInfo();
    } catch (const CancelException &) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration '%1'.")
                            .arg(TopLevelProject::deriveId(
                                d->setupParams.finalBuildConfigurationTree())));
    }
    return tlp;
}

void ProjectResolver::Private::checkCancelation() const
{
    if (progressObserver && progressObserver->canceled())
        throw CancelException();
}

QString ProjectResolver::Private::verbatimValue(
    const ValueConstPtr &value, bool *propertyWasSet) const
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

QString ProjectResolver::Private::verbatimValue(Item *item, const QString &name,
                                                bool *propertyWasSet) const
{
    return verbatimValue(item->property(name), propertyWasSet);
}

void ProjectResolver::Private::ignoreItem(Item *item, ProjectContext *projectContext)
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

TopLevelProjectPtr ProjectResolver::Private::resolveTopLevelProject()
{
    if (progressObserver)
        progressObserver->setMaximum(int(loadResult.productInfos.size()));
    TopLevelProjectPtr project = TopLevelProject::create();
    project->buildDirectory = TopLevelProject::deriveBuildDirectory(
        setupParams.buildRoot(),
        TopLevelProject::deriveId(setupParams.finalBuildConfigurationTree()));
    project->buildSystemFiles = loadResult.qbsFiles;
    project->profileConfigs = loadResult.profileConfigs;
    project->probes = loadResult.projectProbes;
    project->moduleProviderInfo = loadResult.storedModuleProviderInfo;
    ProjectContext projectContext;
    projectContext.project = project;

    resolveProject(loadResult.root, &projectContext);
    ErrorInfo accumulatedErrors;
    for (const ErrorInfo &e : queuedErrors)
        appendError(accumulatedErrors, e);
    if (accumulatedErrors.hasError())
        throw accumulatedErrors;

    project->setBuildConfiguration(setupParams.finalBuildConfigurationTree());
    project->overriddenValues = setupParams.overriddenValues();
    project->canonicalFilePathResults = engine->canonicalFilePathResults();
    project->fileExistsResults = engine->fileExistsResults();
    project->directoryEntriesResults = engine->directoryEntriesResults();
    project->fileLastModifiedResults = engine->fileLastModifiedResults();
    project->environment = engine->environment();
    project->buildSystemFiles.unite(engine->imports());
    makeSubProjectNamesUniqe(project);
    resolveProductDependencies();
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
    project->warningsEncountered = logger.warnings();
    return project;
}

void ProjectResolver::Private::resolveProject(Item *item, ProjectContext *projectContext)
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
        if (setupParams.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        logger.printWarning(error);
    }
}

void ProjectResolver::Private::resolveProjectFully(Item *item, ProjectContext *projectContext)
{
    projectContext->project->enabled = projectContext->project->enabled
        && evaluator.boolValue(item, StringConstants::conditionProperty());
    projectContext->project->name = evaluator.stringValue(item, StringConstants::nameProperty());
    if (projectContext->project->name.isEmpty())
        projectContext->project->name = FileInfo::baseName(item->location().filePath()); // FIXME: Must also be changed in item?
    QVariantMap projectProperties;
    if (!projectContext->project->enabled) {
        projectProperties.insert(StringConstants::profileProperty(),
                                 evaluator.stringValue(item, StringConstants::profileProperty()));
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
        const ScopedJsValue sv(engine->context(), evaluator.value(item, it.key()));
        projectProperties.insert(it.key(), getJsVariant(engine->context(), sv));
    }
    projectContext->project->setProjectProperties(projectProperties);

    static const ItemFuncMap mapping = {
        { ItemType::Project, &ProjectResolver::Private::resolveProject },
        { ItemType::SubProject, &ProjectResolver::Private::resolveSubProject },
        { ItemType::Product, &ProjectResolver::Private::resolveProduct },
        { ItemType::Probe, &ProjectResolver::Private::ignoreItem },
        { ItemType::FileTagger, &ProjectResolver::Private::resolveFileTagger },
        { ItemType::JobLimit, &ProjectResolver::Private::resolveJobLimit },
        { ItemType::Rule, &ProjectResolver::Private::resolveRule },
        { ItemType::PropertyOptions, &ProjectResolver::Private::ignoreItem }
    };

    for (Item * const child : item->children()) {
        try {
            callItemFunction(mapping, child, projectContext);
        } catch (const ErrorInfo &e) {
            queuedErrors.push_back(e);
        }
    }

    for (const ResolvedProductPtr &product : projectContext->project->products)
        postProcess(product, projectContext);
}

void ProjectResolver::Private::resolveSubProject(Item *item, ProjectContext *projectContext)
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
        subProjectContext.project->name = evaluator.stringValue(
            propertiesItem, StringConstants::nameProperty());
    }
}

void ProjectResolver::Private::resolveProduct(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    evaluator.clearPropertyDependencies();
    ProductContext productContext;
    productContext.item = item;
    ResolvedProductPtr product = ResolvedProduct::create();
    product->enabled = projectContext->project->enabled;
    product->moduleProperties = PropertyMapInternal::create();
    product->project = projectContext->project;
    productContext.product = product;
    product->location = item->location();
    class ProductContextSwitcher {
    public:
        ProductContextSwitcher(ProjectResolver::Private &resolver, ProductContext &newContext,
                               ProgressObserver *progressObserver)
            : m_resolver(resolver), m_progressObserver(progressObserver) {
            QBS_CHECK(!m_resolver.productContext);
            m_resolver.productContext = &newContext;
        }
        ~ProductContextSwitcher() {
            if (m_progressObserver)
                m_progressObserver->incrementProgressValue();
            m_resolver.productContext = nullptr;
        }
    private:
        ProjectResolver::Private &m_resolver;
        ProgressObserver * const m_progressObserver;
    } contextSwitcher(*this, productContext, progressObserver);
    const auto errorFromDelayedError = [&] {
        ProjectTreeBuilder::Result::ProductInfo &pi = loadResult.productInfos[item];
        if (pi.delayedError.hasError()) {
            ErrorInfo errorInfo;

            // First item is "main error", gets prepended again in the catch clause.
            const QList<ErrorItem> &items = pi.delayedError.items();
            for (int i = 1; i < items.size(); ++i)
                errorInfo.append(items.at(i));

            pi.delayedError.clear();
            return errorInfo;
        }
        return ErrorInfo();
    };

    // Even if we previously encountered an error, try to continue for as long as possible
    // to provide IDEs with useful data (e.g. the list of files).
    // If we encounter a follow-up error, suppress it and report the original one instead.
    try {
        resolveProductFully(item, projectContext);
        if (const ErrorInfo error = errorFromDelayedError(); error.hasError())
            throw error;
    } catch (ErrorInfo e) {
        if (const ErrorInfo error = errorFromDelayedError(); error.hasError())
            e = error;
        QString mainErrorString = !product->name.isEmpty()
                ? Tr::tr("Error while handling product '%1':").arg(product->name)
                : Tr::tr("Error while handling product:");
        ErrorInfo fullError(mainErrorString, item->location());
        appendError(fullError, e);
        if (!product->enabled) {
            qCDebug(lcProjectResolver) << fullError.toString();
            return;
        }
        if (setupParams.productErrorMode() == ErrorHandlingMode::Strict)
            throw fullError;
        logger.printWarning(fullError);
        logger.printWarning(ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                                          .arg(product->name), item->location()));
        product->enabled = false;
    }
}

void ProjectResolver::Private::resolveProductFully(Item *item, ProjectContext *projectContext)
{
    const ResolvedProductPtr product = productContext->product;
    productItemMap.insert(product, item);
    projectContext->project->products.push_back(product);
    product->name = evaluator.stringValue(item, StringConstants::nameProperty());

    // product->buildDirectory() isn't valid yet, because the productProperties map is not ready.
    productContext->buildDirectory
        = evaluator.stringValue(item, StringConstants::buildDirectoryProperty());
    product->multiplexConfigurationId
        = evaluator.stringValue(item, StringConstants::multiplexConfigurationIdProperty());
    qCDebug(lcProjectResolver) << "resolveProduct" << product->uniqueName();
    productsByItem.insert(item, product);
    product->enabled = product->enabled
                       && evaluator.boolValue(item, StringConstants::conditionProperty());
    ProjectTreeBuilder::Result::ProductInfo &pi = loadResult.productInfos[item];
    gatherProductTypes(product.get(), item);
    product->targetName = evaluator.stringValue(item, StringConstants::targetNameProperty());
    product->sourceDirectory = evaluator.stringValue(
                item, StringConstants::sourceDirectoryProperty());
    product->destinationDirectory = evaluator.stringValue(
                item, StringConstants::destinationDirProperty());

    if (product->destinationDirectory.isEmpty()) {
        product->destinationDirectory = productContext->buildDirectory;
    } else {
        product->destinationDirectory = FileInfo::resolvePath(
                    product->topLevelProject()->buildDirectory,
                    product->destinationDirectory);
    }
    product->probes = pi.probes;
    createProductConfig(product.get());
    product->productProperties.insert(StringConstants::destinationDirProperty(),
                                      product->destinationDirectory);
    ModuleProperties::init(evaluator.engine(), evaluator.scriptValue(item), product.get());

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
        fakeGroup->setupForBuiltinType(setupParams.deprecationWarningMode(), logger);
        subItems.prepend(fakeGroup);
    }

    static const ItemFuncMap mapping = {
        { ItemType::Depends, &ProjectResolver::Private::ignoreItem },
        { ItemType::Rule, &ProjectResolver::Private::resolveRule },
        { ItemType::FileTagger, &ProjectResolver::Private::resolveFileTagger },
        { ItemType::JobLimit, &ProjectResolver::Private::resolveJobLimit },
        { ItemType::Group, &ProjectResolver::Private::resolveGroup },
        { ItemType::Product, &ProjectResolver::Private::resolveShadowProduct },
        { ItemType::Export, &ProjectResolver::Private::resolveExport },
        { ItemType::Probe, &ProjectResolver::Private::ignoreItem },
        { ItemType::PropertyOptions, &ProjectResolver::Private::ignoreItem }
    };

    for (Item * const child : qAsConst(subItems))
        callItemFunction(mapping, child, projectContext);

    for (const ProjectContext *p = projectContext; p; p = p->parentContext) {
        JobLimits tempLimits = p->jobLimits;
        product->jobLimits = tempLimits.update(product->jobLimits);
    }

    resolveModules(item, projectContext);

    for (const FileTag &t : qAsConst(product->fileTags))
        productsByType[t].push_back(product);
}

void ProjectResolver::Private::resolveModules(const Item *item, ProjectContext *projectContext)
{
    JobLimits jobLimits;
    for (const Item::Module &m : item->modules()) {
        resolveModule(m.name, m.item, m.productInfo.has_value(), m.parameters, jobLimits,
                      projectContext);
    }
    for (int i = 0; i < jobLimits.count(); ++i) {
        const JobLimit &moduleJobLimit = jobLimits.jobLimitAt(i);
        if (productContext->product->jobLimits.getLimit(moduleJobLimit.pool()) == -1)
            productContext->product->jobLimits.setJobLimit(moduleJobLimit);
    }
}

void ProjectResolver::Private::resolveModule(
    const QualifiedId &moduleName, Item *item, bool isProduct, const QVariantMap &parameters,
    JobLimits &jobLimits, ProjectContext *projectContext)
{
    checkCancelation();
    if (!item->isPresentModule())
        return;

    ModuleContext * const oldModuleContext = moduleContext;
    ModuleContext newModuleContext;
    newModuleContext.module = ResolvedModule::create();
    moduleContext = &newModuleContext;

    const ResolvedModulePtr &module = newModuleContext.module;
    module->name = moduleName.toString();
    module->isProduct = isProduct;
    module->product = productContext->product.get();
    module->setupBuildEnvironmentScript.initialize(
                scriptFunctionValue(item, StringConstants::setupBuildEnvironmentProperty()));
    module->setupRunEnvironmentScript.initialize(
                scriptFunctionValue(item, StringConstants::setupRunEnvironmentProperty()));

    for (const Item::Module &m : item->modules()) {
        if (m.item->isPresentModule())
            module->moduleDependencies += m.name.toString();
    }

    productContext->product->modules.push_back(module);
    if (!parameters.empty())
        productContext->product->moduleParameters[module] = parameters;

    static const ItemFuncMap mapping {
        { ItemType::Group, &ProjectResolver::Private::ignoreItem },
        { ItemType::Rule, &ProjectResolver::Private::resolveRule },
        { ItemType::FileTagger, &ProjectResolver::Private::resolveFileTagger },
        { ItemType::JobLimit, &ProjectResolver::Private::resolveJobLimit },
        { ItemType::Scanner, &ProjectResolver::Private::resolveScanner },
        { ItemType::PropertyOptions, &ProjectResolver::Private::ignoreItem },
        { ItemType::Depends, &ProjectResolver::Private::ignoreItem },
        { ItemType::Parameter, &ProjectResolver::Private::ignoreItem },
        { ItemType::Properties, &ProjectResolver::Private::ignoreItem },
        { ItemType::Probe, &ProjectResolver::Private::ignoreItem }
    };
    for (Item *child : item->children())
        callItemFunction(mapping, child, projectContext);
    for (int i = 0; i < newModuleContext.jobLimits.count(); ++i) {
        const JobLimit &newJobLimit = newModuleContext.jobLimits.jobLimitAt(i);
        const int oldLimit = jobLimits.getLimit(newJobLimit.pool());
        if (oldLimit == -1 || oldLimit > newJobLimit.limit())
            jobLimits.setJobLimit(newJobLimit);
    }

    moduleContext = oldModuleContext; // FIXME: Exception safety
}

void ProjectResolver::Private::gatherProductTypes(ResolvedProduct *product, Item *item)
{
    const VariantValuePtr type = item->variantProperty(StringConstants::typeProperty());
    if (type)
        product->fileTags = FileTags::fromStringList(type->value().toStringList());
}

SourceArtifactPtr ProjectResolver::Private::createSourceArtifact(
    const ResolvedProductPtr &rproduct, const QString &fileName, const GroupPtr &group,
    bool wildcard, const CodeLocation &filesLocation, FileLocations *fileLocations,
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

static QualifiedIdSet propertiesToEvaluate(std::deque<QualifiedId> initialProps,
                                           const PropertyDependencies &deps)
{
    std::deque<QualifiedId> remainingProps = std::move(initialProps);
    QualifiedIdSet allProperties;
    while (!remainingProps.empty()) {
        const QualifiedId prop = remainingProps.front();
        remainingProps.pop_front();
        const auto insertResult = allProperties.insert(prop);
        if (!insertResult.second)
            continue;
        transform(deps.value(prop), remainingProps, [](const QualifiedId &id) { return id; });
    }
    return allProperties;
}

QVariantMap ProjectResolver::Private::resolveAdditionalModuleProperties(
    const Item *group, const QVariantMap &currentValues)
{
    // Step 1: Retrieve the properties directly set in the group
    const ModulePropertiesPerGroup &mp = mapValue(
            loadResult.productInfos, productContext->item).modulePropertiesSetInGroups;
    const auto it = mp.find(group);
    if (it == mp.end())
        return {};
    const QualifiedIdSet &propsSetInGroup = it->second;

    // Step 2: Gather all properties that depend on these properties.
    const QualifiedIdSet &propsToEval = propertiesToEvaluate(
        rangeTo<std::deque<QualifiedId>>(propsSetInGroup),
        evaluator.propertyDependencies());

    // Step 3: Evaluate all these properties and replace their values in the map
    QVariantMap modulesMap = currentValues;
    QHash<QString, QStringList> propsPerModule;
    for (auto fullPropName : propsToEval) {
        const QString moduleName
                = QualifiedId(fullPropName.mid(0, fullPropName.size() - 1)).toString();
        propsPerModule[moduleName] << fullPropName.last();
    }
    EvalCacheEnabler cachingEnabler(&evaluator, productContext->product->sourceDirectory);
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
    return modulesMap;
}

void ProjectResolver::Private::resolveGroup(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    const bool parentEnabled = productContext->currentGroup
            ? productContext->currentGroup->enabled
            : productContext->product->enabled;
    const bool isEnabled = parentEnabled
                           && evaluator.boolValue(item, StringConstants::conditionProperty());
    try {
        resolveGroupFully(item, projectContext, isEnabled);
    } catch (const ErrorInfo &error) {
        if (!isEnabled) {
            qCDebug(lcProjectResolver) << "error resolving group at" << item->location()
                                       << error.toString();
            return;
        }
        if (setupParams.productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        logger.printWarning(error);
    }
}

void ProjectResolver::Private::resolveGroupFully(
    Item *item, ProjectContext *projectContext, bool isEnabled)
{
    AccumulatingTimer groupTimer(setupParams.logElapsedTime() ? &elapsedTimeGroups : nullptr);

    const auto getGroupPropertyMap = [this, item](const ArtifactProperties *existingProps) {
        PropertyMapPtr moduleProperties;
        bool newPropertyMapRequired = false;
        if (existingProps)
            moduleProperties = existingProps->propertyMap();
        if (!moduleProperties) {
            newPropertyMapRequired = true;
            moduleProperties = productContext->currentGroup
                    ? productContext->currentGroup->properties
                    : productContext->product->moduleProperties;
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

    QStringList files = evaluator.stringListValue(item, StringConstants::filesProperty());
    bool fileTagsSet;
    const FileTags fileTags = evaluator.fileTagsValue(item, StringConstants::fileTagsProperty(),
                                                      &fileTagsSet);
    const QStringList fileTagsFilter
        = evaluator.stringListValue(item, StringConstants::fileTagsFilterProperty());
    if (!fileTagsFilter.empty()) {
        if (Q_UNLIKELY(!files.empty()))
            throw ErrorInfo(Tr::tr("Group.files and Group.fileTagsFilters are exclusive."),
                        item->location());

        if (!isEnabled)
            return;

        ProductContext::ArtifactPropertiesInfo &apinfo
                = productContext->artifactPropertiesPerFilter[fileTagsFilter];
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
            productContext->product->artifactProperties.push_back(apinfo.first);
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
    group->prefix = evaluator.stringValue(item, StringConstants::prefixProperty(), QString(),
                                          &prefixWasSet);
    if (!prefixWasSet && productContext->currentGroup)
        group->prefix = productContext->currentGroup->prefix;
    if (!group->prefix.isEmpty()) {
        for (auto it = files.rbegin(), end = files.rend(); it != end; ++it)
            it->prepend(group->prefix);
    }
    group->location = item->location();
    group->enabled = isEnabled;
    group->properties = getGroupPropertyMap(nullptr);
    group->fileTags = fileTags;
    group->overrideTags = evaluator.boolValue(item, StringConstants::overrideTagsProperty());
    if (group->overrideTags && fileTagsSet) {
        if (group->fileTags.empty() )
            group->fileTags.insert(unknownFileTag());
    } else if (productContext->currentGroup) {
        group->fileTags.unite(productContext->currentGroup->fileTags);
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
        wildcards->excludePatterns = evaluator.stringListValue(
            item, StringConstants::excludeFilesProperty());
        wildcards->patterns = patterns;
        const Set<QString> files = wildcards->expandPatterns(group,
                FileInfo::path(item->file()->filePath()),
                projectContext->project->topLevelProject()->buildDirectory);
        for (const QString &fileName : files)
            createSourceArtifact(productContext->product, fileName, group, true, filesLocation,
                                 &productContext->sourceArtifactLocations, &fileError);
    }

    for (const QString &fileName : qAsConst(files)) {
        createSourceArtifact(productContext->product, fileName, group, false, filesLocation,
                             &productContext->sourceArtifactLocations, &fileError);
    }
    if (fileError.hasError()) {
        if (group->enabled) {
            if (setupParams.productErrorMode() == ErrorHandlingMode::Strict)
                throw ErrorInfo(fileError);
            logger.printWarning(fileError);
        } else {
            qCDebug(lcProjectResolver) << "error for disabled group:" << fileError.toString();
        }
    }
    group->name = evaluator.stringValue(item, StringConstants::nameProperty());
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(productContext->product->groups.size());
    productContext->product->groups.push_back(group);

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
    GroupContextSwitcher groupSwitcher(*productContext, group);
    for (Item * const childItem : item->children())
        resolveGroup(childItem, projectContext);
}

void ProjectResolver::Private::adaptExportedPropertyValues(const Item *shadowProductItem)
{
    ExportedModule &m = productContext->product->exportedModule;
    const QVariantList prefixList = m.propertyValues.take(
                StringConstants::prefixMappingProperty()).toList();
    const QString shadowProductName = evaluator.stringValue(
                shadowProductItem, StringConstants::nameProperty());
    const QString shadowProductBuildDir = evaluator.stringValue(
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
        switch (static_cast<QMetaType::Type>(value.userType())) {
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

void ProjectResolver::Private::collectExportedProductDependencies()
{
    ResolvedProductPtr dummyProduct = ResolvedProduct::create();
    dummyProduct->enabled = false;
    for (const auto &exportingProductInfo : qAsConst(productExportInfo)) {
        const ResolvedProductPtr exportingProduct = exportingProductInfo.first;
        if (!exportingProduct->enabled)
            continue;
        Item * const importingProductItem = exportingProductInfo.second;

        std::vector<std::pair<ResolvedProductPtr, QVariantMap>> directDeps;
        for (const Item::Module &m : importingProductItem->modules()) {
            if (m.name.toString() != exportingProduct->name)
                continue;
            for (const Item::Module &dep : m.item->modules()) {
                if (dep.productInfo) {
                    directDeps.emplace_back(productsByItem.value(dep.productInfo->item),
                                            m.parameters);
                }
            }
        }
        for (const auto &dep : directDeps) {
            if (!contains(exportingProduct->exportedModule.productDependencies,
                          dep.first->uniqueName())) {
                exportingProduct->exportedModule.productDependencies.push_back(
                    dep.first->uniqueName());
            }
            if (!dep.second.isEmpty()) {
                exportingProduct->exportedModule.dependencyParameters.insert(dep.first,
                                                                             dep.second);
            }
        }
        auto &productDeps = exportingProduct->exportedModule.productDependencies;
        std::sort(productDeps.begin(), productDeps.end());
    }
}

void ProjectResolver::Private::resolveShadowProduct(Item *item, ProjectContext *)
{
    if (!productContext->product->enabled)
        return;
    for (const auto &m : item->modules()) {
        if (m.name.toString() != productContext->product->name)
            continue;
        collectPropertiesForExportItem(m.item);
        for (const auto &dep : m.item->modules())
            collectPropertiesForModuleInExportItem(dep);
        break;
    }
    try {
        adaptExportedPropertyValues(item);
    } catch (const ErrorInfo &) {}
    productExportInfo.emplace_back(productContext->product, item);
}

void ProjectResolver::Private::setupExportedProperties(
    const Item *item, const QString &namePrefix, std::vector<ExportedProperty> &properties)
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
        if (!exportedProperty.isBuiltin || evaluator.isNonDefaultValue(item, it.key()))
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

static bool usesImport(const ExportedProperty &prop, const QRegularExpression &regex)
{
    return prop.sourceCode.indexOf(regex) != -1;
}

static bool usesImport(const ExportedItem &item, const QRegularExpression &regex)
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

    const QRegularExpression regex(pattern.arg(name)); // std::regex is much slower
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

void ProjectResolver::Private::resolveExport(Item *exportItem, ProjectContext *)
{
    ExportedModule &exportedModule = productContext->product->exportedModule;
    setupExportedProperties(exportItem, QString(), exportedModule.m_properties);
    static const auto cmpFunc = [](const ExportedProperty &p1, const ExportedProperty &p2) {
        return p1.fullName < p2.fullName;
    };
    std::sort(exportedModule.m_properties.begin(), exportedModule.m_properties.end(), cmpFunc);

    transform(exportItem->children(), exportedModule.children,
              [&exportedModule, this](const auto &child) {
        return resolveExportChild(child, exportedModule); });

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
std::unique_ptr<ExportedItem> ProjectResolver::Private::resolveExportChild(
    const Item *item, const ExportedModule &module)
{
    std::unique_ptr<ExportedItem> exportedItem(new ExportedItem);

    // This is the type of the built-in base item. It may turn out that we need to support
    // derived items under Export. In that case, we probably need a new Item member holding
    // the original type name.
    exportedItem->name = item->typeName();

    transform(item->children(), exportedItem->children, [&module, this](const auto &child) {
        return resolveExportChild(child, module); });

    setupExportedProperties(item, QString(), exportedItem->properties);
    return exportedItem;
}

QString ProjectResolver::Private::sourceCodeAsFunction(const JSSourceValueConstPtr &value,
                                                       const PropertyDeclaration &decl) const
{
    QString &scriptFunction = scriptFunctions[std::make_pair(value->sourceCode(),
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

QString ProjectResolver::Private::sourceCodeForEvaluation(const JSSourceValueConstPtr &value) const
{
    QString &code = sourceCode[value->sourceCode()];
    if (!code.isNull())
        return code;
    code = value->sourceCodeForEvaluation();
    return code;
}

ScriptFunctionPtr ProjectResolver::Private::scriptFunctionValue(
    Item *item, const QString &name) const
{
    JSSourceValuePtr value = item->sourceProperty(name);
    ScriptFunctionPtr &script = scriptFunctionMap[value ? value->location() : CodeLocation()];
    if (!script.get()) {
        script = ScriptFunction::create();
        const PropertyDeclaration decl = item->propertyDeclaration(name);
        script->sourceCode = sourceCodeAsFunction(value, decl);
        script->location = value->location();
        script->fileContext = resolvedFileContext(value->file());
    }
    return script;
}

ResolvedFileContextPtr ProjectResolver::Private::resolvedFileContext(
    const FileContextConstPtr &ctx) const
{
    ResolvedFileContextPtr &result = fileContextMap[ctx];
    if (!result)
        result = ResolvedFileContext::create(*ctx);
    return result;
}

void ProjectResolver::Private::resolveRule(Item *item, ProjectContext *projectContext)
{
    checkCancelation();

    if (!evaluator.boolValue(item, StringConstants::conditionProperty()))
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

    rule->name = evaluator.stringValue(item, StringConstants::nameProperty());
    rule->prepareScript.initialize(
                scriptFunctionValue(item, StringConstants::prepareProperty()));
    rule->outputArtifactsScript.initialize(
                scriptFunctionValue(item, StringConstants::outputArtifactsProperty()));
    rule->outputFileTags = evaluator.fileTagsValue(item, StringConstants::outputFileTagsProperty());
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
    rule->multiplex = evaluator.boolValue(item, StringConstants::multiplexProperty());
    rule->alwaysRun = evaluator.boolValue(item, StringConstants::alwaysRunProperty());
    rule->inputs = evaluator.fileTagsValue(item, StringConstants::inputsProperty());
    rule->inputsFromDependencies
        = evaluator.fileTagsValue(item, StringConstants::inputsFromDependenciesProperty());
    bool requiresInputsSet = false;
    rule->requiresInputs = evaluator.boolValue(item, StringConstants::requiresInputsProperty(),
                                               &requiresInputsSet);
    if (!requiresInputsSet)
        rule->requiresInputs = rule->declaresInputs();
    rule->auxiliaryInputs
        = evaluator.fileTagsValue(item, StringConstants::auxiliaryInputsProperty());
    rule->excludedInputs
        = evaluator.fileTagsValue(item, StringConstants::excludedInputsProperty());
    if (rule->excludedInputs.empty()) {
        rule->excludedInputs = evaluator.fileTagsValue(
                    item, StringConstants::excludedAuxiliaryInputsProperty());
    }
    rule->explicitlyDependsOn
        = evaluator.fileTagsValue(item, StringConstants::explicitlyDependsOnProperty());
    rule->explicitlyDependsOnFromDependencies = evaluator.fileTagsValue(
                item, StringConstants::explicitlyDependsOnFromDependenciesProperty());
    rule->module = moduleContext ? moduleContext->module : projectContext->dummyModule;
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
    if (productContext) {
        rule->product = productContext->product.get();
        productContext->product->rules.push_back(rule);
    } else {
        projectContext->rules.push_back(rule);
    }
}

void ProjectResolver::Private::resolveRuleArtifact(const RulePtr &rule, Item *item)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts.push_back(artifact);
    artifact->location = item->location();

    if (const auto sourceProperty = item->sourceProperty(StringConstants::filePathProperty()))
        artifact->filePathLocation = sourceProperty->location();

    artifact->filePath = verbatimValue(item, StringConstants::filePathProperty());
    artifact->fileTags = evaluator.fileTagsValue(item, StringConstants::fileTagsProperty());
    artifact->alwaysUpdated = evaluator.boolValue(item,
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

void ProjectResolver::Private::resolveRuleArtifactBinding(
    const RuleArtifactPtr &ruleArtifact, Item *item, const QStringList &namePrefix,
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

void ProjectResolver::Private::resolveFileTagger(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty()))
        return;
    std::vector<FileTaggerConstPtr> &fileTaggers = productContext
            ? productContext->product->fileTaggers
            : projectContext->fileTaggers;
    const QStringList patterns = evaluator.stringListValue(item,
                                                           StringConstants::patternsProperty());
    if (patterns.empty())
        throw ErrorInfo(Tr::tr("FileTagger.patterns must be a non-empty list."), item->location());

    const FileTags fileTags = evaluator.fileTagsValue(item, StringConstants::fileTagsProperty());
    if (fileTags.empty())
        throw ErrorInfo(Tr::tr("FileTagger.fileTags must not be empty."), item->location());

    for (const QString &pattern : patterns) {
        if (pattern.isEmpty())
            throw ErrorInfo(Tr::tr("A FileTagger pattern must not be empty."), item->location());
    }

    const int priority = evaluator.intValue(item, StringConstants::priorityProperty());
    fileTaggers.push_back(FileTagger::create(patterns, fileTags, priority));
}

void ProjectResolver::Private::resolveJobLimit(Item *item, ProjectContext *projectContext)
{
    if (!evaluator.boolValue(item, StringConstants::conditionProperty()))
        return;
    const QString jobPool = evaluator.stringValue(item, StringConstants::jobPoolProperty());
    if (jobPool.isEmpty())
        throw ErrorInfo(Tr::tr("A JobLimit item needs to have a non-empty '%1' property.")
                        .arg(StringConstants::jobPoolProperty()), item->location());
    bool jobCountWasSet;
    const int jobCount = evaluator.intValue(item, StringConstants::jobCountProperty(), -1,
                                            &jobCountWasSet);
    if (!jobCountWasSet) {
        throw ErrorInfo(Tr::tr("A JobLimit item needs to have a '%1' property.")
                        .arg(StringConstants::jobCountProperty()), item->location());
    }
    if (jobCount < 0) {
        throw ErrorInfo(Tr::tr("A JobLimit item must have a non-negative '%1' property.")
                        .arg(StringConstants::jobCountProperty()), item->location());
    }
    JobLimits &jobLimits = moduleContext
            ? moduleContext->jobLimits
            : productContext ? productContext->product->jobLimits
            : projectContext->jobLimits;
    JobLimit jobLimit(jobPool, jobCount);
    const int oldLimit = jobLimits.getLimit(jobPool);
    if (oldLimit == -1 || oldLimit > jobCount)
        jobLimits.setJobLimit(jobLimit);
}

void ProjectResolver::Private::resolveScanner(Item *item, ProjectContext *projectContext)
{
    checkCancelation();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty())) {
        qCDebug(lcProjectResolver) << "scanner condition is false";
        return;
    }

    ResolvedScannerPtr scanner = ResolvedScanner::create();
    scanner->module = moduleContext ? moduleContext->module : projectContext->dummyModule;
    scanner->inputs = evaluator.fileTagsValue(item, StringConstants::inputsProperty());
    scanner->recursive = evaluator.boolValue(item, StringConstants::recursiveProperty());
    scanner->searchPathsScript.initialize(
                scriptFunctionValue(item, StringConstants::searchPathsProperty()));
    scanner->scanScript.initialize(
                scriptFunctionValue(item, StringConstants::scanProperty()));
    productContext->product->scanners.push_back(scanner);
}

void ProjectResolver::Private::matchArtifactProperties(const ResolvedProductPtr &product,
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

void ProjectResolver::Private::printProfilingInfo()
{
    if (!setupParams.logElapsedTime())
        return;
    logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("All property evaluation took %1.")
                                         .arg(elapsedTimeString(elapsedTimeAllPropEval));
    logger.qbsLog(LoggerInfo, true) << "\t" << Tr::tr("Module property evaluation took %1.")
                                         .arg(elapsedTimeString(elapsedTimeModPropEval));
    logger.qbsLog(LoggerInfo, true) << "\t"
                                      << Tr::tr("Resolving groups (without module property "
                                                "evaluation) took %1.")
                                         .arg(elapsedTimeString(elapsedTimeGroups));
}

class TempScopeSetter
{
public:
    TempScopeSetter(const ValuePtr &value, Item *newScope) : m_value(value), m_oldScope(value->scope())
    {
        value->setScope(newScope, {});
    }
    ~TempScopeSetter() { if (m_value) m_value->setScope(m_oldScope, {}); }

    TempScopeSetter(const TempScopeSetter &) = delete;
    TempScopeSetter &operator=(const TempScopeSetter &) = delete;
    TempScopeSetter &operator=(TempScopeSetter &&) = delete;

    TempScopeSetter(TempScopeSetter &&other) noexcept
        : m_value(std::move(other.m_value)), m_oldScope(other.m_oldScope)
    {
        other.m_value.reset();
        other.m_oldScope = nullptr;
    }

private:
    ValuePtr m_value;
    Item *m_oldScope;
};

void ProjectResolver::Private::collectPropertiesForExportItem(Item *productModuleInstance)
{
    if (!productModuleInstance->isPresentModule())
        return;
    Item * const exportItem = productModuleInstance->prototype();
    QBS_CHECK(exportItem);
    QBS_CHECK(exportItem->type() == ItemType::Export);
    const ItemDeclaration::Properties exportDecls = BuiltinDeclarations::instance()
            .declarationsForType(ItemType::Export).properties();
    ExportedModule &exportedModule = productContext->product->exportedModule;
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
            TempScopeSetter tss(it.value(), productModuleInstance);
            evaluateProperty(exportItem, it.key(), it.value(), exportedModule.propertyValues,
                             false);
        }
    }
}

// Collects module properties assigned to in other (higher-level) modules.
void ProjectResolver::Private::collectPropertiesForModuleInExportItem(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    ExportedModule &exportedModule = productContext->product->exportedModule;
    if (module.productInfo || module.name.first() == StringConstants::qbsModule())
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

void ProjectResolver::Private::resolveProductDependencies()
{
    for (auto it = productsByItem.cbegin(); it != productsByItem.cend(); ++it) {
        const ResolvedProductPtr &product = it.value();
        for (const Item::Module &module : it.key()->modules()) {
            if (!module.productInfo)
                continue;
            const ResolvedProductPtr &dep = productsByItem.value(module.productInfo->item);
            QBS_CHECK(dep);
            QBS_CHECK(dep != product);
            it.value()->dependencies << dep;
            it.value()->dependencyParameters.insert(dep, module.parameters); // TODO: Streamline this with normal module dependencies?
        }

        // TODO: We might want to keep the topological sorting and get rid of "module module dependencies".
        std::sort(product->dependencies.begin(),product->dependencies.end(),
                  [](const ResolvedProductPtr &p1, const ResolvedProductPtr &p2) {
            return p1->fullDisplayName() < p2->fullDisplayName();
        });
    }
}

void ProjectResolver::Private::postProcess(const ResolvedProductPtr &product,
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

void ProjectResolver::Private::applyFileTaggers(const ResolvedProductPtr &product) const
{
    for (const SourceArtifactPtr &artifact : product->allEnabledFiles())
        applyFileTaggers(artifact, product);
}

void ProjectResolver::Private::applyFileTaggers(const SourceArtifactPtr &artifact,
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

QVariantMap ProjectResolver::Private::evaluateModuleValues(Item *item, bool lookupPrototype)
{
    AccumulatingTimer modPropEvalTimer(setupParams.logElapsedTime()
                                       ? &elapsedTimeModPropEval : nullptr);
    QVariantMap moduleValues;
    for (const Item::Module &module : item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        const QString fullName = module.name.toString();
        moduleValues[fullName] = evaluateProperties(module.item, lookupPrototype, true);
    }

    return moduleValues;
}

QVariantMap ProjectResolver::Private::evaluateProperties(Item *item, bool lookupPrototype,
                                                         bool checkErrors)
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt, lookupPrototype, checkErrors);
}

QVariantMap ProjectResolver::Private::evaluateProperties(
    const Item *item, const Item *propertiesContainer, const QVariantMap &tmplt,
    bool lookupPrototype, bool checkErrors)
{
    AccumulatingTimer propEvalTimer(setupParams.logElapsedTime()
                                    ? &elapsedTimeAllPropEval : nullptr);
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

void ProjectResolver::Private::evaluateProperty(
    const Item *item, const QString &propName, const ValuePtr &propValue, QVariantMap &result,
    bool checkErrors)
{
    JSContext * const ctx = engine->context();
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
        const ScopedJsValue scriptValue(ctx, evaluator.property(item, propName));
        if (JsException ex = evaluator.engine()->checkAndClearException(propValue->location())) {
            if (checkErrors)
                throw ex.toErrorInfo();
        }

        // NOTE: Loses type information if scriptValue.isUndefined == true,
        //       as such QScriptValues become invalid QVariants.
        QVariant v;
        if (JS_IsFunction(ctx, scriptValue)) {
            v = getJsString(ctx, scriptValue);
        } else {
            v = getJsVariant(ctx, scriptValue);
            QVariantMap m = v.toMap();
            if (m.contains(StringConstants::importScopeNamePropertyInternal())) {
                QVariantMap tmp = m;
                const ScopedJsValue proto(ctx, JS_GetPrototype(ctx, scriptValue));
                m = getJsVariant(ctx, proto).toMap();
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
        checkAllowedValues(v, propValue->location(), pd, propName);
        result[propName] = v;
        break;
    }
    case Value::VariantValueType:
    {
        if (result.contains(propName))
            break;
        VariantValuePtr vvp = std::static_pointer_cast<VariantValue>(propValue);
        QVariant v = vvp->value();

        const PropertyDeclaration pd = item->propertyDeclaration(propName);
        if (v.isNull() && !pd.isScalar()) // QTBUG-51237
            v = QStringList();

        checkAllowedValues(v, propValue->location(), pd, propName);
        result[propName] = v;
        break;
    }
    }
}

void ProjectResolver::Private::checkAllowedValues(
        const QVariant &value, const CodeLocation &loc, const PropertyDeclaration &decl,
        const QString &key) const
{
    const auto type = decl.type();
    if (type != PropertyDeclaration::String && type != PropertyDeclaration::StringList)
        return;

    if (value.isNull())
        return;

    const auto &allowedValues = decl.allowedValues();
    if (allowedValues.isEmpty())
        return;

    const auto checkValue = [this, &loc, &allowedValues, &key](const QString &value)
    {
        if (!allowedValues.contains(value)) {
            const auto message = Tr::tr("Value '%1' is not allowed for property '%2'.")
                    .arg(value, key);
            ErrorInfo error(message, loc);
            handlePropertyError(error, setupParams, logger);
        }
    };

    if (type == PropertyDeclaration::StringList) {
        const auto strings = value.toStringList();
        for (const auto &string: strings) {
            checkValue(string);
        }
    } else if (type == PropertyDeclaration::String) {
        checkValue(value.toString());
    }
}

void ProjectResolver::Private::collectPropertiesForExportItem(const QualifiedId &moduleName,
         const ValuePtr &value, Item *moduleInstance, QVariantMap &moduleProps)
{
    QBS_CHECK(value->type() == Value::ItemValueType);
    Item * const itemValueItem = std::static_pointer_cast<ItemValue>(value)->item();
    if (itemValueItem->propertyDeclarations().isEmpty()) {
        for (const Item::Module &module : moduleInstance->modules()) {
            if (module.name == moduleName) {
                itemValueItem->setPropertyDeclarations(module.item->propertyDeclarations());
                break;
            }
        }
    }
    if (itemValueItem->type() == ItemType::ModuleInstancePlaceholder) {
        struct EvalPreparer {
            EvalPreparer(Item *valueItem, const QualifiedId &moduleName)
                : valueItem(valueItem),
                  hadName(!!valueItem->variantProperty(StringConstants::nameProperty()))
            {
                if (!hadName) {
                    // Evaluator expects a name here.
                    valueItem->setProperty(StringConstants::nameProperty(),
                                           VariantValue::create(moduleName.toString()));
                }
            }
            ~EvalPreparer()
            {
                if (!hadName)
                    valueItem->setProperty(StringConstants::nameProperty(), VariantValuePtr());
            }
            Item * const valueItem;
            const bool hadName;
        };
        EvalPreparer ep(itemValueItem, moduleName);
        std::vector<TempScopeSetter> tss;
        for (const ValuePtr &v : itemValueItem->properties())
            tss.emplace_back(v, moduleInstance);
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

void ProjectResolver::Private::createProductConfig(ResolvedProduct *product)
{
    EvalCacheEnabler cachingEnabler(&evaluator, productContext->product->sourceDirectory);
    product->moduleProperties->setValue(evaluateModuleValues(productContext->item));
    product->productProperties = evaluateProperties(productContext->item, productContext->item,
                                                    QVariantMap(), true, true);
}

void ProjectResolver::Private::callItemFunction(const ItemFuncMap &mappings, Item *item,
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

ProjectContext ProjectResolver::Private::createProjectContext(
    ProjectContext *parentProjectContext) const
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
