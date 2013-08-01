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
#include "buildgraphloader.h"

#include "artifact.h"
#include "artifactlist.h"
#include "buildgraph.h"
#include "cycledetector.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include <language/language.h>
#include <language/loader.h>
#include <logging/translator.h>
#include <tools/propertyfinder.h>
#include <tools/qbsassert.h>
#include <tools/setupprojectparameters.h>

#include <QDir>
#include <QFileInfo>

namespace qbs {
namespace Internal {

BuildGraphLoader::BuildGraphLoader(const QProcessEnvironment &env, const Logger &logger) :
    m_logger(logger), m_environment(env)
{
}

static bool isConfigCompatible(const QVariantMap &userCfg, const QVariantMap &projectCfg)
{
    QVariantMap::const_iterator it = userCfg.begin();
    for (; it != userCfg.end(); ++it) {
        if (it.value().type() == QVariant::Map) {
            if (!isConfigCompatible(it.value().toMap(), projectCfg.value(it.key()).toMap()))
                return false;
        } else {
            QVariant value = projectCfg.value(it.key());
            if (!value.isNull() && value != it.value()) {
                return false;
            }
        }
    }
    return true;
}

static void restoreBackPointers(const ResolvedProjectPtr &project)
{
    foreach (const ResolvedProductPtr &product, project->products) {
        product->project = project;
        if (!product->buildData)
            continue;
        foreach (Artifact * const a, product->buildData->artifacts)
            project->topLevelProject()->buildData->insertIntoLookupTable(a);
    }

    foreach (const ResolvedProjectPtr &subProject, project->subProjects) {
        subProject->parentProject = project;
        restoreBackPointers(subProject);
    }
}

BuildGraphLoadResult BuildGraphLoader::load(const SetupProjectParameters &parameters,
        const RulesEvaluationContextPtr &evalContext)
{
    m_result = BuildGraphLoadResult();
    m_evalContext = evalContext;

    const QString projectId = TopLevelProject::deriveId(parameters.buildConfigurationTree());
    const QString buildDir
            = TopLevelProject::deriveBuildDirectory(parameters.buildRoot(), projectId);
    const QString buildGraphFilePath
            = ProjectBuildData::deriveBuildGraphFilePath(buildDir, projectId);

    PersistentPool pool(m_logger);
    m_logger.qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    try {
        pool.load(buildGraphFilePath);
    } catch (const ErrorInfo &loadError) {
        if (parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly)
            throw loadError;
        m_logger.qbsInfo() << loadError.toString();
        return m_result;
    }

    if (!isConfigCompatible(parameters.buildConfigurationTree(), pool.headData().projectConfig)) {
        const QString message = Tr::tr("Cannot use stored build graph at '%1':"
                "Incompatible project configuration.").arg(buildGraphFilePath);
        if (parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly)
            throw ErrorInfo(message);
        m_logger.qbsInfo() << message;
        return m_result;
    }

    const TopLevelProjectPtr project = TopLevelProject::create();

    // TODO: Store some meta data that will enable us to show actual progress (e.g. number of products).
    evalContext->initializeObserver(Tr::tr("Restoring build graph from disk"), 1);

    project->load(pool);
    project->buildData->evaluationContext = evalContext;

    if (QFileInfo(project->location.fileName()) != QFileInfo(parameters.projectFilePath())) {
        QString errorMessage = Tr::tr("Stored build graph is for project file '%1', but "
                                      "input file is '%2'. ")
                .arg(QDir::toNativeSeparators(project->location.fileName()),
                     QDir::toNativeSeparators(parameters.projectFilePath()));
        if (!parameters.ignoreDifferentProjectFilePath()) {
            errorMessage += Tr::tr("Aborting.");
            throw ErrorInfo(errorMessage);
        }

        // Okay, let's assume it's the same project anyway (the source dir might have moved).
        errorMessage += Tr::tr("Ignoring.");
        m_logger.qbsWarning() << errorMessage;
    }

    restoreBackPointers(project);

    project->location = CodeLocation(parameters.projectFilePath(), project->location.line(),
                                     project->location.column());
    project->setBuildConfiguration(pool.headData().projectConfig);
    project->buildDirectory = buildDir;
    m_result.loadedProject = project;
    evalContext->incrementProgressValue();

    if (parameters.restoreBehavior() == SetupProjectParameters::RestoreOnly)
        return m_result;
    QBS_CHECK(parameters.restoreBehavior() == SetupProjectParameters::RestoreAndTrackChanges);

    trackProjectChanges(parameters, buildGraphFilePath, project);
    return m_result;
}

void BuildGraphLoader::trackProjectChanges(const SetupProjectParameters &parameters,
        const QString &buildGraphFilePath, const TopLevelProjectPtr &restoredProject)
{
    const FileTime buildGraphTimeStamp = FileInfo(buildGraphFilePath).lastModified();
    QSet<QString> buildSystemFiles = restoredProject->buildSystemFiles;
    QList<ResolvedProductPtr> allRestoredProducts = restoredProject->allProducts();
    QList<ResolvedProductPtr> changedProducts;
    bool reResolvingNecessary = hasProductFileChanged(allRestoredProducts, buildGraphTimeStamp,
                                                      buildSystemFiles, changedProducts);

    // "External" changes, e.g. in the environment or in a JavaScript file,
    // can make the list of source files in a product change without the respective file
    // having been touched. In such a case, the build data for that product will have to be set up
    // anew.
    bool allProductsPotentiallyChanged = false;
    if (hasBuildSystemFileChanged(buildSystemFiles, buildGraphTimeStamp)
            || hasEnvironmentChanged(restoredProject)
            || hasFileExistsResultChanged(restoredProject)) {
        reResolvingNecessary = true;
        allProductsPotentiallyChanged = true;
    }

   if (!reResolvingNecessary)
       return;

    restoredProject->buildData->isDirty = true;
    Loader ldr(m_evalContext->engine(), m_logger);
    ldr.setSearchPaths(parameters.searchPaths());
    ldr.setProgressObserver(m_evalContext->observer());
    m_result.newlyResolvedProject = ldr.loadProject(parameters);

    QMap<QString, ResolvedProductPtr> freshProductsByName;
    QList<ResolvedProductPtr> allNewlyResolvedProducts
            = m_result.newlyResolvedProject->allProducts();
    foreach (const ResolvedProductPtr &cp, allNewlyResolvedProducts)
        freshProductsByName.insert(cp->name, cp);

    QSet<TransformerPtr> seenTransformers;
    foreach (const ResolvedProductPtr &product, allRestoredProducts) {
        if (!product->buildData)
            continue;
        foreach (Artifact *artifact, product->buildData->artifacts) {
            if (!artifact->transformer || seenTransformers.contains(artifact->transformer))
                continue;
            seenTransformers.insert(artifact->transformer);
            ResolvedProductPtr freshProduct = freshProductsByName.value(product->name);
            if (freshProduct && checkForPropertyChanges(artifact->transformer, freshProduct)) {
                m_logger.qbsDebug() << "Cannot re-use build graph due to property changes "
                                       "in product '" << freshProduct->name << "'.";
                m_result.discardLoadedProject = true;
                return;
            }
        }
    }

    if (allProductsPotentiallyChanged)
        checkAllProductsForChanges(allRestoredProducts, freshProductsByName, changedProducts);
    foreach (const ResolvedProductPtr &product, changedProducts) {
        ResolvedProductPtr freshProduct = freshProductsByName.value(product->name);
        if (!freshProduct)
            continue;
        onProductChanged(product, freshProduct);
        if (m_result.discardLoadedProject)
            return;
    }

    // Move over restored build data to newly resolved project.
    m_result.newlyResolvedProject->buildData.swap(restoredProject->buildData);
    QBS_CHECK(m_result.newlyResolvedProject->buildData);
    m_result.newlyResolvedProject->buildData->isDirty = true;
    for (int i = allNewlyResolvedProducts.count() - 1; i >= 0; --i) {
        const ResolvedProductPtr &newlyResolvedProduct = allNewlyResolvedProducts.at(i);
        for (int j = allRestoredProducts.count() - 1; j >= 0; --j) {
            const ResolvedProductPtr &restoredProduct = allRestoredProducts.at(j);
            if (newlyResolvedProduct->name == restoredProduct->name) {
                newlyResolvedProduct->buildData.swap(restoredProduct->buildData);
                if (newlyResolvedProduct->buildData) {
                    foreach (Artifact * const a, newlyResolvedProduct->buildData->artifacts)
                        a->product = newlyResolvedProduct;
                }

                // Keep in list if build data still needs to be resolved.
                if (!newlyResolvedProduct->enabled || newlyResolvedProduct->buildData)
                    allNewlyResolvedProducts.removeAt(i);

                allRestoredProducts.removeAt(j);
                break;
            }
        }
    }

    // Products still left in the list need resolving, either because they are new
    // or because they are newly enabled.
    if (!allNewlyResolvedProducts.isEmpty()) {
        BuildDataResolver bpr(m_logger);
        bpr.resolveProductBuildDataForExistingProject(m_result.newlyResolvedProject,
                                                      allNewlyResolvedProducts);
    }

    // Products still left in the list do not exist anymore.
    foreach (const ResolvedProductPtr &removedProduct, allRestoredProducts)
        onProductRemoved(removedProduct, m_result.newlyResolvedProject->buildData.data());

    CycleDetector(m_logger).visitProject(m_result.newlyResolvedProject);
}

bool BuildGraphLoader::hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const
{
    for (QHash<QString, QString>::ConstIterator it = restoredProject->usedEnvironment.constBegin();
         it != restoredProject->usedEnvironment.constEnd(); ++it) {
        if (m_environment.value(it.key()) != it.value()) {
            m_logger.qbsDebug() << "A relevant environment variable changed, "
                                   "must re-resolve project.";
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

bool BuildGraphLoader::hasProductFileChanged(const QList<ResolvedProductPtr> &restoredProducts,
        const FileTime &referenceTime, QSet<QString> &remainingBuildSystemFiles,
        QList<ResolvedProductPtr> &changedProducts)
{
    bool hasChanged = false;
    foreach (const ResolvedProductPtr &product, restoredProducts) {
        const QString fileName = product->location.fileName();
        const FileInfo pfi(fileName);
        remainingBuildSystemFiles.remove(fileName);
        if (!pfi.exists()) {
            m_logger.qbsDebug() << "A product was removed, must re-resolve project";
            hasChanged = true;
        } else if (referenceTime < pfi.lastModified()) {
            m_logger.qbsDebug() << "A product was changed, must re-resolve project";
            changedProducts += product;
        } else {
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
                changedProducts += product;
                break;
            }
        }
    }

    return hasChanged | !changedProducts.isEmpty();
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
        if (changedProducts.contains(restoredProduct))
            continue;
        const ResolvedProductPtr newlyResolvedProduct
                = newlyResolvedProductsByName.value(restoredProduct->name);
        if (!newlyResolvedProduct)
            continue;
        if (newlyResolvedProduct->allFiles() == restoredProduct->allFiles())
            continue;
        m_logger.qbsDebug() << "Product '" << restoredProduct->name
                            << "' was changed, must re-resolve project";
        changedProducts << restoredProduct;
    }
}

void BuildGraphLoader::onProductRemoved(const ResolvedProductPtr &product,
                                        ProjectBuildData *projectBuildData)
{
    m_logger.qbsDebug() << "[BG] product '" << product->name << "' removed.";

    product->project->products.removeOne(product);

    // delete all removed artifacts physically from the disk
    if (product->buildData) {
        foreach (Artifact *artifact, product->buildData->artifacts)
            projectBuildData->removeArtifact(artifact, projectBuildData, m_logger);
    }
}

void BuildGraphLoader::onProductChanged(const ResolvedProductPtr &product,
                                        const ResolvedProductPtr &changedProduct)
{
    m_logger.qbsDebug() << "[BG] product '" << product->name << "' changed.";

    ArtifactsPerFileTagMap artifactsPerFileTag;
    QList<Artifact *> addedArtifacts;
    ArtifactList artifactsToRemove;
    QHash<QString, SourceArtifactConstPtr> oldArtifacts, newArtifacts;

    const QList<SourceArtifactPtr> oldProductAllFiles = product->allEnabledFiles();
    foreach (const SourceArtifactConstPtr &a, oldProductAllFiles)
        oldArtifacts.insert(a->absoluteFilePath, a);
    foreach (const SourceArtifactPtr &a, changedProduct->allEnabledFiles()) {
        newArtifacts.insert(a->absoluteFilePath, a);
        if (!oldArtifacts.contains(a->absoluteFilePath)) {
            // artifact added
            m_logger.qbsDebug() << "[BG] artifact '" << a->absoluteFilePath
                                << "' added to product " << product->name;
            Artifact *newArtifact = lookupArtifact(product, a->absoluteFilePath);
            if (newArtifact) {
                // User added a source file that was a generated artifact in the previous
                // build, e.g. a C++ source file that was generated and now is a non-generated
                // source file.
                newArtifact->artifactType = Artifact::SourceFile;
            } else {
                newArtifact = createArtifact(product, a, m_logger);
                foreach (FileResourceBase *oldArtifactLookupResult,
                         product->topLevelProject()->buildData->lookupFiles(newArtifact->filePath())) {
                    if (oldArtifactLookupResult == newArtifact)
                        continue;
                    FileDependency *oldFileDependency
                            = dynamic_cast<FileDependency *>(oldArtifactLookupResult);
                    if (!oldFileDependency) {
                        // The source file already exists in another product.
                        continue;
                    }
                    // User added a source file that was recognized as file dependency in the
                    // previous build, e.g. a C++ header file.
                    replaceFileDependencyWithArtifact(product,
                                                      oldFileDependency,
                                                      newArtifact);
                }
            }
            addedArtifacts += newArtifact;
        }
    }

    foreach (const SourceArtifactPtr &a, oldProductAllFiles) {
        const SourceArtifactConstPtr changedArtifact = newArtifacts.value(a->absoluteFilePath);
        if (!changedArtifact) {
            // artifact removed
            m_logger.qbsDebug() << "[BG] artifact '" << a->absoluteFilePath
                                << "' removed from product " << product->name;
            Artifact *artifact = lookupArtifact(product, a->absoluteFilePath);
            QBS_CHECK(artifact);
            removeArtifactAndExclusiveDependents(artifact, &artifactsToRemove);
            continue;
        }
        if (changedArtifact->fileTags != a->fileTags) {
            // artifact's filetags have changed
            m_logger.qbsDebug() << "[BG] filetags have changed for artifact '"
                    << a->absoluteFilePath << "' from " << a->fileTags << " to "
                    << changedArtifact->fileTags;
            Artifact *artifact = lookupArtifact(product, a->absoluteFilePath);
            QBS_CHECK(artifact);

            // handle added filetags
            foreach (const FileTag &addedFileTag, changedArtifact->fileTags - a->fileTags)
                artifactsPerFileTag[addedFileTag] += artifact;

            // handle removed filetags
            foreach (const FileTag &removedFileTag, a->fileTags - changedArtifact->fileTags) {
                artifact->fileTags -= removedFileTag;
                foreach (Artifact *parent, artifact->parents) {
                    if (parent->transformer && parent->transformer->rule->inputs.contains(removedFileTag)) {
                        // this parent has been created because of the removed filetag
                        removeArtifactAndExclusiveDependents(parent, &artifactsToRemove);
                    }
                }
            }
        }
    }

    // Discard groups of the old product. Use the groups of the new one.
    product->groups = changedProduct->groups;
    product->properties = changedProduct->properties;

    // apply rules for new artifacts
    foreach (Artifact *artifact, addedArtifacts)
        foreach (const FileTag &ft, artifact->fileTags)
            artifactsPerFileTag[ft] += artifact;
    RulesApplicator(product, artifactsPerFileTag, m_logger).applyAllRules();

    addTargetArtifacts(product, artifactsPerFileTag, m_logger);

    // parents of removed artifacts must update their transformers
    foreach (Artifact *removedArtifact, artifactsToRemove)
        foreach (Artifact *parent, removedArtifact->parents)
            product->topLevelProject()->buildData->artifactsThatMustGetNewTransformers += parent;
    product->topLevelProject()->buildData->updateNodesThatMustGetNewTransformer(m_logger);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, artifactsToRemove) {
        removeGeneratedArtifactFromDisk(artifact, m_logger);
        delete artifact;
    }
}

/**
  * Removes the artifact and all the artifacts that depend exclusively on it.
  * Example: if you remove a cpp artifact then the obj artifact is removed but
  * not the resulting application (if there's more then one cpp artifact).
  */
void BuildGraphLoader::removeArtifactAndExclusiveDependents(Artifact *artifact,
        ArtifactList *removedArtifacts)
{
    if (removedArtifacts)
        removedArtifacts->insert(artifact);
    TopLevelProject * const project = artifact->product->topLevelProject();
    foreach (Artifact *parent, artifact->parents) {
        bool removeParent = false;
        disconnect(parent, artifact, m_logger);
        if (parent->children.isEmpty()) {
            removeParent = true;
        } else if (parent->transformer) {
            project->buildData->artifactsThatMustGetNewTransformers += parent;
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.isEmpty();
        }
        if (removeParent)
            removeArtifactAndExclusiveDependents(parent, removedArtifacts);
    }
    project->buildData->removeArtifact(artifact, m_logger);
}

bool BuildGraphLoader::checkForPropertyChanges(const TransformerPtr &restoredTrafo,
                                               const ResolvedProductPtr &freshProduct)
{
    PropertyFinder finder;
    foreach (const Property &property, restoredTrafo->modulePropertiesUsedInPrepareScript) {
        QVariant v;
        if (property.kind == Property::PropertyInProduct) {
            v = freshProduct->properties->value().value(property.propertyName);
        } else if (property.value.type() == QVariant::List) {
            v = finder.propertyValues(freshProduct->properties->value(), property.moduleName,
                                      property.propertyName);
        } else {
            v = finder.propertyValue(freshProduct->properties->value(), property.moduleName,
                                     property.propertyName);
        }
        if (property.value != v) {
            m_logger.qbsDebug() << "Value for property '" << property.moduleName << "."
                                << property.propertyName << "' has changed.";
            m_logger.qbsDebug() << "Old value was '" << property.value << "'.";
            m_logger.qbsDebug() << "New value is '" << v << "'.";
            return true;
        }
    }
    return false;
}

void BuildGraphLoader::replaceFileDependencyWithArtifact(const ResolvedProductPtr &fileDepProduct,
        FileDependency *filedep, Artifact *artifact)
{
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace()
            << QString::fromLocal8Bit("[BG] replace file dependency '%1' "
                                      "with artifact of type '%2'")
                             .arg(filedep->filePath()).arg(artifact->artifactType);
    }
    foreach (const ResolvedProductPtr &product, fileDepProduct->topLevelProject()->allProducts()) {
        if (!product->buildData)
            continue;
        foreach (Artifact *artifactInProduct, product->buildData->artifacts) {
            if (artifactInProduct->fileDependencies.contains(filedep)) {
                artifactInProduct->fileDependencies.remove(filedep);
                loggedConnect(artifactInProduct, artifact, m_logger);
            }
        }
    }
    fileDepProduct->topLevelProject()->buildData->fileDependencies.remove(filedep);
    fileDepProduct->topLevelProject()->buildData->removeFromLookupTable(filedep);
    delete filedep;
}

void addTargetArtifacts(const ResolvedProductPtr &product,
                        ArtifactsPerFileTagMap &artifactsPerFileTag, const Logger &logger)
{
    foreach (const FileTag &fileTag, product->fileTags) {
        foreach (Artifact * const artifact, artifactsPerFileTag.value(fileTag)) {
            if (artifact->artifactType == Artifact::Generated)
                product->buildData->targetArtifacts += artifact;
        }
    }
    if (product->buildData->targetArtifacts.isEmpty()) {
        const QString msg = QString::fromLocal8Bit("No artifacts generated for product '%1'.");
        logger.qbsDebug() << msg.arg(product->name);
    }
}

} // namespace Internal
} // namespace qbs
