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
#include "buildproject.h"

#include "artifact.h"
#include "buildgraph.h"
#include "buildproduct.h"
#include "cycledetector.h"
#include "rulesapplicator.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include <language/language.h>
#include <language/loader.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/persistence.h>
#include <tools/setupprojectparameters.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QFileInfo>

namespace qbs {
namespace Internal {

BuildProject::BuildProject(const Logger &logger)
    : m_evalContext(0), m_dirty(false), m_logger(logger)
{
}

BuildProject::~BuildProject()
{
    qDeleteAll(m_dependencyArtifacts);
}

void BuildProject::store() const
{
    if (!dirty()) {
        m_logger.qbsDebug() << "[BG] build graph is unchanged in project "
                            << resolvedProject()->id() << ".";
        return;
    }
    const QString fileName = buildGraphFilePath();
    m_logger.qbsDebug() << "[BG] storing: " << fileName;
    PersistentPool pool(m_logger);
    PersistentPool::HeadData headData;
    headData.projectConfig = resolvedProject()->buildConfiguration();
    pool.setHeadData(headData);
    pool.setupWriteStream(fileName);
    store(pool);
    m_dirty = false;
}

QString BuildProject::deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId)
{
    return buildDir + QLatin1Char('/') + projectId + QLatin1String(".bg");
}

QString BuildProject::buildGraphFilePath() const
{
    return deriveBuildGraphFilePath(resolvedProject()->buildDirectory, resolvedProject()->id());
}

void BuildProject::setEvaluationContext(const RulesEvaluationContextPtr &evalContext)
{
    m_evalContext = evalContext;
}

ResolvedProjectPtr BuildProject::resolvedProject() const
{
    return m_resolvedProject;
}

QSet<BuildProductPtr> BuildProject::buildProducts() const
{
    return m_buildProducts;
}

bool BuildProject::dirty() const
{
    return m_dirty;
}

void BuildProject::insertIntoArtifactLookupTable(Artifact *artifact)
{
    QList<Artifact *> &lst = m_artifactLookupTable[artifact->fileName()][artifact->dirPath()];
    if (!lst.contains(artifact))
        lst.append(artifact);
}

void BuildProject::removeFromArtifactLookupTable(Artifact *artifact)
{
    m_artifactLookupTable[artifact->fileName()][artifact->dirPath()].removeOne(artifact);
}

QList<Artifact *> BuildProject::lookupArtifacts(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifacts(dirPath, fileName);
}

QList<Artifact *> BuildProject::lookupArtifacts(const QString &dirPath, const QString &fileName) const
{
    QList<Artifact *> result = m_artifactLookupTable.value(fileName).value(dirPath);
    return result;
}

QList<Artifact *> BuildProject::lookupArtifacts(const Artifact *artifact) const
{
    return lookupArtifacts(artifact->dirPath(), artifact->fileName());
}

void BuildProject::insertFileDependency(Artifact *artifact)
{
    QBS_CHECK(artifact->artifactType == Artifact::FileDependency);
    m_dependencyArtifacts += artifact;
    insertIntoArtifactLookupTable(artifact);
}

/**
 * Copies dependencies between artifacts from the other project to this project.
 */
void BuildProject::rescueDependencies(const BuildProjectPtr &other)
{
    QHash<QString, BuildProductPtr> otherProductsByName;
    foreach (const BuildProductPtr &product, other->m_buildProducts)
        otherProductsByName.insert(product->rProduct->name, product);

    foreach (const BuildProductPtr &product, m_buildProducts) {
        BuildProductPtr otherProduct = otherProductsByName.value(product->rProduct->name);
        if (!otherProduct)
            continue;

        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << QString::fromLocal8Bit("[BG] rescue dependencies of "
                    "product '%1'").arg(product->rProduct->name);
        }

        foreach (Artifact *artifact, product->artifacts) {
            if (m_logger.traceEnabled()) {
                m_logger.qbsTrace() << QString::fromLocal8Bit("[BG]    artifact '%1'")
                                       .arg(artifact->fileName());
            }

            Artifact *otherArtifact = otherProduct->lookupArtifact(artifact);
            if (!otherArtifact || !otherArtifact->transformer)
                continue;

            foreach (Artifact *otherChild, otherArtifact->children) {
                // skip transform edges
                if (otherArtifact->transformer->inputs.contains(otherChild))
                    continue;

                QList<Artifact *> children = lookupArtifacts(otherChild);
                foreach (Artifact *child, children) {
                    if (!artifact->children.contains(child))
                        safeConnect(artifact, child, m_logger);
                }
            }
        }
    }
}

void BuildProject::markDirty()
{
    m_dirty = true;
}

void BuildProject::addBuildProduct(const BuildProductPtr &product)
{
    m_buildProducts.insert(product);
}

void BuildProject::setResolvedProject(const ResolvedProjectPtr &resolvedProject)
{
    m_resolvedProject = resolvedProject;
}

void BuildProject::removeArtifact(Artifact *artifact)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[BG] remove artifact " << relativeArtifactFileName(artifact);

    removeGeneratedArtifactFromDisk(artifact, m_logger);
    artifact->product->artifacts.remove(artifact);
    removeFromArtifactLookupTable(artifact);
    artifact->product->targetArtifacts.remove(artifact);
    foreach (Artifact *parent, artifact->parents) {
        parent->children.remove(artifact);
        if (parent->transformer) {
            parent->transformer->inputs.remove(artifact);
            m_artifactsThatMustGetNewTransformers += parent;
        }
    }
    foreach (Artifact *child, artifact->children)
        child->parents.remove(artifact);
    artifact->children.clear();
    artifact->parents.clear();
    markDirty();
}

void BuildProject::updateNodesThatMustGetNewTransformer()
{
    RulesEvaluationContext::Scope s(evaluationContext().data());
    foreach (Artifact *artifact, m_artifactsThatMustGetNewTransformers)
        updateNodeThatMustGetNewTransformer(artifact);
    m_artifactsThatMustGetNewTransformers.clear();
}

void BuildProject::updateNodeThatMustGetNewTransformer(Artifact *artifact)
{
    QBS_CHECK(artifact->transformer);

    if (m_logger.debugEnabled()) {
        m_logger.qbsDebug() << "[BG] updating transformer for "
                            << relativeArtifactFileName(artifact);
    }

    removeGeneratedArtifactFromDisk(artifact, m_logger);

    const RuleConstPtr rule = artifact->transformer->rule;
    markDirty();
    artifact->transformer = TransformerPtr();

    ArtifactsPerFileTagMap artifactsPerFileTag;
    foreach (Artifact *input, artifact->children) {
        foreach (const FileTag &fileTag, input->fileTags)
            artifactsPerFileTag[fileTag] += input;
    }
    RulesApplicator rulesApplier(artifact->product, artifactsPerFileTag, m_logger);
    rulesApplier.applyRule(rule);
}

void BuildProject::load(PersistentPool &pool)
{
    TimedActivityLogger logger(m_logger, QLatin1String("Loading project from disk."));
    m_resolvedProject = pool.idLoadS<ResolvedProject>();
    foreach (const ResolvedProductPtr &product, m_resolvedProject->products)
        product->project = m_resolvedProject;

    int count;
    pool.stream() >> count;
    for (; --count >= 0;) {
        BuildProductPtr product = pool.idLoadS<BuildProduct>();
        foreach (Artifact *artifact, product->artifacts) {
            artifact->project = this;
            insertIntoArtifactLookupTable(artifact);
        }
        addBuildProduct(product);
    }

    pool.stream() >> count;
    m_dependencyArtifacts.clear();
    m_dependencyArtifacts.reserve(count);
    for (; --count >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        artifact->project = this;
        insertFileDependency(artifact);
    }
}

void BuildProject::store(PersistentPool &pool) const
{
    pool.store(m_resolvedProject);
    pool.storeContainer(m_buildProducts);
    pool.storeContainer(m_dependencyArtifacts);
}


BuildProjectResolver::BuildProjectResolver(const Logger &logger) : m_logger(logger)
{
}

BuildProjectPtr BuildProjectResolver::resolveProject(const ResolvedProjectPtr &resolvedProject,
                                                     const RulesEvaluationContextPtr &evalContext)
{
    m_productCache.clear();
    m_project = BuildProjectPtr(new BuildProject(m_logger));
    m_project->setEvaluationContext(evalContext);
    m_project->setResolvedProject(resolvedProject);
    evalContext->initializeObserver(Tr::tr("Setting up build graph"),
                                    resolvedProject->products.count());
    foreach (ResolvedProductPtr rProduct, resolvedProject->products) {
        if (rProduct->enabled)
            resolveProduct(rProduct);
        evalContext->incrementProgressValue();
    }
    CycleDetector(m_logger).visitProject(m_project);
    return m_project;
}

static void addTargetArtifacts(const BuildProductPtr &product,
                              ArtifactsPerFileTagMap &artifactsPerFileTag, const Logger &logger)
{
    foreach (const FileTag &fileTag, product->rProduct->fileTags) {
        foreach (Artifact * const artifact, artifactsPerFileTag.value(fileTag)) {
            if (artifact->artifactType == Artifact::Generated)
                product->targetArtifacts += artifact;
        }
    }
    if (product->targetArtifacts.isEmpty()) {
        const QString msg = QString::fromLocal8Bit("No artifacts generated for product '%1'.");
        logger.qbsDebug() << msg.arg(product->rProduct->name);
    }
}

BuildProductPtr BuildProjectResolver::resolveProduct(const ResolvedProductPtr &rProduct)
{
    BuildProductPtr product = m_productCache.value(rProduct);
    if (product)
        return product;

    evalContext()->checkForCancelation();

    product = BuildProduct::create();
    m_productCache.insert(rProduct, product);
    product->project = m_project;
    product->rProduct = rProduct;
    ArtifactsPerFileTagMap artifactsPerFileTag;

    foreach (ResolvedProductPtr dependency, rProduct->dependencies) {
        if (dependency == rProduct)
            throw Error(Tr::tr("circular using"));
        if (!dependency->enabled) {
            QString msg = Tr::tr("Product '%1' depends on '%2' but '%2' is disabled.");
            throw Error(msg.arg(rProduct->name, dependency->name));
        }
        BuildProductPtr referencedProduct = resolveProduct(dependency);
        product->dependencies.append(referencedProduct);
    }

    //add qbsFile artifact
    Artifact *qbsFileArtifact = product->lookupArtifact(rProduct->location.fileName);
    if (!qbsFileArtifact) {
        qbsFileArtifact = new Artifact(m_project.data());
        qbsFileArtifact->artifactType = Artifact::SourceFile;
        qbsFileArtifact->setFilePath(rProduct->location.fileName);
        qbsFileArtifact->properties = rProduct->properties;
        product->insertArtifact(qbsFileArtifact, m_logger);
    }
    qbsFileArtifact->fileTags.insert("qbs");
    artifactsPerFileTag["qbs"].insert(qbsFileArtifact);

    // read sources
    foreach (const SourceArtifactConstPtr &sourceArtifact, rProduct->allEnabledFiles()) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (product->lookupArtifact(filePath))
            continue; // ignore duplicate artifacts

        Artifact *artifact = product->createArtifact(sourceArtifact, m_logger);
        foreach (const FileTag &fileTag, artifact->fileTags)
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    // read manually added transformers
    foreach (const ResolvedTransformer::Ptr rtrafo, rProduct->transformers) {
        ArtifactList inputArtifacts;
        foreach (const QString &inputFileName, rtrafo->inputs) {
            Artifact *artifact = product->lookupArtifact(inputFileName);
            if (!artifact)
                throw Error(QString("Can't find artifact '%0' in the list of source files.").arg(inputFileName));
            inputArtifacts += artifact;
        }
        TransformerPtr transformer = Transformer::create();
        transformer->inputs = inputArtifacts;
        const RulePtr rule = Rule::create();
        rule->jsImports = rtrafo->jsImports;
        ResolvedModulePtr module = ResolvedModule::create();
        module->name = rtrafo->module->name;
        rule->module = module;
        rule->script = rtrafo->transform;
        foreach (const SourceArtifactConstPtr &sourceArtifact, rtrafo->outputs) {
            Artifact *outputArtifact = product->createArtifact(sourceArtifact, m_logger);
            outputArtifact->artifactType = Artifact::Generated;
            outputArtifact->transformer = transformer;
            transformer->outputs += outputArtifact;
            product->targetArtifacts += outputArtifact;
            foreach (Artifact *inputArtifact, inputArtifacts)
                safeConnect(outputArtifact, inputArtifact, m_logger);
            foreach (const FileTag &fileTag, outputArtifact->fileTags)
                artifactsPerFileTag[fileTag].insert(outputArtifact);

            RuleArtifactPtr ruleArtifact = RuleArtifact::create();
            ruleArtifact->fileName = outputArtifact->filePath();
            ruleArtifact->fileTags = outputArtifact->fileTags;
            rule->artifacts += ruleArtifact;
        }
        transformer->rule = rule;

        RulesEvaluationContext::Scope s(evalContext().data());
        setupScriptEngineForProduct(engine(), rProduct, transformer->rule, scope());
        transformer->setupInputs(engine(), scope());
        transformer->setupOutputs(engine(), scope());
        transformer->createCommands(rtrafo->transform, evalContext());
        if (transformer->commands.isEmpty())
            throw Error(QString("There's a transformer without commands."), rtrafo->transform->location);
    }

    RulesApplicator(product.data(), artifactsPerFileTag, m_logger).applyAllRules();
    addTargetArtifacts(product, artifactsPerFileTag, m_logger);
    m_project->addBuildProduct(product);
    return product;
}

RulesEvaluationContextPtr BuildProjectResolver::evalContext() const
{
    return m_project->evaluationContext();
}

ScriptEngine *BuildProjectResolver::engine() const
{
    return evalContext()->engine();
}

QScriptValue BuildProjectResolver::scope() const
{
    return evalContext()->scope();
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

BuildProjectLoader::BuildProjectLoader(const Logger &logger) : m_logger(logger)
{
}

BuildProjectLoader::LoadResult BuildProjectLoader::load(const SetupProjectParameters &parameters,
        const RulesEvaluationContextPtr &evalContext)
{
    m_result = LoadResult();
    m_evalContext = evalContext;

    const QString projectId = ResolvedProject::deriveId(parameters.buildConfiguration);
    const QString buildDir = ResolvedProject::deriveBuildDirectory(parameters.buildRoot, projectId);
    const QString buildGraphFilePath = BuildProject::deriveBuildGraphFilePath(buildDir, projectId);

    PersistentPool pool(m_logger);
    m_logger.qbsDebug() << "[BG] trying to load: " << buildGraphFilePath;
    if (!pool.load(buildGraphFilePath))
        return m_result;
    if (!isConfigCompatible(parameters.buildConfiguration, pool.headData().projectConfig)) {
        m_logger.qbsDebug() << "[BG] Cannot use stored build graph: "
                               "Incompatible project configuration.";
        return m_result;
    }

    const BuildProjectPtr project = BuildProjectPtr(new BuildProject(m_logger));
    project->setEvaluationContext(evalContext);
    TimedActivityLogger loadLogger(m_logger, QLatin1String("Loading build graph"),
                                   QLatin1String("[BG] "));
    project->load(pool);
    if (QFileInfo(project->resolvedProject()->location.fileName)
            != QFileInfo(parameters.projectFilePath)) {
        QString errorMessage = Tr::tr("Stored build graph is for project file '%1', but "
                                      "input file is '%2'. ")
                .arg(QDir::toNativeSeparators(project->resolvedProject()->location.fileName),
                     QDir::toNativeSeparators(parameters.projectFilePath));
        if (!parameters.ignoreDifferentProjectFilePath) {
            errorMessage += Tr::tr("Aborting.");
            throw Error(errorMessage);
        }

        errorMessage += Tr::tr("Ignoring.");
        m_logger.qbsWarning() << errorMessage;

        // Okay, let's assume it's the same project anyway (the source dir might have moved).
        project->resolvedProject()->location.fileName = parameters.projectFilePath;
    }
    foreach (const BuildProductPtr &bp, project->buildProducts())
        bp->project = project;
    project->resolvedProject()->location = CodeLocation(parameters.projectFilePath, 1, 1);
    project->resolvedProject()->setBuildConfiguration(pool.headData().projectConfig);
    project->resolvedProject()->buildDirectory = buildDir;
    m_result.loadedProject = project;
    loadLogger.finishActivity();
    trackProjectChanges(parameters, evalContext, buildGraphFilePath, project);
    return m_result;
}

void BuildProjectLoader::trackProjectChanges(const SetupProjectParameters &parameters,
        const RulesEvaluationContextPtr &evalContext, const QString &buildGraphFilePath,
        const BuildProjectPtr &restoredProject)
{
    const FileInfo bgfi(buildGraphFilePath);
    const bool projectFileChanged
            = bgfi.lastModified() < FileInfo(parameters.projectFilePath).lastModified();

    bool referencedProductRemoved = false;
    QList<BuildProductPtr> changedProducts;
    foreach (BuildProductPtr product, restoredProject->buildProducts()) {
        const ResolvedProductConstPtr &resolvedProduct = product->rProduct;
        const FileInfo pfi(resolvedProduct->location.fileName);
        if (!pfi.exists()) {
            referencedProductRemoved = true;
        } else if (bgfi.lastModified() < pfi.lastModified()) {
            changedProducts += product;
        } else {
            foreach (const GroupPtr &group, resolvedProduct->groups) {
                if (!group->wildcards)
                    continue;
                const QSet<QString> files
                        = group->wildcards->expandPatterns(group, resolvedProduct->sourceDirectory);
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

    if (!projectFileChanged && !referencedProductRemoved && changedProducts.isEmpty())
        return;

    Loader ldr(evalContext->engine(), m_logger);
    ldr.setSearchPaths(parameters.searchPaths);
    const ResolvedProjectPtr freshProject = ldr.loadProject(parameters);
    m_result.changedResolvedProject = freshProject;

    QMap<QString, ResolvedProductPtr> freshProductsByName;
    if (!changedProducts.isEmpty()) {
        foreach (const ResolvedProductPtr &cp, freshProject->products)
            freshProductsByName.insert(cp->name, cp);
    }
    foreach (const BuildProductPtr &product, changedProducts) {
        ResolvedProductPtr freshProduct = freshProductsByName.value(product->rProduct->name);
        if (!freshProduct)
            continue;
        onProductChanged(product, freshProduct);
        if (m_result.discardLoadedProject)
            return;
    }

    QSet<QString> oldProductNames, newProductNames;
    foreach (const ResolvedProductConstPtr &product, restoredProject->resolvedProject()->products)
        oldProductNames += product->name;
    foreach (const ResolvedProductConstPtr &product, freshProject->products)
        newProductNames += product->name;
    const QSet<QString> addedProductNames = newProductNames - oldProductNames;
    if (!addedProductNames.isEmpty()) {
        // TODO: apply rules for the new products
        m_logger.qbsDebug() << "New products were added, discarding the loaded project.";
        m_result.discardLoadedProject = true;
        return;
    }
    const QSet<QString> removedProductsNames = oldProductNames - newProductNames;
    if (!removedProductsNames.isEmpty()) {
        foreach (const BuildProductPtr &product, restoredProject->buildProducts()) {
            if (removedProductsNames.contains(product->rProduct->name))
                onProductRemoved(product);
        }
    }

    CycleDetector(m_logger).visitProject(restoredProject);
}

void BuildProjectLoader::onProductRemoved(const BuildProductPtr &product)
{
    m_logger.qbsDebug() << "[BG] product '" << product->rProduct->name << "' removed.";

    product->project->markDirty();
    product->project->m_buildProducts.remove(product);

    // delete all removed artifacts physically from the disk
    foreach (Artifact *artifact, product->artifacts) {
        artifact->disconnectAll(m_logger);
        removeGeneratedArtifactFromDisk(artifact, m_logger);
    }
}

void BuildProjectLoader::onProductChanged(const BuildProductPtr &product,
                                          const ResolvedProductPtr &changedProduct)
{
    m_logger.qbsDebug() << "[BG] product '" << product->rProduct->name << "' changed.";

    ArtifactsPerFileTagMap artifactsPerFileTag;
    QSet<SourceArtifactConstPtr> addedSourceArtifacts;
    QList<Artifact *> addedArtifacts, artifactsToRemove;
    QHash<QString, SourceArtifactConstPtr> oldArtifacts, newArtifacts;

    const QList<SourceArtifactPtr> oldProductAllFiles = product->rProduct->allEnabledFiles();
    foreach (const SourceArtifactConstPtr &a, oldProductAllFiles)
        oldArtifacts.insert(a->absoluteFilePath, a);
    foreach (const SourceArtifactPtr &a, changedProduct->allEnabledFiles()) {
        newArtifacts.insert(a->absoluteFilePath, a);
        if (!oldArtifacts.contains(a->absoluteFilePath)) {
            // artifact added
            m_logger.qbsDebug() << "[BG] artifact '" << a->absoluteFilePath
                                << "' added to product " << product->rProduct->name;
            addedArtifacts += product->createArtifact(a, m_logger);
            addedSourceArtifacts += a;
        }
    }

    foreach (const SourceArtifactPtr &a, oldProductAllFiles) {
        const SourceArtifactConstPtr changedArtifact = newArtifacts.value(a->absoluteFilePath);
        if (!changedArtifact) {
            // artifact removed
            m_logger.qbsDebug() << "[BG] artifact '" << a->absoluteFilePath
                                << "' removed from product " << product->rProduct->name;
            Artifact *artifact = product->lookupArtifact(a->absoluteFilePath);
            QBS_CHECK(artifact);
            removeArtifactAndExclusiveDependents(artifact, &artifactsToRemove);
            continue;
        }
        if (!addedSourceArtifacts.contains(changedArtifact)
            && changedArtifact->properties->value() != a->properties->value())
        {
            m_logger.qbsInfo() << Tr::tr("Some properties changed. Regenerating build graph.");
            m_logger.qbsDebug() << "Artifact with changed properties: "
                                << changedArtifact->absoluteFilePath;
            m_result.discardLoadedProject = true;
            return;
        }
        if (changedArtifact->fileTags != a->fileTags) {
            // artifact's filetags have changed
            m_logger.qbsDebug() << "[BG] filetags have changed for artifact '"
                    << a->absoluteFilePath << "' from " << a->fileTags << " to "
                    << changedArtifact->fileTags;
            Artifact *artifact = product->lookupArtifact(a->absoluteFilePath);
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
    product->rProduct->groups = changedProduct->groups;
    product->rProduct->properties = changedProduct->properties;

    // apply rules for new artifacts
    foreach (Artifact *artifact, addedArtifacts)
        foreach (const FileTag &ft, artifact->fileTags)
            artifactsPerFileTag[ft] += artifact;
    RulesApplicator(product.data(), artifactsPerFileTag, m_logger).applyAllRules();

    addTargetArtifacts(product, artifactsPerFileTag, m_logger);

    // parents of removed artifacts must update their transformers
    foreach (Artifact *removedArtifact, artifactsToRemove)
        foreach (Artifact *parent, removedArtifact->parents)
            product->project->addToArtifactsThatMustGetNewTransformers(parent);
    product->project->updateNodesThatMustGetNewTransformer();

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
void BuildProjectLoader::removeArtifactAndExclusiveDependents(Artifact *artifact,
                                                              QList<Artifact*> *removedArtifacts)
{
    if (removedArtifacts)
        removedArtifacts->append(artifact);
    foreach (Artifact *parent, artifact->parents) {
        bool removeParent = false;
        disconnect(parent, artifact, m_logger);
        if (parent->children.isEmpty()) {
            removeParent = true;
        } else if (parent->transformer) {
            artifact->project->addToArtifactsThatMustGetNewTransformers(parent);
            parent->transformer->inputs.remove(artifact);
            removeParent = parent->transformer->inputs.isEmpty();
        }
        if (removeParent)
            removeArtifactAndExclusiveDependents(parent, removedArtifacts);
    }
    artifact->project->removeArtifact(artifact);
}

} // namespace Internal
} // namespace qbs
