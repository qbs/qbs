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

#include "dependenciesresolver.h"
#include "itemreader.h"
#include "loaderutils.h"
#include "localprofiles.h"
#include "moduleinstantiator.h"
#include "modulepropertymerger.h"
#include "probesresolver.h"
#include "productscollector.h"
#include "productshandler.h"

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
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/joblimits.h>
#include <tools/jsliterals.h>
#include <tools/profile.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/set.h>
#include <tools/settings.h>
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

static SetupProjectParameters finalizedProjectParameters(const SetupProjectParameters &in,
                                                         Logger &logger)
{
    SetupProjectParameters params = in;
    if (params.topLevelProfile().isEmpty()) {
        Settings settings(params.settingsDirectory());
        QString profileName = settings.defaultProfile();
        if (profileName.isEmpty()) {
            logger.qbsDebug() << Tr::tr("No profile specified and no default profile exists. "
                                        "Using default property values.");
            profileName = Profile::fallbackName();
        }
        params.setTopLevelProfile(profileName);
        params.expandBuildConfiguration();
    }
    params.finalizeProjectFilePath();

    QBS_CHECK(QFileInfo(params.projectFilePath()).isAbsolute());
    QBS_CHECK(FileInfo::isAbsolute(params.buildRoot()));

    return params;
}

class CancelException { };

class ProjectResolver::Private
{
public:
    Private(const SetupProjectParameters &parameters, ScriptEngine *engine, Logger &&logger)
        : setupParams(finalizedProjectParameters(parameters, logger)), engine(engine),
          logger(std::move(logger)) {}

    static void applyFileTaggers(const SourceArtifactPtr &artifact,
                                 const ResolvedProductConstPtr &product);
    void checkCancelation() const;
    TopLevelProjectPtr resolveTopLevelProject();
    void resolveProject(ProjectContext *projectContext);
    void resolveProjectFully(ProjectContext *projectContext);
    void resolveSubProject(Item *item, ProjectContext *projectContext);
    void resolveProductDependencies();
    void postProcess(const ResolvedProductPtr &product, ProjectContext *projectContext) const;
    void applyFileTaggers(const ResolvedProductPtr &product) const;
    void collectExportedProductDependencies();
    void checkOverriddenValues();
    void collectNameFromOverride(const QString &overrideString);
    void loadTopLevelProjectItem();
    void buildProjectTree();

    static void matchArtifactProperties(const ResolvedProductPtr &product,
                                        const std::vector<SourceArtifactPtr> &artifacts);
    void printProfilingInfo();

    const SetupProjectParameters setupParams;
    ScriptEngine * const engine;
    mutable Logger logger;
    Evaluator evaluator{engine};
    ItemPool itemPool;
    LoaderState state{setupParams, itemPool, evaluator, logger};
    ProductsCollector productsCollector{state};
    ProductsHandler productsHandler{state};
    Item *rootProjectItem = nullptr;
    ProgressObserver *progressObserver = nullptr;
    std::vector<ErrorInfo> queuedErrors;
    FileTime lastResolveTime;
    qint64 elapsedTimePropertyChecking = 0;
};

ProjectResolver::ProjectResolver(const SetupProjectParameters &parameters, ScriptEngine *engine,
                                 Logger logger)
    : d(makePimpl<Private>(parameters, engine, std::move(logger)))
{
    d->logger.storeWarnings();
}

ProjectResolver::~ProjectResolver() = default;

void ProjectResolver::setProgressObserver(ProgressObserver *observer)
{
    d->progressObserver = observer;
    d->state.topLevelProject().progressObserver = observer;
}

void ProjectResolver::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    d->state.probesResolver().setOldProjectProbes(oldProbes);
}

void ProjectResolver::setOldProductProbes(
    const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    d->state.probesResolver().setOldProductProbes(oldProbes);
}

void ProjectResolver::setLastResolveTime(const FileTime &time)
{
    d->lastResolveTime = time;
}

void ProjectResolver::setStoredProfiles(const QVariantMap &profiles)
{
    d->state.topLevelProject().profileConfigs = profiles;
}

void ProjectResolver::setStoredModuleProviderInfo(const StoredModuleProviderInfo &providerInfo)
{
    d->state.dependenciesResolver().setStoredModuleProviderInfo(providerInfo);
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
    qCDebug(lcProjectResolver) << "resolving" << d->setupParams.projectFilePath();

    d->engine->setEnvironment(d->setupParams.adjustedEnvironment());
    d->engine->checkAndClearException({});
    d->engine->clearImportsCache();
    d->engine->clearRequestedProperties();
    d->engine->enableProfiling(d->setupParams.logElapsedTime());
    d->logger.clearWarnings();
    EvalContextSwitcher evalContextSwitcher(d->engine, EvalContext::PropertyEvaluation);

    // At this point, we cannot set a sensible total effort, because we know nothing about
    // the project yet. That's why we use a placeholder here, so the user at least
    // sees that an operation is starting. The real total effort will be set later when
    // we have enough information.
    if (d->progressObserver) {
        d->progressObserver->initialize(Tr::tr("Resolving project for configuration %1")
            .arg(TopLevelProject::deriveId(d->setupParams.finalBuildConfigurationTree())), 1);
        d->progressObserver->setScriptEngine(d->engine);
    }

    const FileTime resolveTime = FileTime::currentTime();
    TopLevelProjectPtr tlp;
    try {
        d->checkOverriddenValues();
        d->loadTopLevelProjectItem();
        d->buildProjectTree();
        tlp = d->resolveTopLevelProject();
    } catch (const CancelException &) {
        throw ErrorInfo(Tr::tr("Project resolving canceled for configuration '%1'.")
                            .arg(TopLevelProject::deriveId(
                                d->setupParams.finalBuildConfigurationTree())));
    }
    tlp->lastStartResolveTime = resolveTime;
    tlp->lastEndResolveTime = FileTime::currentTime();

    // E.g. if the top-level project is disabled.
    if (d->progressObserver) {
        d->progressObserver->setFinished();
        d->printProfilingInfo();
    }
    return tlp;
}

void ProjectResolver::Private::checkCancelation() const
{
    if (progressObserver && progressObserver->canceled())
        throw CancelException();
}

static void makeSubProjectNamesUniqe(const ResolvedProjectPtr &parentProject)
{
    Set<QString> subProjectNames;
    Set<ResolvedProjectPtr> projectsInNeedOfNameChange;
    for (const ResolvedProjectPtr &p : std::as_const(parentProject->subProjects)) {
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
        progressObserver->setMaximum(int(state.topLevelProject().productsByItem.size()));
    TopLevelProjectPtr project = TopLevelProject::create();
    project->buildDirectory = TopLevelProject::deriveBuildDirectory(
        setupParams.buildRoot(),
        TopLevelProject::deriveId(setupParams.finalBuildConfigurationTree()));
    project->buildSystemFiles = state.itemReader().filesRead()
            - state.dependenciesResolver().tempQbsFiles();
    project->profileConfigs = state.topLevelProject().profileConfigs;
    const QVariantMap &profiles = state.localProfiles().profiles();
    for (auto it = profiles.begin(); it != profiles.end(); ++it)
        project->profileConfigs.remove(it.key());

    project->probes = state.topLevelProject().probes;
    project->moduleProviderInfo = state.dependenciesResolver().storedModuleProviderInfo();
    if (!state.topLevelProject().projects.empty()) {
        ProjectContext * const projectContext = state.topLevelProject().projects.front();
        QBS_CHECK(projectContext->item == rootProjectItem);
        projectContext->project = project;
        resolveProject(projectContext);
    }
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

void ProjectResolver::Private::resolveProject(ProjectContext *projectContext)
{
    checkCancelation();

    if (projectContext->parent) {
        projectContext->project = ResolvedProject::create();
        projectContext->parent->project->subProjects.push_back(projectContext->project);
        projectContext->project->parentProject = projectContext->parent->project;
        projectContext->project->enabled = projectContext->parent->project->enabled;
    }
    projectContext->project->location = projectContext->item->location();

    try {
        resolveProjectFully(projectContext);
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

void ProjectResolver::Private::resolveProjectFully(ProjectContext *projectContext)
{
    Item * const item = projectContext->item;
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

    for (Item * const child : item->children()) {
        checkCancelation();
        try {
            switch (child->type()) {
            case ItemType::SubProject:
                resolveSubProject(child, projectContext);
                break;
            case ItemType::Product:
                productsHandler.resolveProduct(child, projectContext);
                break;
            case ItemType::FileTagger:
                resolveFileTagger(state, child, projectContext, nullptr);
                break;
            case ItemType::JobLimit:
                resolveJobLimit(state, child, projectContext, nullptr, nullptr);
                break;
            case ItemType::Rule:
                resolveRule(state, child, projectContext, nullptr, nullptr);
                break;
            default:
                break;
            }
        } catch (const ErrorInfo &e) {
            queuedErrors.push_back(e);
        }
    }

    for (ProjectContext * const childContext : projectContext->children)
        resolveProject(childContext);

    for (const ResolvedProductPtr &product : projectContext->project->products)
        postProcess(product, projectContext);
}

void ProjectResolver::Private::resolveSubProject(Item *item, ProjectContext *projectContext)
{
    // If we added a Project child item in ProductsCollector, then the sub-project will be
    // resolved normally via resolveProject(). Otherwise, it was not loaded, for instance
    // because its Properties condition was false, and special handling applies.
    if (item->child(ItemType::Project))
        return;

    ResolvedProjectPtr project = ResolvedProject::create();
    project->enabled = false;
    project->parentProject = projectContext->project;
    projectContext->project->subProjects << project;
    if (Item * const propertiesItem = item->child(ItemType::PropertiesInSubProject))
        project->name = evaluator.stringValue(propertiesItem, StringConstants::nameProperty());
}

void ProjectResolver::Private::collectExportedProductDependencies()
{
    ResolvedProductPtr dummyProduct = ResolvedProduct::create();
    dummyProduct->enabled = false;
    for (auto it = state.topLevelProject().productsByItem.cbegin();
         it != state.topLevelProject().productsByItem.cend(); ++it) {
        ProductContext &productContext = *it->second;
        if (!productContext.shadowProduct)
            continue;
        const ResolvedProductPtr exportingProduct = productContext.product;
        if (!exportingProduct || !exportingProduct->enabled)
            continue;
        Item * const importingProductItem = productContext.shadowProduct->item;

        std::vector<std::pair<ResolvedProductPtr, QVariantMap>> directDeps;
        for (const Item::Module &m : importingProductItem->modules()) {
            if (m.name.toString() != exportingProduct->name)
                continue;
            for (const Item::Module &dep : m.item->modules()) {
                if (dep.productInfo) {
                    directDeps.emplace_back(state.topLevelProject()
                                            .productsByItem[dep.productInfo->item]->product,
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

void ProjectResolver::Private::checkOverriddenValues()
{
    static const auto matchesPrefix = [](const QString &key) {
        static const QStringList prefixes({StringConstants::projectPrefix(),
                                           QStringLiteral("projects"),
                                           QStringLiteral("products"), QStringLiteral("modules"),
                                           StringConstants::moduleProviders(),
                                           StringConstants::qbsModule()});
        return any_of(prefixes, [&key](const QString &prefix) {
            return key.startsWith(prefix + QLatin1Char('.')); });
    };
    const QVariantMap &overriddenValues = state.parameters().overriddenValues();
    for (auto it = overriddenValues.begin(); it != overriddenValues.end(); ++it) {
        if (matchesPrefix(it.key())) {
            collectNameFromOverride(it.key());
            continue;
        }

        ErrorInfo e(Tr::tr("Property override key '%1' not understood.").arg(it.key()));
        e.append(Tr::tr("Please use one of the following:"));
        e.append(QLatin1Char('\t') + Tr::tr("projects.<project-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("modules.<module-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<module-name>."
                                            "<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("moduleProviders.<provider-name>."
                                            "<property-name>:value"));
        handlePropertyError(e, state.parameters(), state.logger());
    }
}

void ProjectResolver::Private::collectNameFromOverride(const QString &overrideString)
{
    const auto extract = [&overrideString](const QString &prefix) {
        if (!overrideString.startsWith(prefix))
            return QString();
        const int startPos = prefix.length();
        const int endPos = overrideString.lastIndexOf(StringConstants::dot());
        if (endPos == -1)
            return QString();
        return overrideString.mid(startPos, endPos - startPos);
    };
    const QString &projectName = extract(StringConstants::projectsOverridePrefix());
    if (!projectName.isEmpty()) {
        state.topLevelProject().projectNamesUsedInOverrides.insert(projectName);
        return;
    }
    const QString &productName = extract(StringConstants::productsOverridePrefix());
    if (!productName.isEmpty()) {
        state.topLevelProject().productNamesUsedInOverrides.insert(productName.left(
            productName.indexOf(StringConstants::dot())));
        return;
    }
}

void ProjectResolver::Private::loadTopLevelProjectItem()
{
    const QStringList topLevelSearchPaths
            = state.parameters().finalBuildConfigurationTree()
              .value(StringConstants::projectPrefix()).toMap()
              .value(StringConstants::qbsSearchPathsProperty()).toStringList();
    SearchPathsManager searchPathsManager(state.itemReader(), topLevelSearchPaths);
    Item * const root = state.itemReader().setupItemFromFile(
                state.parameters().projectFilePath(), {});
    if (!root)
        return;

    switch (root->type()) {
    case ItemType::Product:
        rootProjectItem = state.itemReader().wrapInProjectIfNecessary(root);
        break;
    case ItemType::Project:
        rootProjectItem = root;
        break;
    default:
        throw ErrorInfo(Tr::tr("The top-level item must be of type 'Project' or 'Product', but it"
                               " is of type '%1'.").arg(root->typeName()), root->location());
    }
}

void ProjectResolver::Private::buildProjectTree()
{
    state.topLevelProject().buildDirectory = TopLevelProject::deriveBuildDirectory(
                state.parameters().buildRoot(),
                TopLevelProject::deriveId(state.parameters().finalBuildConfigurationTree()));
    rootProjectItem->setProperty(StringConstants::sourceDirectoryProperty(),
                                 VariantValue::create(QFileInfo(rootProjectItem->file()->filePath())
                                                      .absolutePath()));
    rootProjectItem->setProperty(StringConstants::buildDirectoryProperty(),
                                 VariantValue::create(state.topLevelProject().buildDirectory));
    rootProjectItem->setProperty(StringConstants::profileProperty(),
                                 VariantValue::create(state.parameters().topLevelProfile()));
    productsCollector.run(rootProjectItem);
    productsHandler.run();

    state.itemReader().clearExtraSearchPathsStack(); // TODO: Unneeded?
    AccumulatingTimer timer(state.parameters().logElapsedTime()
                            ? &elapsedTimePropertyChecking : nullptr);
    checkPropertyDeclarations(rootProjectItem, state.topLevelProject().disabledItems,
                              state.parameters(), state.logger());
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
    logger.qbsLog(LoggerInfo, true)
            << "  "
            << Tr::tr("Project file loading and parsing took %1.")
               .arg(elapsedTimeString(state.itemReader().elapsedTime()));
    productsCollector.printProfilingInfo(2);
    productsHandler.printProfilingInfo(2);
    state.dependenciesResolver().printProfilingInfo(4);
    state.moduleInstantiator().printProfilingInfo(6);
    state.propertyMerger().printProfilingInfo(6);
    state.probesResolver().printProfilingInfo(4);
    state.logger().qbsLog(LoggerInfo, true)
            << "  "
            << Tr::tr("Property checking took %1.")
               .arg(elapsedTimeString(elapsedTimePropertyChecking));
}

void ProjectResolver::Private::resolveProductDependencies()
{
    for (auto it = state.topLevelProject().productsByItem.cbegin();
         it != state.topLevelProject().productsByItem.cend(); ++it) {
        const ResolvedProductPtr &product = it->second->product;
        if (!product)
            continue;
        for (const Item::Module &module : it->first->modules()) {
            if (!module.productInfo)
                continue;
            const ResolvedProductPtr &dep = state.topLevelProject()
                    .productsByItem[module.productInfo->item]->product;
            QBS_CHECK(dep);
            QBS_CHECK(dep != product);
            product->dependencies << dep;
            product->dependencyParameters.insert(dep, module.parameters); // TODO: Streamline this with normal module dependencies?
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

} // namespace Internal
} // namespace qbs
