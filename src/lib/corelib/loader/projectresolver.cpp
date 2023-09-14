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

#include "itemreader.h"
#include "loaderutils.h"
#include "productscollector.h"
#include "productsresolver.h"

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

class ProjectResolver::Private
{
public:
    Private(const SetupProjectParameters &parameters, ScriptEngine *engine, Logger &&logger)
        : setupParams(finalizedProjectParameters(parameters, logger)), engine(engine),
          logger(std::move(logger)) {}

    TopLevelProjectPtr resolveTopLevelProject();
    void resolveProject(ProjectContext *projectContext);
    void resolveProjectFully(ProjectContext *projectContext);
    void resolveSubProject(Item *item, ProjectContext *projectContext);
    void checkOverriddenValues();
    void collectNameFromOverride(const QString &overrideString);
    void loadTopLevelProjectItem();
    void buildProjectTree();

    void printProfilingInfo();

    const SetupProjectParameters setupParams;
    ScriptEngine * const engine;
    mutable Logger logger;
    ItemPool itemPool;
    TopLevelProjectContext topLevelProject;
    LoaderState state{setupParams, topLevelProject, itemPool, *engine, logger};
    Item *rootProjectItem = nullptr;
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
    d->state.topLevelProject().setProgressObserver(observer);
}

void ProjectResolver::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    d->state.topLevelProject().setOldProjectProbes(oldProbes);
}

void ProjectResolver::setOldProductProbes(
    const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    d->state.topLevelProject().setOldProductProbes(oldProbes);
}

void ProjectResolver::setLastResolveTime(const FileTime &time)
{
    d->state.topLevelProject().setLastResolveTime(time);
}

void ProjectResolver::setStoredProfiles(const QVariantMap &profiles)
{
    d->state.topLevelProject().setProfileConfigs(profiles);
}

void ProjectResolver::setStoredModuleProviderInfo(const StoredModuleProviderInfo &providerInfo)
{
    d->state.topLevelProject().setModuleProvidersCache(providerInfo.providers);
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
    if (ProgressObserver * const observer = d->state.topLevelProject().progressObserver()) {
        observer->initialize(Tr::tr("Resolving project for configuration %1")
            .arg(TopLevelProject::deriveId(d->setupParams.finalBuildConfigurationTree())), 1);
        observer->addScriptEngine(d->engine);
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
    if (ProgressObserver * const observer = d->state.topLevelProject().progressObserver()) {
        observer->setFinished();
        d->printProfilingInfo();
    }
    return tlp;
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
    if (ProgressObserver * const observer = state.topLevelProject().progressObserver())
        observer->setMaximum(state.topLevelProject().productCount());
    TopLevelProjectPtr project = TopLevelProject::create();
    project->buildDirectory = TopLevelProject::deriveBuildDirectory(
        setupParams.buildRoot(),
        TopLevelProject::deriveId(setupParams.finalBuildConfigurationTree()));
    if (!state.topLevelProject().projects().empty()) {
        ProjectContext * const projectContext = state.topLevelProject().projects().front();
        QBS_CHECK(projectContext->item == rootProjectItem);
        projectContext->project = project;
        resolveProject(projectContext);
    }
    resolveProducts(state);
    ErrorInfo accumulatedErrors;
    for (const ErrorInfo &e : state.topLevelProject().queuedErrors())
        appendError(accumulatedErrors, e);
    if (accumulatedErrors.hasError())
        throw accumulatedErrors;

    project->buildSystemFiles.unite(state.topLevelProject().buildSystemFiles());
    project->profileConfigs = state.topLevelProject().profileConfigs();
    const QVariantMap &profiles = state.topLevelProject().localProfiles();
    for (auto it = profiles.begin(); it != profiles.end(); ++it)
        project->profileConfigs.remove(it.key());
    project->probes = state.topLevelProject().projectLevelProbes();
    project->moduleProviderInfo.providers = state.topLevelProject().moduleProvidersCache();
    project->setBuildConfiguration(setupParams.finalBuildConfigurationTree());
    project->overriddenValues = setupParams.overriddenValues();
    state.topLevelProject().collectDataFromEngine(*engine);
    makeSubProjectNamesUniqe(project);
    checkForDuplicateProductNames(project);
    project->warningsEncountered << logger.warnings();

    return project;
}

void ProjectResolver::Private::resolveProject(ProjectContext *projectContext)
{
    state.topLevelProject().checkCancelation();

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
            && state.evaluator().boolValue(item, StringConstants::conditionProperty());
    projectContext->project->name = state.evaluator().stringValue(
                item, StringConstants::nameProperty());
    if (projectContext->project->name.isEmpty())
        projectContext->project->name = FileInfo::baseName(item->location().filePath()); // FIXME: Must also be changed in item?
    QVariantMap projectProperties;
    if (!projectContext->project->enabled) {
        projectProperties.insert(StringConstants::profileProperty(),
                                 state.evaluator().stringValue(
                                     item, StringConstants::profileProperty()));
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
        const ScopedJsValue sv(engine->context(), state.evaluator().value(item, it.key()));
        projectProperties.insert(it.key(), getJsVariant(engine->context(), sv));
    }
    projectContext->project->setProjectProperties(projectProperties);

    for (Item * const child : item->children()) {
        state.topLevelProject().checkCancelation();
        try {
            switch (child->type()) {
            case ItemType::SubProject:
                resolveSubProject(child, projectContext);
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
            state.topLevelProject().addQueuedError(e);
        }
    }

    for (ProjectContext * const childContext : projectContext->children)
        resolveProject(childContext);
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
    if (Item * const propertiesItem = item->child(ItemType::PropertiesInSubProject)) {
        project->name = state.evaluator().stringValue(propertiesItem,
                                                      StringConstants::nameProperty());
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
        state.topLevelProject().addProjectNameUsedInOverrides(projectName);
        return;
    }
    const QString &productName = extract(StringConstants::productsOverridePrefix());
    if (!productName.isEmpty()) {
        state.topLevelProject().addProductNameUsedInOverrides(productName.left(
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
    state.topLevelProject().setBuildDirectory(TopLevelProject::deriveBuildDirectory(
                state.parameters().buildRoot(),
                TopLevelProject::deriveId(state.parameters().finalBuildConfigurationTree())));
    rootProjectItem->setProperty(StringConstants::sourceDirectoryProperty(),
                                 VariantValue::create(QFileInfo(rootProjectItem->file()->filePath())
                                                      .absolutePath()));
    rootProjectItem->setProperty(StringConstants::buildDirectoryProperty(),
                                 VariantValue::create(state.topLevelProject().buildDirectory()));
    rootProjectItem->setProperty(StringConstants::profileProperty(),
                                 VariantValue::create(state.parameters().topLevelProfile()));
    ProductsCollector(state).run(rootProjectItem);

    AccumulatingTimer timer(state.parameters().logElapsedTime()
                            ? &state.topLevelProject().timingData().propertyChecking : nullptr);
    checkPropertyDeclarations(rootProjectItem, state);
}

void ProjectResolver::Private::printProfilingInfo()
{
    if (!setupParams.logElapsedTime())
        return;
    const auto print = [this](int indent, const QString &pattern, qint64 time) {
        logger.qbsLog(LoggerInfo, true) << QByteArray(indent, ' ')
                                        << pattern.arg(elapsedTimeString(time));
    };
    print(2, Tr::tr("Project file loading and parsing took %1."), state.itemReader().elapsedTime());
    print(2, Tr::tr("Preparing products took %1."),
          state.topLevelProject().timingData().preparingProducts);
    print(2, Tr::tr("Setting up Groups took %1."),
          state.topLevelProject().timingData().groupsSetup);
    print(2, Tr::tr("Scheduling products took %1."),
          state.topLevelProject().timingData().schedulingProducts);
    print(2, Tr::tr("Resolving products took %1."),
          state.topLevelProject().timingData().resolvingProducts);
    print(4, Tr::tr("Property evaluation took %1."),
          state.topLevelProject().timingData().propertyEvaluation);
    print(4, Tr::tr("Resolving groups (without module property evaluation) took %1."),
          state.topLevelProject().timingData().groupsResolving);
    print(4, Tr::tr("Setting up product dependencies took %1."),
          state.topLevelProject().timingData().dependenciesResolving);
    print(6, Tr::tr("Running module providers took %1."),
          state.topLevelProject().timingData().moduleProviders);
    print(6, Tr::tr("Instantiating modules took %1."),
          state.topLevelProject().timingData().moduleInstantiation);
    print(6, Tr::tr("Merging module property values took %1."),
          state.topLevelProject().timingData().propertyMerging);
    logger.qbsLog(LoggerInfo, true) << QByteArray(4, ' ') << "There were "
                                    << state.topLevelProject().productDeferrals()
                                    << " product deferrals with a total of "
                                    << state.topLevelProject().productCount() << " products.";
    print(2, Tr::tr("Running Probes took %1."), state.topLevelProject().timingData().probes);
    state.logger().qbsLog(LoggerInfo, true)
        << "    "
        << Tr::tr("%1 probes encountered, %2 configure scripts executed, "
                  "%3 re-used from current run, %4 re-used from earlier run.")
           .arg(state.topLevelProject().probesEncounteredCount())
           .arg(state.topLevelProject().probesRunCount())
           .arg(state.topLevelProject().reusedCurrentProbesCount())
           .arg(state.topLevelProject().reusedOldProbesCount());
    print(2, Tr::tr("Property checking took %1."),
          state.topLevelProject().timingData().propertyChecking);
}

} // namespace Internal
} // namespace qbs
