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

#include "artifact.h"
#include "artifactset.h"
#include "buildgraph.h"
#include "command.h"
#include "cycledetector.h"
#include "emptydirectoriesremover.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <language/artifactproperties.h>
#include <language/language.h>
#include <language/loader.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/profiling.h>
#include <tools/propertyfinder.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QFileInfo>

namespace qbs {
namespace Internal {

BuildGraphLoader::BuildGraphLoader(const QProcessEnvironment &env, const Logger &logger) :
    m_logger(logger), m_environment(env)
{
}

BuildGraphLoader::~BuildGraphLoader()
{
    qDeleteAll(m_objectsToDelete);
}

static void restoreBackPointers(const ResolvedProjectPtr &project)
{
    foreach (const ResolvedProductPtr &product, project->products) {
        product->project = project;
        if (!product->buildData)
            continue;
        foreach (BuildGraphNode * const n, product->buildData->nodes) {
            if (Artifact *a = dynamic_cast<Artifact *>(n))
                project->topLevelProject()->buildData->insertIntoLookupTable(a);
        }
    }

    foreach (const ResolvedProjectPtr &subProject, project->subProjects) {
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
        checkBuildGraphCompatibility(existingProject);
        m_result.loadedProject = existingProject;
    } else {
        loadBuildGraphFromDisk();
    }
    if (!m_result.loadedProject)
        return m_result;
    if (parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly) {
        foreach (const ErrorInfo &e, m_result.loadedProject->warningsEncountered)
            m_logger.printWarning(e);
        return m_result;
    }
    QBS_CHECK(parameters.restoreBehavior() == SetupProjectParameters::RestoreAndTrackChanges);

    if (m_parameters.logElapsedTime())
        m_wildcardExpansionEffort = 0;
    trackProjectChanges();
    if (m_parameters.logElapsedTime()) {
        m_logger.qbsLog(LoggerInfo, true) << "\t"
                << Tr::tr("Wilcard expansion took %1.")
                   .arg(elapsedTimeString(m_wildcardExpansionEffort));
    }
    return m_result;
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
    } catch (const ErrorInfo &loadError) {
        if (m_parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly)
            throw;
        m_logger.qbsInfo() << loadError.toString();
        return;
    }

    const TopLevelProjectPtr project = TopLevelProject::create();

    // TODO: Store some meta data that will enable us to show actual progress (e.g. number of products).
    m_evalContext->initializeObserver(Tr::tr("Restoring build graph from disk"), 1);

    project->load(pool);
    project->buildData->evaluationContext = m_evalContext;
    project->setBuildConfiguration(pool.headData().projectConfig);
    project->buildDirectory = buildDir;
    checkBuildGraphCompatibility(project);
    restoreBackPointers(project);
    project->location = CodeLocation(m_parameters.projectFilePath(), project->location.line(),
                                     project->location.column());
    m_result.loadedProject = project;
    m_evalContext->incrementProgressValue();
    doSanityChecks(project, m_logger);
}

void BuildGraphLoader::checkBuildGraphCompatibility(const TopLevelProjectConstPtr &project)
{
    if (QFileInfo(project->location.filePath()) != QFileInfo(m_parameters.projectFilePath())) {
        QString errorMessage = Tr::tr("Stored build graph at '%1' is for project file '%2', but "
                                      "input file is '%3'. ")
                .arg(QDir::toNativeSeparators(project->buildGraphFilePath()),
                     QDir::toNativeSeparators(project->location.filePath()),
                     QDir::toNativeSeparators(m_parameters.projectFilePath()));
        if (!m_parameters.ignoreDifferentProjectFilePath()) {
            errorMessage += Tr::tr("Aborting.");
            throw ErrorInfo(errorMessage);
        }

        // Okay, let's assume it's the same project anyway (the source dir might have moved).
        errorMessage += Tr::tr("Ignoring.");
        m_logger.qbsWarning() << errorMessage;
    }
}

static bool checkProductForChangedDependency(QList<ResolvedProductPtr> &changedProducts,
        QSet<ResolvedProductPtr> &seenProducts, const ResolvedProductPtr &product)
{
    if (seenProducts.contains(product))
        return false;
    if (changedProducts.contains(product))
        return true;
    foreach (const ResolvedProductPtr &dep, product->dependencies) {
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
    QSet<ResolvedProductPtr> seenProducts;
    foreach (const ResolvedProductPtr &p, allRestoredProducts)
        checkProductForChangedDependency(changedProducts, seenProducts, p);
}

void BuildGraphLoader::trackProjectChanges()
{
    TimedActivityLogger trackingTimer(m_logger, Tr::tr("Change tracking"),
                                      m_parameters.logElapsedTime());
    const TopLevelProjectPtr &restoredProject = m_result.loadedProject;
    QSet<QString> buildSystemFiles = restoredProject->buildSystemFiles;
    QList<ResolvedProductPtr> allRestoredProducts = restoredProject->allProducts();
    QList<ResolvedProductPtr> changedProducts;
    bool reResolvingNecessary = false;
    if (!isConfigCompatible())
        reResolvingNecessary = true;
    if (hasProductFileChanged(allRestoredProducts, restoredProject->lastResolveTime,
                              buildSystemFiles, changedProducts)) {
        reResolvingNecessary = true;
    }

    // "External" changes, e.g. in the environment or in a JavaScript file,
    // can make the list of source files in a product change without the respective file
    // having been touched. In such a case, the build data for that product will have to be set up
    // anew.
    if (probeExecutionForced(allRestoredProducts)
            || hasBuildSystemFileChanged(buildSystemFiles, restoredProject->lastResolveTime)
            || hasEnvironmentChanged(restoredProject)
            || hasCanonicalFilePathResultChanged(restoredProject)
            || hasFileExistsResultChanged(restoredProject)
            || hasDirectoryEntriesResultChanged(restoredProject)
            || hasFileLastModifiedResultChanged(restoredProject)) {
        reResolvingNecessary = true;
    }

    if (!reResolvingNecessary) {
        foreach (const ErrorInfo &e, restoredProject->warningsEncountered)
            m_logger.printWarning(e);
        return;
    }

    restoredProject->buildData->isDirty = true;
    Loader ldr(m_evalContext->engine(), m_logger);
    ldr.setSearchPaths(m_parameters.searchPaths());
    ldr.setProgressObserver(m_evalContext->observer());
    QHash<QString, QList<ProbeConstPtr>> restoredProbes;
    foreach (const auto restoredProduct, allRestoredProducts)
        restoredProbes.insert(restoredProduct->uniqueName(), restoredProduct->probes);
    ldr.setOldProbes(restoredProbes);
    m_result.newlyResolvedProject = ldr.loadProject(m_parameters);

    QMap<QString, ResolvedProductPtr> freshProductsByName;
    QList<ResolvedProductPtr> allNewlyResolvedProducts
            = m_result.newlyResolvedProject->allProducts();
    foreach (const ResolvedProductPtr &cp, allNewlyResolvedProducts)
        freshProductsByName.insert(cp->uniqueName(), cp);

    checkAllProductsForChanges(allRestoredProducts, freshProductsByName, changedProducts);

    QSharedPointer<ProjectBuildData> oldBuildData;
    ChildListHash childLists;
    if (!changedProducts.isEmpty()) {
        oldBuildData = QSharedPointer<ProjectBuildData>(
                    new ProjectBuildData(restoredProject->buildData.data()));
        foreach (const ResolvedProductConstPtr &product, allRestoredProducts) {
            if (!product->buildData)
                continue;

            // If the product gets temporarily removed, its artifacts will get disconnected
            // and this structural information will no longer be directly available from them.
            for (const Artifact *a : filterByType<Artifact>(product->buildData->nodes)) {
                childLists.insert(a, ChildrenInfo(ArtifactSet::fromNodeSet(a->children),
                                                  a->childrenAddedByScanner));
            }
        }
    }

    makeChangedProductsListComplete(changedProducts, allRestoredProducts);

    // Set up build data from scratch for all changed products. This does not necessarily
    // mean that artifacts will have to get rebuilt; whether this is necesessary will be decided
    // an a per-artifact basis by the Executor on the next build.
    QHash<QString, AllRescuableArtifactData> rescuableArtifactData;
    foreach (const ResolvedProductPtr &product, changedProducts) {
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
                    foreach (BuildGraphNode *node, newlyResolvedProduct->buildData->nodes)
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
    foreach (const ResolvedProductPtr &removedProduct, allRestoredProducts) {
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

    foreach (const ResolvedProductConstPtr &changedProduct, changedProducts) {
        rescueOldBuildData(changedProduct, freshProductsByName.value(changedProduct->uniqueName()),
                           childLists, rescuableArtifactData.value(changedProduct->uniqueName()));
    }

    EmptyDirectoriesRemover(m_result.newlyResolvedProject.data(), m_logger)
            .removeEmptyParentDirectories(m_artifactsRemovedFromDisk);

    foreach (FileResourceBase * const f, m_objectsToDelete) {
        Artifact * const a = dynamic_cast<Artifact *>(f);
        if (a)
            a->product.clear(); // To help with the sanity checks.
    }
    doSanityChecks(m_result.newlyResolvedProject, m_logger);
}

bool BuildGraphLoader::probeExecutionForced(const QList<ResolvedProductPtr> &restoredProducts) const
{
    if (!m_parameters.forceProbeExecution())
        return false;
    foreach (const auto &p, restoredProducts) {
        if (!p->probes.isEmpty())
            return true;
    }
    return false;
}

bool BuildGraphLoader::hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    QProcessEnvironment oldEnv = restoredProject->environment;
    QProcessEnvironment newEnv = m_environment;

    // HACK. Valgrind screws up our null-build benchmarker otherwise.
    // TODO: Think about a (module-provided) whitelist.
    oldEnv.remove(QLatin1String("LD_PRELOAD"));
    newEnv.remove(QLatin1String("LD_PRELOAD"));

    if (oldEnv != newEnv) {
        m_logger.qbsDebug() << "Set of environment variables changed. Must re-resolve project.";
        m_logger.qbsTrace() << "old: " << restoredProject->environment.toStringList() << "\nnew:"
                            << m_environment.toStringList();
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
        const FileTime &referenceTime, QSet<QString> &remainingBuildSystemFiles,
        QList<ResolvedProductPtr> &changedProducts)
{
    bool hasChanged = false;
    foreach (const ResolvedProductPtr &product, restoredProducts) {
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
            AccumulatingTimer wildcardTimer(m_parameters.logElapsedTime()
                                            ? &m_wildcardExpansionEffort : nullptr);
            foreach (const GroupPtr &group, product->groups) {
                if (!group->wildcards)
                    continue;
                const QSet<QString> files
                        = group->wildcards->expandPatterns(group, product->sourceDirectory);
                QSet<QString> wcFiles;
                foreach (const SourceArtifactConstPtr &sourceArtifact, group->wildcards->files)
                    wcFiles += sourceArtifact->absoluteFilePath;
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

bool BuildGraphLoader::hasBuildSystemFileChanged(const QSet<QString> &buildSystemFiles,
                                                 const FileTime &referenceTime)
{
    foreach (const QString &file, buildSystemFiles) {
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
    foreach (const ResolvedProductPtr &restoredProduct, restoredProducts) {
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
    QSet<QString> names1;
    QSet<QString> names2;
    foreach (const ResolvedProductConstPtr &dep, p1->dependencies)
        names1 << dep->uniqueName();
    foreach (const ResolvedProductConstPtr &dep, p2->dependencies)
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
    foreach (const RuleConstPtr &rule, newlyResolvedProduct->rules) {
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
    foreach (const QString &key, specialProperties) {
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
    m_logger.qbsDebug() << "Checking for changes in properties requested in prepare scripts for "
                           "product '"  << restoredProduct->uniqueName() << "'.";
    if (!restoredProduct->buildData)
        return false;

    // This check must come first, as it can prevent build data rescuing.
    if (checkTransformersForPropertyChanges(restoredProduct, newlyResolvedProduct))
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

bool BuildGraphLoader::checkTransformersForPropertyChanges(const ResolvedProductPtr &restoredProduct,
        const ResolvedProductPtr &newlyResolvedProduct)
{
    bool transformerChanges = false;
    QSet<TransformerConstPtr> seenTransformers;
    for (Artifact *artifact : filterByType<Artifact>(restoredProduct->buildData->nodes)) {
        const TransformerPtr transformer = artifact->transformer;
        if (!transformer || seenTransformers.contains(transformer))
            continue;
        seenTransformers.insert(transformer);
        if (checkForPropertyChanges(transformer, newlyResolvedProduct))
            transformerChanges = true;
    }
    if (transformerChanges) {
        m_logger.qbsDebug() << "Property changes in product '"
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
        foreach (BuildGraphNode * const node, product->buildData->nodes) {
            if (node->type() == BuildGraphNode::ArtifactNodeType) {
                Artifact * const artifact = static_cast<Artifact *>(node);
                projectBuildData->removeArtifact(artifact, m_logger, removeArtifactsFromDisk,
                                                 false);
                if (removeArtifactsFromDisk && artifact->artifactType == Artifact::Generated)
                    m_artifactsRemovedFromDisk << artifact->filePath();
            } else {
                foreach (BuildGraphNode * const parent, node->parents)
                    parent->children.remove(node);
                node->parents.clear();
                foreach (BuildGraphNode * const child, node->children)
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
        foreach (const SourceArtifactConstPtr &a, product->allFiles()) {
            if (a->absoluteFilePath == artifactFilePath) {
                artifact = a;
                break;
            }
        }
    }
    return artifact;
}

static QVariantMap propertyMapByKind(const ResolvedProductConstPtr &product, Property::Kind kind)
{
    switch (kind) {
    case Property::PropertyInModule:
        return product->moduleProperties->value();
    case Property::PropertyInProduct:
        return product->productProperties;
    case Property::PropertyInProject:
        return product->project->projectProperties();
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
    foreach (const Property &property, restoredTrafo->propertiesRequestedInCommands) {
        if (checkForPropertyChange(property, propertyMapByKind(freshProduct, property.kind))) {
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
        foreach (const Property &property, it.value()) {
            if (checkForPropertyChange(property, artifact->properties->value())) {
                invalidateTransformer(restoredTrafo);
                return true;
            }
        }
    }

    foreach (const Property &property, restoredTrafo->propertiesRequestedInPrepareScript) {
        if (checkForPropertyChange(property, propertyMapByKind(freshProduct, property.kind)))
            return true;
    }

    for (QHash<QString, PropertySet>::ConstIterator it =
         restoredTrafo->propertiesRequestedFromArtifactInPrepareScript.constBegin();
         it != restoredTrafo->propertiesRequestedFromArtifactInPrepareScript.constEnd(); ++it) {
        const SourceArtifactConstPtr artifact
                = findSourceArtifact(freshProduct, it.key(), artifactMap);
        if (!artifact)
            continue;
        foreach (const Property &property, it.value()) {
            if (checkForPropertyChange(property, artifact->properties->value()))
                return true;
        }
    }
    return false;
}

bool BuildGraphLoader::checkForPropertyChange(const Property &restoredProperty,
                                              const QVariantMap &newProperties)
{
    PropertyFinder finder;
    QVariant v;
    switch (restoredProperty.kind) {
    case Property::PropertyInProduct:
    case Property::PropertyInProject:
        v = newProperties.value(restoredProperty.propertyName);
        break;
    case Property::PropertyInModule:
        v = finder.propertyValue(newProperties, restoredProperty.moduleName,
                                 restoredProperty.propertyName);
        break;
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
    foreach (const ResolvedProductPtr &product, fileDepProduct->topLevelProject()->allProducts()) {
        if (!product->buildData)
            continue;
        for (Artifact *artifactInProduct : filterByType<Artifact>(product->buildData->nodes)) {
            if (artifactInProduct->fileDependencies.contains(filedep)) {
                artifactInProduct->fileDependencies.remove(filedep);
                loggedConnect(artifactInProduct, artifact, m_logger);
            }
        }
    }
    fileDepProduct->topLevelProject()->buildData->fileDependencies.remove(filedep);
    fileDepProduct->topLevelProject()->buildData->removeFromLookupTable(filedep);
    m_objectsToDelete << filedep;
}

bool BuildGraphLoader::isConfigCompatible()
{
    const TopLevelProjectConstPtr restoredProject = m_result.loadedProject;
    if (m_parameters.finalBuildConfigurationTree() != restoredProject->buildConfiguration())
        return false;
    for (QVariantMap::ConstIterator it = restoredProject->profileConfigs.constBegin();
         it != restoredProject->profileConfigs.constEnd(); ++it) {
        const QVariantMap buildConfig = SetupProjectParameters::expandedBuildConfiguration(
                    m_parameters.settingsDirectory(), it.key(), m_parameters.configurationName());
        const QVariantMap newConfig = SetupProjectParameters::finalBuildConfigurationTree(
                    buildConfig, m_parameters.overriddenValues(), m_parameters.buildRoot());
        if (newConfig != it.value())
            return false;
    }
    return true;
}

bool BuildGraphLoader::isPrepareScriptUpToDate(const ScriptFunctionConstPtr &script,
                                               const FileTime &referenceTime)
{
    foreach (const JsImport &jsImport, script->fileContext->jsImports()) {
        foreach (const QString &filePath, jsImport.filePaths) {
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
            rad.properties = oldArtifact->properties->value();
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
            foreach (Artifact * const child, childrenInfo.children) {
                rad.children << RescuableArtifactData::ChildData(child->product->name,
                        child->product->profile, child->filePath(),
                        childrenInfo.childrenAddedByScanner.contains(child));
            }
            newlyResolvedProduct->buildData->rescuableArtifactData.insert(
                        oldArtifact->filePath(), rad);
        }
    }
}

} // namespace Internal
} // namespace qbs
