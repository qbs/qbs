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
#include "buildgraphloader.h"

#include "buildgraph.h"
#include "cycledetector.h"
#include "emptydirectoriesremover.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rulecommands.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <language/artifactproperties.h>
#include <language/language.h>
#include <language/loader.h>
#include <language/propertymapinternal.h>
#include <language/qualifiedid.h>
#include <language/resolvedfilecontext.h>
#include <logging/translator.h>
#include <tools/buildgraphlocker.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/profiling.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

#include <algorithm>

namespace qbs {
namespace Internal {

BuildGraphLoader::BuildGraphLoader(const Logger &logger) :
    m_logger(logger)
{
}

BuildGraphLoader::~BuildGraphLoader()
{
    qDeleteAll(m_objectsToDelete);
}

static void restoreBackPointers(const ResolvedProjectPtr &project)
{
    for (const ResolvedProductPtr &product : qAsConst(project->products)) {
        product->project = project;
        if (!product->buildData)
            continue;
        for (BuildGraphNode * const n : qAsConst(product->buildData->nodes)) {
            if (Artifact *a = dynamic_cast<Artifact *>(n))
                project->topLevelProject()->buildData->insertIntoLookupTable(a);
        }
    }

    for (const ResolvedProjectPtr &subProject : qAsConst(project->subProjects)) {
        subProject->parentProject = project;
        restoreBackPointers(subProject);
    }
}

BuildGraphLoadResult BuildGraphLoader::load(const TopLevelProjectPtr &existingProject,
                                            const SetupProjectParameters &parameters,
                                            const RulesEvaluationContextPtr &evalContext)
{
    m_parameters = parameters;
    m_result = BuildGraphLoadResult();
    m_evalContext = evalContext;

    if (existingProject) {
        QBS_CHECK(existingProject->buildData);
        existingProject->buildData->evaluationContext = evalContext;
        if (!checkBuildGraphCompatibility(existingProject))
            return m_result;
        m_result.loadedProject = existingProject;
    } else {
        loadBuildGraphFromDisk();
    }
    if (!m_result.loadedProject)
        return m_result;
    if (parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly) {
        for (const ErrorInfo &e : qAsConst(m_result.loadedProject->warningsEncountered))
            m_logger.printWarning(e);
        return m_result;
    }
    QBS_CHECK(parameters.restoreBehavior() == SetupProjectParameters::RestoreAndTrackChanges);

    if (m_parameters.logElapsedTime()) {
        m_wildcardExpansionEffort = 0;
        m_propertyComparisonEffort = 0;
    }
    trackProjectChanges();
    if (m_parameters.logElapsedTime()) {
        m_logger.qbsLog(LoggerInfo, true) << "\t"
                << Tr::tr("Wildcard expansion took %1.")
                   .arg(elapsedTimeString(m_wildcardExpansionEffort));
        m_logger.qbsLog(LoggerInfo, true) << "\t"
                << Tr::tr("Comparing property values took %1.")
                   .arg(elapsedTimeString(m_propertyComparisonEffort));
    }
    return m_result;
}

TopLevelProjectConstPtr BuildGraphLoader::loadProject(const QString &bgFilePath)
{
    class LogSink : public ILogSink {
        void doPrintMessage(LoggerLevel, const QString &, const QString &) override { }
    } dummySink;
    Logger dummyLogger(&dummySink);
    BuildGraphLocker bgLocker(bgFilePath, dummyLogger, false, nullptr);
    PersistentPool pool(dummyLogger);
    pool.load(bgFilePath);
    const TopLevelProjectPtr project = TopLevelProject::create();
    project->load(pool);
    project->setBuildConfiguration(pool.headData().projectConfig);
    return project;
}

void BuildGraphLoader::loadBuildGraphFromDisk()
{
    const QString projectId = TopLevelProject::deriveId(m_parameters.finalBuildConfigurationTree());
    const QString buildDir
            = TopLevelProject::deriveBuildDirectory(m_parameters.buildRoot(), projectId);
    const QString buildGraphFilePath
            = ProjectBuildData::deriveBuildGraphFilePath(buildDir, projectId);

    PersistentPool pool(m_logger);
    m_logger.qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    try {
        pool.load(buildGraphFilePath);
    } catch (const NoBuildGraphError &e) {
        if (m_parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly)
            throw;
        m_logger.qbsInfo() << e.toString();
        return;
    } catch (const ErrorInfo &loadError) {
        if (!m_parameters.overrideBuildGraphData()) {
            ErrorInfo fullError = loadError;
            fullError.append(Tr::tr("Use the 'resolve' command to set up a new build graph."));
            throw fullError;
        }
        m_logger.qbsWarning() << loadError.toString();
        return;
    }

    const TopLevelProjectPtr project = TopLevelProject::create();

    // TODO: Store some meta data that will enable us to show actual progress (e.g. number of products).
    m_evalContext->initializeObserver(Tr::tr("Restoring build graph from disk"), 1);

    project->load(pool);
    project->buildData->evaluationContext = m_evalContext;
    project->setBuildConfiguration(pool.headData().projectConfig);
    project->buildDirectory = buildDir;
    if (!checkBuildGraphCompatibility(project))
        return;
    restoreBackPointers(project);
    project->location = CodeLocation(m_parameters.projectFilePath(), project->location.line(),
                                     project->location.column());
    m_result.loadedProject = project;
    m_evalContext->incrementProgressValue();
    doSanityChecks(project, m_logger);
}

bool BuildGraphLoader::checkBuildGraphCompatibility(const TopLevelProjectConstPtr &project)
{
    if (m_parameters.projectFilePath().isEmpty())
        m_parameters.setProjectFilePath(project->location.filePath());
    else
        Loader::setupProjectFilePath(m_parameters);
    if (QFileInfo(project->location.filePath()) == QFileInfo(m_parameters.projectFilePath()))
        return true;
    QString message = Tr::tr("Stored build graph at '%1' is for project file '%2', but "
                             "input file is '%3'.")
            .arg(QDir::toNativeSeparators(project->buildGraphFilePath()),
                 QDir::toNativeSeparators(project->location.filePath()),
                 QDir::toNativeSeparators(m_parameters.projectFilePath()));
    if (m_parameters.overrideBuildGraphData()) {
        m_logger.qbsInfo() << message;
        return false;
    }
    message.append(QLatin1Char('\n')).append(Tr::tr("Use the 'resolve' command to enforce "
                                                    "using a different project file."));
    throw ErrorInfo(message);
}

static bool checkProductForChangedDependency(QList<ResolvedProductPtr> &changedProducts,
        Set<ResolvedProductPtr> &seenProducts, const ResolvedProductPtr &product)
{
    if (seenProducts.contains(product))
        return false;
    if (changedProducts.contains(product))
        return true;
    for (const ResolvedProductPtr &dep : qAsConst(product->dependencies)) {
        if (checkProductForChangedDependency(changedProducts, seenProducts, dep)) {
            changedProducts << product;
            return true;
        }
    }
    seenProducts << product;
    return false;
}

// All products depending on changed products also become changed. Otherwise the output
// artifacts of the rules taking the artifacts from the dependency as inputs will be
// rebuilt due to their rule getting re-applied (as the rescued input artifacts will show
// up as newly added) and no rescue data being available.
static void makeChangedProductsListComplete(QList<ResolvedProductPtr> &changedProducts,
                                            const QList<ResolvedProductPtr> &allRestoredProducts)
{
    Set<ResolvedProductPtr> seenProducts;
    for (const ResolvedProductPtr &p : allRestoredProducts)
        checkProductForChangedDependency(changedProducts, seenProducts, p);
}

void BuildGraphLoader::trackProjectChanges()
{
    TimedActivityLogger trackingTimer(m_logger, Tr::tr("Change tracking"),
                                      m_parameters.logElapsedTime());
    const TopLevelProjectPtr &restoredProject = m_result.loadedProject;
    Set<QString> buildSystemFiles = restoredProject->buildSystemFiles;
    QList<ResolvedProductPtr> allRestoredProducts = restoredProject->allProducts();
    QList<ResolvedProductPtr> changedProducts;
    bool reResolvingNecessary = false;
    if (!checkConfigCompatibility())
        reResolvingNecessary = true;
    if (hasProductFileChanged(allRestoredProducts, restoredProject->lastResolveTime,
                              buildSystemFiles, changedProducts)) {
        reResolvingNecessary = true;
    }

    // "External" changes, e.g. in the environment or in a JavaScript file,
    // can make the list of source files in a product change without the respective file
    // having been touched. In such a case, the build data for that product will have to be set up
    // anew.
    if (probeExecutionForced(restoredProject, allRestoredProducts)
            || hasBuildSystemFileChanged(buildSystemFiles, restoredProject->lastResolveTime)
            || hasEnvironmentChanged(restoredProject)
            || hasCanonicalFilePathResultChanged(restoredProject)
            || hasFileExistsResultChanged(restoredProject)
            || hasDirectoryEntriesResultChanged(restoredProject)
            || hasFileLastModifiedResultChanged(restoredProject)) {
        reResolvingNecessary = true;
    }

    if (!reResolvingNecessary) {
        for (const ErrorInfo &e : qAsConst(restoredProject->warningsEncountered))
            m_logger.printWarning(e);
        return;
    }

    restoredProject->buildData->isDirty = true;
    if (!m_parameters.overrideBuildGraphData())
        m_parameters.setEnvironment(restoredProject->environment);
    Loader ldr(m_evalContext->engine(), m_logger);
    ldr.setSearchPaths(m_parameters.searchPaths());
    ldr.setProgressObserver(m_evalContext->observer());
    ldr.setOldProjectProbes(restoredProject->probes);
    QHash<QString, QList<ProbeConstPtr>> restoredProbes;
    for (const auto &restoredProduct : qAsConst(allRestoredProducts))
        restoredProbes.insert(restoredProduct->uniqueName(), restoredProduct->probes);
    ldr.setOldProductProbes(restoredProbes);
    if (!m_parameters.overrideBuildGraphData())
        ldr.setStoredProfiles(restoredProject->profileConfigs);
    m_result.newlyResolvedProject = ldr.loadProject(m_parameters);

    QMap<QString, ResolvedProductPtr> freshProductsByName;
    QList<ResolvedProductPtr> allNewlyResolvedProducts
            = m_result.newlyResolvedProject->allProducts();
    for (const ResolvedProductPtr &cp : qAsConst(allNewlyResolvedProducts))
        freshProductsByName.insert(cp->uniqueName(), cp);

    m_envChange = restoredProject->environment != m_result.newlyResolvedProject->environment;
    checkAllProductsForChanges(allRestoredProducts, freshProductsByName, changedProducts);

    std::shared_ptr<ProjectBuildData> oldBuildData;
    ChildListHash childLists;
    if (!changedProducts.isEmpty()) {
        oldBuildData = std::make_shared<ProjectBuildData>(restoredProject->buildData.data());
        for (const ResolvedProductConstPtr &product : qAsConst(allRestoredProducts)) {
            if (!product->buildData)
                continue;

            // If the product gets temporarily removed, its artifacts will get disconnected
            // and this structural information will no longer be directly available from them.
            for (const Artifact *a : filterByType<Artifact>(product->buildData->nodes)) {
                childLists.insert(a, ChildrenInfo(ArtifactSet::filtered(a->children),
                                                  a->childrenAddedByScanner));
            }
        }
    }

    makeChangedProductsListComplete(changedProducts, allRestoredProducts);

    // Set up build data from scratch for all changed products. This does not necessarily
    // mean that artifacts will have to get rebuilt; whether this is necesessary will be decided
    // an a per-artifact basis by the Executor on the next build.
    QHash<QString, AllRescuableArtifactData> rescuableArtifactData;
    for (const ResolvedProductPtr &product : qAsConst(changedProducts)) {
        ResolvedProductPtr freshProduct = freshProductsByName.value(product->uniqueName());
        if (!freshProduct)
            continue;
        onProductRemoved(product, product->topLevelProject()->buildData.data(), false);
        if (product->buildData) {
            rescuableArtifactData.insert(product->uniqueName(),
                                         product->buildData->rescuableArtifactData);
        }
        allRestoredProducts.removeOne(product);
    }

    // Move over restored build data to newly resolved project.
    m_result.newlyResolvedProject->buildData.swap(restoredProject->buildData);
    QBS_CHECK(m_result.newlyResolvedProject->buildData);
    m_result.newlyResolvedProject->buildData->isDirty = true;
    for (int i = allNewlyResolvedProducts.count() - 1; i >= 0; --i) {
        const ResolvedProductPtr &newlyResolvedProduct = allNewlyResolvedProducts.at(i);
        for (int j = allRestoredProducts.count() - 1; j >= 0; --j) {
            const ResolvedProductPtr &restoredProduct = allRestoredProducts.at(j);
            if (newlyResolvedProduct->uniqueName() == restoredProduct->uniqueName()) {
                if (newlyResolvedProduct->enabled)
                    newlyResolvedProduct->buildData.swap(restoredProduct->buildData);
                if (newlyResolvedProduct->buildData) {
                    for (BuildGraphNode *node : qAsConst(newlyResolvedProduct->buildData->nodes))
                        node->product = newlyResolvedProduct;
                }

                // Keep in list if build data still needs to be resolved.
                if (!newlyResolvedProduct->enabled || newlyResolvedProduct->buildData)
                    allNewlyResolvedProducts.removeAt(i);

                allRestoredProducts.removeAt(j);
                break;
            }
        }
    }

    // Products still left in the list do not exist anymore.
    for (const ResolvedProductPtr &removedProduct : qAsConst(allRestoredProducts)) {
        changedProducts.removeOne(removedProduct);
        onProductRemoved(removedProduct, m_result.newlyResolvedProject->buildData.data());
    }

    // Products still left in the list need resolving, either because they are new
    // or because they are newly enabled.
    if (!allNewlyResolvedProducts.isEmpty()) {
        BuildDataResolver bpr(m_logger);
        bpr.resolveProductBuildDataForExistingProject(m_result.newlyResolvedProject,
                                                      allNewlyResolvedProducts);
    }

    for (const ResolvedProductConstPtr &changedProduct : qAsConst(changedProducts)) {
        rescueOldBuildData(changedProduct, freshProductsByName.value(changedProduct->uniqueName()),
                           childLists, rescuableArtifactData.value(changedProduct->uniqueName()));
    }

    EmptyDirectoriesRemover(m_result.newlyResolvedProject.get(), m_logger)
            .removeEmptyParentDirectories(m_artifactsRemovedFromDisk);

    for (FileResourceBase * const f : qAsConst(m_objectsToDelete)) {
        Artifact * const a = dynamic_cast<Artifact *>(f);
        if (a)
            a->product.reset(); // To help with the sanity checks.
    }
    doSanityChecks(m_result.newlyResolvedProject, m_logger);
}

bool BuildGraphLoader::probeExecutionForced(
        const TopLevelProjectConstPtr &restoredProject,
        const QList<ResolvedProductPtr> &restoredProducts) const
{
    if (!m_parameters.forceProbeExecution())
        return false;

    if (!restoredProject->probes.isEmpty())
        return true;

    for (const auto &p : qAsConst(restoredProducts)) {
        if (!p->probes.isEmpty())
            return true;
    }

    return false;
}

bool BuildGraphLoader::hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    if (!m_parameters.overrideBuildGraphData())
        return false;
    QProcessEnvironment oldEnv = restoredProject->environment;
    QProcessEnvironment newEnv = m_parameters.adjustedEnvironment();

    // HACK. Valgrind screws up our null-build benchmarker otherwise.
    // TODO: Think about a (module-provided) whitelist.
    oldEnv.remove(QLatin1String("LD_PRELOAD"));
    newEnv.remove(QLatin1String("LD_PRELOAD"));

    if (oldEnv != newEnv) {
        m_logger.qbsDebug() << "Set of environment variables changed. Must re-resolve project.";
        m_logger.qbsTrace() << "old: " << restoredProject->environment.toStringList() << "\nnew:"
                            << m_parameters.adjustedEnvironment().toStringList();
        return true;
    }
    return false;
}

bool BuildGraphLoader::hasCanonicalFilePathResultChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    for (auto it = restoredProject->canonicalFilePathResults.constBegin();
         it != restoredProject->canonicalFilePathResults.constEnd(); ++it) {
        if (QFileInfo(it.key()).canonicalFilePath() != it.value()) {
            m_logger.qbsDebug() << "Canonical file path for file '" << it.key()
                                << "' changed, must re-resolve project.";
            return true;
        }
    }

    return false;
}

bool BuildGraphLoader::hasFileExistsResultChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    for (QHash<QString, bool>::ConstIterator it = restoredProject->fileExistsResults.constBegin();
         it != restoredProject->fileExistsResults.constEnd(); ++it) {
        if (FileInfo(it.key()).exists() != it.value()) {
            m_logger.qbsDebug() << "Existence check for file '" << it.key()
                                << " 'changed, must re-resolve project.";
            return true;
        }
    }

    return false;
}

bool BuildGraphLoader::hasDirectoryEntriesResultChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    for (auto it = restoredProject->directoryEntriesResults.constBegin();
         it != restoredProject->directoryEntriesResults.constEnd(); ++it) {
        if (QDir(it.key().first).entryList(static_cast<QDir::Filters>(it.key().second), QDir::Name)
                != it.value()) {
            m_logger.qbsDebug() << "Entry list for directory '" << it.key().first
                                << "' ("
                                << static_cast<QDir::Filters>(it.key().second)
                                << ") changed, must re-resolve project.";
            return true;
        }
    }

    return false;
}

bool BuildGraphLoader::hasFileLastModifiedResultChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    for (QHash<QString, FileTime>::ConstIterator it
         = restoredProject->fileLastModifiedResults.constBegin();
         it != restoredProject->fileLastModifiedResults.constEnd(); ++it) {
        if (FileInfo(it.key()).lastModified() != it.value()) {
            m_logger.qbsDebug() << "Timestamp for file '" << it.key()
                                << " 'changed, must re-resolve project.";
            return true;
        }
    }

    return false;
}

bool BuildGraphLoader::hasProductFileChanged(const QList<ResolvedProductPtr> &restoredProducts,
        const FileTime &referenceTime, Set<QString> &remainingBuildSystemFiles,
        QList<ResolvedProductPtr> &changedProducts)
{
    bool hasChanged = false;
    for (const ResolvedProductPtr &product : restoredProducts) {
        const QString filePath = product->location.filePath();
        const FileInfo pfi(filePath);
        remainingBuildSystemFiles.remove(filePath);
        if (!pfi.exists()) {
            m_logger.qbsDebug() << "A product was removed, must re-resolve project";
            hasChanged = true;
        } else if (referenceTime < pfi.lastModified()) {
            m_logger.qbsDebug() << "A product was changed, must re-resolve project";
            hasChanged = true;
        } else if (!changedProducts.contains(product)) {
            bool foundMissingSourceFile = false;
            for (const QString &file : qAsConst(product->missingSourceFiles)) {
                if (FileInfo(file).exists()) {
                    m_logger.qbsDebug() << "Formerly missing file " << file << " in product "
                                        << product->name << " exists now, must re-resolve project";
                    foundMissingSourceFile = true;
                    break;
                }
            }
            if (foundMissingSourceFile) {
                hasChanged = true;
                changedProducts += product;
                continue;
            }

            AccumulatingTimer wildcardTimer(m_parameters.logElapsedTime()
                                            ? &m_wildcardExpansionEffort : nullptr);
            for (const GroupPtr &group : qAsConst(product->groups)) {
                if (!group->wildcards)
                    continue;
                const bool reExpansionRequired = std::any_of(
                            group->wildcards->dirTimeStamps.cbegin(),
                            group->wildcards->dirTimeStamps.cend(),
                            [](const std::pair<QString, FileTime> &pair) {
                                return FileInfo(pair.first).lastModified() > pair.second;
                });
                if (!reExpansionRequired)
                    continue;
                const Set<QString> files = group->wildcards->expandPatterns(group,
                        FileInfo::path(group->location.filePath()),
                        product->topLevelProject()->buildDirectory);
                Set<QString> wcFiles;
                for (const SourceArtifactConstPtr &sourceArtifact
                     : qAsConst(group->wildcards->files)) {
                    wcFiles += sourceArtifact->absoluteFilePath;
                }
                if (files == wcFiles)
                    continue;
                hasChanged = true;
                changedProducts += product;
                break;
            }
        }
    }

    return hasChanged;
}

bool BuildGraphLoader::hasBuildSystemFileChanged(const Set<QString> &buildSystemFiles,
                                                 const FileTime &referenceTime)
{
    for (const QString &file : buildSystemFiles) {
        const FileInfo fi(file);
        if (!fi.exists() || referenceTime < fi.lastModified()) {
            m_logger.qbsDebug() << "A qbs or js file changed, must re-resolve project.";
            return true;
        }
    }
    return false;
}

void BuildGraphLoader::checkAllProductsForChanges(const QList<ResolvedProductPtr> &restoredProducts,
        const QMap<QString, ResolvedProductPtr> &newlyResolvedProductsByName,
        QList<ResolvedProductPtr> &changedProducts)
{
    for (const ResolvedProductPtr &restoredProduct : restoredProducts) {
        const ResolvedProductPtr newlyResolvedProduct
                = newlyResolvedProductsByName.value(restoredProduct->uniqueName());
        if (!newlyResolvedProduct)
            continue;
        if (newlyResolvedProduct->enabled != restoredProduct->enabled) {
            m_logger.qbsDebug() << "Condition of product '" << restoredProduct->uniqueName()
                                << "' was changed, must set up build data from scratch";
            if (!changedProducts.contains(restoredProduct))
                changedProducts << restoredProduct;
            continue;
        }

        if (checkProductForChanges(restoredProduct, newlyResolvedProduct)) {
            m_logger.qbsDebug() << "Product '" << restoredProduct->uniqueName()
                                << "' was changed, must set up build data from scratch";
            if (!changedProducts.contains(restoredProduct))
                changedProducts << restoredProduct;
            continue;
        }

        if (!sourceArtifactSetsAreEqual(restoredProduct->allEnabledFiles(),
                                        newlyResolvedProduct->allEnabledFiles())) {
            m_logger.qbsDebug() << "File list of product '" << restoredProduct->uniqueName()
                                << "' was changed.";
            if (!changedProducts.contains(restoredProduct))
                changedProducts << restoredProduct;
        }
    }
}

static bool dependenciesAreEqual(const ResolvedProductConstPtr &p1,
                                 const ResolvedProductConstPtr &p2)
{
    if (p1->dependencies.count() != p2->dependencies.count())
        return false;
    Set<QString> names1;
    Set<QString> names2;
    for (const ResolvedProductConstPtr &dep : qAsConst(p1->dependencies))
        names1 << dep->uniqueName();
    for (const ResolvedProductConstPtr &dep : qAsConst(p2->dependencies))
        names2 << dep->uniqueName();
    return names1 == names2;
}

bool BuildGraphLoader::checkProductForChanges(const ResolvedProductPtr &restoredProduct,
                                              const ResolvedProductPtr &newlyResolvedProduct)
{
    // This check must come first, as it can prevent build data rescuing as a side effect.
    // TODO: Similar special checks must be done for Environment.getEnv() and File.exists() in
    // commands (or possibly it could be reasonable to just forbid such "dynamic" constructs
    // within commands).
    if (checkForPropertyChanges(restoredProduct, newlyResolvedProduct))
        return true;
    if (!ruleListsAreEqual(restoredProduct->rules.toList(), newlyResolvedProduct->rules.toList()))
        return true;
    if (!dependenciesAreEqual(restoredProduct, newlyResolvedProduct))
        return true;
    const FileTime referenceTime = restoredProduct->topLevelProject()->lastResolveTime;
    for (const RuleConstPtr &rule : qAsConst(newlyResolvedProduct->rules)) {
        if (!isPrepareScriptUpToDate(rule->prepareScript, referenceTime))
            return true;
    }
    return false;
}

bool BuildGraphLoader::checkProductForInstallInfoChanges(const ResolvedProductPtr &restoredProduct,
        const ResolvedProductPtr &newlyResolvedProduct)
{
    // These are not requested from rules at build time, but we still need to take
    // them into account.
    const QStringList specialProperties = QStringList() << QLatin1String("install")
            << QLatin1String("installDir") << QLatin1String("installPrefix")
            << QLatin1String("installRoot");
    for (const QString &key : specialProperties) {
        if (restoredProduct->moduleProperties->qbsPropertyValue(key)
                != newlyResolvedProduct->moduleProperties->qbsPropertyValue(key)) {
            m_logger.qbsDebug() << "Product property 'qbs." << key << "' changed.";
            return true;
        }
    }
    return false;
}

bool BuildGraphLoader::checkForPropertyChanges(const ResolvedProductPtr &restoredProduct,
                                               const ResolvedProductPtr &newlyResolvedProduct)
{
    AccumulatingTimer propertyComparisonTimer(m_parameters.logElapsedTime()
                                              ? &m_propertyComparisonEffort: nullptr);
    m_logger.qbsDebug() << "Checking for changes in properties requested in prepare scripts for "
                           "product '"  << restoredProduct->uniqueName() << "'.";
    if (!restoredProduct->buildData)
        return false;

    // This check must come first, as it can prevent build data rescuing.
    if (checkTransformersForChanges(restoredProduct, newlyResolvedProduct))
        return true;

    if (restoredProduct->fileTags != newlyResolvedProduct->fileTags) {
        m_logger.qbsTrace() << "Product type changed from " << restoredProduct->fileTags
                            << "to " << newlyResolvedProduct->fileTags;
        return true;
    }

    if (checkProductForInstallInfoChanges(restoredProduct, newlyResolvedProduct))
        return true;
    if (!artifactPropertyListsAreEqual(restoredProduct->artifactProperties,
                                       newlyResolvedProduct->artifactProperties)) {
        return true;
    }
    return false;
}

bool BuildGraphLoader::checkTransformersForChanges(const ResolvedProductPtr &restoredProduct,
        const ResolvedProductPtr &newlyResolvedProduct)
{
    bool transformerChanges = false;
    Set<TransformerConstPtr> seenTransformers;
    for (Artifact *artifact : filterByType<Artifact>(restoredProduct->buildData->nodes)) {
        const TransformerPtr transformer = artifact->transformer;
        if (!transformer || !seenTransformers.insert(transformer).second)
            continue;
        if (checkForPropertyChanges(transformer, newlyResolvedProduct)
                || checkForEnvChanges(transformer))
            transformerChanges = true;
    }
    if (transformerChanges) {
        m_logger.qbsDebug() << "Property or environment changes in product '"
                            << newlyResolvedProduct->uniqueName() << "'.";
    }
    return transformerChanges;
}

void BuildGraphLoader::onProductRemoved(const ResolvedProductPtr &product,
        ProjectBuildData *projectBuildData, bool removeArtifactsFromDisk)
{
    m_logger.qbsDebug() << "[BG] product '" << product->uniqueName() << "' removed.";

    product->project->products.removeOne(product);
    if (product->buildData) {
        for (BuildGraphNode * const node : qAsConst(product->buildData->nodes)) {
            if (node->type() == BuildGraphNode::ArtifactNodeType) {
                Artifact * const artifact = static_cast<Artifact *>(node);
                projectBuildData->removeArtifact(artifact, m_logger, removeArtifactsFromDisk,
                                                 false);
                if (removeArtifactsFromDisk && artifact->artifactType == Artifact::Generated)
                    m_artifactsRemovedFromDisk << artifact->filePath();
            } else {
                for (BuildGraphNode * const parent : qAsConst(node->parents))
                    parent->children.remove(node);
                node->parents.clear();
                for (BuildGraphNode * const child : qAsConst(node->children))
                    child->parents.remove(node);
                node->children.clear();
            }
        }
    }
}

static SourceArtifactConstPtr findSourceArtifact(const ResolvedProductConstPtr &product,
        const QString &artifactFilePath, QMap<QString, SourceArtifactConstPtr> &artifactMap)
{
    SourceArtifactConstPtr &artifact = artifactMap[artifactFilePath];
    if (!artifact) {
        for (const SourceArtifactConstPtr &a : product->allFiles()) {
            if (a->absoluteFilePath == artifactFilePath) {
                artifact = a;
                break;
            }
        }
    }
    return artifact;
}

template<typename T> static QVariantMap getParameterValue(
        const QHash<T, QVariantMap> &parameters,
        const QString &depName)
{
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        if (it.key()->name == depName)
            return it.value();
    }
    return QVariantMap();
}

static QVariantMap propertyMapByKind(const ResolvedProductConstPtr &product,
                                     const Property &property)
{
    switch (property.kind) {
    case Property::PropertyInModule:
        return product->moduleProperties->value();
    case Property::PropertyInProduct:
        return product->productProperties;
    case Property::PropertyInProject:
        return product->project->projectProperties();
    case Property::PropertyInParameters: {
        const int sepIndex = property.moduleName.indexOf(QLatin1Char(':'));
        const QString depName = property.moduleName.left(sepIndex);
        QVariantMap v = getParameterValue(product->dependencyParameters, depName);
        if (!v.isEmpty())
            return v;
        return getParameterValue(product->moduleParameters, depName);
    }
    default:
        QBS_CHECK(false);
    }
    return QVariantMap();
}

static void invalidateTransformer(const TransformerPtr &transformer)
{
    const JavaScriptCommandPtr &pseudoCommand = JavaScriptCommand::create();
    pseudoCommand->setSourceCode(QLatin1String("random stuff that will cause "
                                               "commandsEqual() to fail"));
    transformer->commands << pseudoCommand;
}

bool BuildGraphLoader::checkForPropertyChanges(const TransformerPtr &restoredTrafo,
        const ResolvedProductPtr &freshProduct)
{
    // This check must come first, as it can prevent build data rescuing.
    for (const Property &property : qAsConst(restoredTrafo->propertiesRequestedInCommands)) {
        if (checkForPropertyChange(property, propertyMapByKind(freshProduct, property))) {
            invalidateTransformer(restoredTrafo);
            return true;
        }
    }

    QMap<QString, SourceArtifactConstPtr> artifactMap;
    for (auto it = restoredTrafo->propertiesRequestedFromArtifactInCommands.cbegin();
         it != restoredTrafo->propertiesRequestedFromArtifactInCommands.cend(); ++it) {
        const SourceArtifactConstPtr artifact
                = findSourceArtifact(freshProduct, it.key(), artifactMap);
        if (!artifact)
            continue;
        for (const Property &property : qAsConst(it.value())) {
            if (checkForPropertyChange(property, artifact->properties->value())) {
                invalidateTransformer(restoredTrafo);
                return true;
            }
        }
    }

    for (const Property &property : qAsConst(restoredTrafo->propertiesRequestedInPrepareScript)) {
        if (checkForPropertyChange(property, propertyMapByKind(freshProduct, property)))
            return true;
    }

    for (QHash<QString, PropertySet>::ConstIterator it =
         restoredTrafo->propertiesRequestedFromArtifactInPrepareScript.constBegin();
         it != restoredTrafo->propertiesRequestedFromArtifactInPrepareScript.constEnd(); ++it) {
        const SourceArtifactConstPtr &artifact
                = findSourceArtifact(freshProduct, it.key(), artifactMap);
        if (!artifact)
            continue;
        for (const Property &property : qAsConst(it.value())) {
            if (checkForPropertyChange(property, artifact->properties->value()))
                return true;
        }
    }
    return false;
}

bool BuildGraphLoader::checkForEnvChanges(const TransformerPtr &restoredTrafo)
{
    if (!m_envChange)
        return false;
    // TODO: Also check results of getEnv() from commands here; we currently do not track them
    for (const AbstractCommandPtr &c : qAsConst(restoredTrafo->commands)) {
        if (c->type() != AbstractCommand::ProcessCommandType)
            continue;
        for (const QString &var : std::static_pointer_cast<ProcessCommand>(c)->relevantEnvVars()) {
            if (restoredTrafo->product()->topLevelProject()->environment.value(var)
                    != m_result.newlyResolvedProject->environment.value(var)) {
                invalidateTransformer(restoredTrafo);
                return true;
            }
        }
    }
    return false;
}

bool BuildGraphLoader::checkForPropertyChange(const Property &restoredProperty,
                                              const QVariantMap &newProperties)
{
    QVariant v;
    switch (restoredProperty.kind) {
    case Property::PropertyInProduct:
    case Property::PropertyInProject:
        v = newProperties.value(restoredProperty.propertyName);
        break;
    case Property::PropertyInModule:
        v = moduleProperty(newProperties, restoredProperty.moduleName,
                           restoredProperty.propertyName);
        break;
    case Property::PropertyInParameters: {
        const int sepIndex = restoredProperty.moduleName.indexOf(QLatin1Char(':'));
        QualifiedId moduleName
                = QualifiedId::fromString(restoredProperty.moduleName.mid(sepIndex + 1));
        QVariantMap map = newProperties;
        while (!moduleName.isEmpty())
            map = map.value(moduleName.takeFirst()).toMap();
        v = map.value(restoredProperty.propertyName);
    }
    }
    if (restoredProperty.value != v) {
        m_logger.qbsDebug() << "Value for property '" << restoredProperty.moduleName << "."
                            << restoredProperty.propertyName << "' has changed.";
        m_logger.qbsDebug() << "Old value was '" << restoredProperty.value << "'.";
        m_logger.qbsDebug() << "New value is '" << v << "'.";
        return true;
    }
    return false;
}

void BuildGraphLoader::replaceFileDependencyWithArtifact(const ResolvedProductPtr &fileDepProduct,
        FileDependency *filedep, Artifact *artifact)
{
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace()
            << QString::fromLatin1("[BG] replace file dependency '%1' with artifact of type '%2'")
               .arg(filedep->filePath()).arg(artifact->artifactType);
    }
    for (const ResolvedProductPtr &product : fileDepProduct->topLevelProject()->allProducts()) {
        if (!product->buildData)
            continue;
        for (Artifact *artifactInProduct : filterByType<Artifact>(product->buildData->nodes)) {
            if (artifactInProduct->fileDependencies.remove(filedep))
                loggedConnect(artifactInProduct, artifact, m_logger);
        }
    }
    fileDepProduct->topLevelProject()->buildData->fileDependencies.remove(filedep);
    fileDepProduct->topLevelProject()->buildData->removeFromLookupTable(filedep);
    m_objectsToDelete << filedep;
}

bool BuildGraphLoader::checkConfigCompatibility()
{
    const TopLevelProjectConstPtr restoredProject = m_result.loadedProject;
    if (m_parameters.topLevelProfile().isEmpty())
        m_parameters.setTopLevelProfile(restoredProject->profile());
    if (!m_parameters.overrideBuildGraphData()) {
        if (!m_parameters.overriddenValues().isEmpty()
                && m_parameters.overriddenValues() != restoredProject->overriddenValues) {
            throw ErrorInfo(Tr::tr("Property values set on the command line differ from the "
                                   "ones used for the previous build. Use the 'resolve' command if "
                                   "you really want to rebuild with the new properties."));
            }
        m_parameters.setOverriddenValues(restoredProject->overriddenValues);
        if (m_parameters.topLevelProfile() != restoredProject->profile()) {
            throw ErrorInfo(Tr::tr("The current profile is '%1', but profile '%2' was used "
                                   "when last building for configuration '%3'. Use  the "
                                   "'resolve' command if you really want to rebuild with a "
                                   "different profile.")
                            .arg(m_parameters.topLevelProfile(), restoredProject->profile(),
                                 m_parameters.configurationName()));
        }
        m_parameters.setTopLevelProfile(restoredProject->profile());
        m_parameters.expandBuildConfiguration();
    }
    if (!m_parameters.overrideBuildGraphData())
        return true;
    if (m_parameters.finalBuildConfigurationTree() != restoredProject->buildConfiguration())
        return false;
    for (QVariantMap::ConstIterator it = restoredProject->profileConfigs.constBegin();
         it != restoredProject->profileConfigs.constEnd(); ++it) {
        const QVariantMap buildConfig = SetupProjectParameters::expandedBuildConfiguration(
                    m_parameters.settingsDirectory(), it.key(), m_parameters.configurationName());
        const QVariantMap newConfig = SetupProjectParameters::finalBuildConfigurationTree(
                    buildConfig, m_parameters.overriddenValues());
        if (newConfig != it.value())
            return false;
    }
    return true;
}

bool BuildGraphLoader::isPrepareScriptUpToDate(const ScriptFunctionConstPtr &script,
                                               const FileTime &referenceTime)
{
    for (const JsImport &jsImport : script->fileContext->jsImports()) {
        for (const QString &filePath : qAsConst(jsImport.filePaths)) {
            if (FileInfo(filePath).lastModified() > referenceTime) {
                m_logger.qbsDebug() << "Change in import '" << filePath
                        << "' potentially influences prepare script, marking as out of date";
                return false;
            }
        }
    }
    return true;
}

void BuildGraphLoader::rescueOldBuildData(const ResolvedProductConstPtr &restoredProduct,
        const ResolvedProductPtr &newlyResolvedProduct, const ChildListHash &childLists,
        const AllRescuableArtifactData &existingRad)
{
    QBS_CHECK(newlyResolvedProduct);
    if (!restoredProduct->enabled || !newlyResolvedProduct->enabled)
        return;

    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << QString::fromLatin1("[BG] rescue data of product '%1'")
                               .arg(restoredProduct->uniqueName());
    }
    QBS_CHECK(newlyResolvedProduct->buildData);
    QBS_CHECK(newlyResolvedProduct->buildData->rescuableArtifactData.isEmpty());
    newlyResolvedProduct->buildData->rescuableArtifactData = existingRad;

    // This is needed for artifacts created by rules, which happens later in the executor.
    for (Artifact * const oldArtifact : filterByType<Artifact>(restoredProduct->buildData->nodes)) {
        if (!oldArtifact->transformer)
            continue;
        Artifact * const newArtifact = lookupArtifact(newlyResolvedProduct, oldArtifact, false);
        if (!newArtifact) {
            RescuableArtifactData rad;
            rad.timeStamp = oldArtifact->timestamp();
            rad.fileTags = oldArtifact->fileTags();
            rad.properties = oldArtifact->properties;
            rad.commands = oldArtifact->transformer->commands;
            rad.propertiesRequestedInPrepareScript
                    = oldArtifact->transformer->propertiesRequestedInPrepareScript;
            rad.propertiesRequestedInCommands
                    = oldArtifact->transformer->propertiesRequestedInCommands;
            rad.propertiesRequestedFromArtifactInPrepareScript
                    = oldArtifact->transformer->propertiesRequestedFromArtifactInPrepareScript;
            rad.propertiesRequestedFromArtifactInCommands
                    = oldArtifact->transformer->propertiesRequestedFromArtifactInCommands;
            const ChildrenInfo &childrenInfo = childLists.value(oldArtifact);
            for (Artifact * const child : qAsConst(childrenInfo.children)) {
                rad.children << RescuableArtifactData::ChildData(child->product->name,
                        child->product->multiplexConfigurationId, child->filePath(),
                        childrenInfo.childrenAddedByScanner.contains(child));
            }
            newlyResolvedProduct->buildData->rescuableArtifactData.insert(
                        oldArtifact->filePath(), rad);
        }
    }
}

} // namespace Internal
} // namespace qbs
