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
#include "projectbuilddata.h"

#include "artifact.h"
#include "buildgraph.h"
#include "productbuilddata.h"
#include "command.h"
#include "cycledetector.h"
#include "rulesapplicator.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ProjectBuildData::ProjectBuildData() : isDirty(true)
{
}

ProjectBuildData::~ProjectBuildData()
{
    qDeleteAll(fileDependencies);
}

QString ProjectBuildData::deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId)
{
    return buildDir + QLatin1Char('/') + projectId + QLatin1String(".bg");
}

void ProjectBuildData::insertIntoLookupTable(FileResourceBase *fileres)
{
    QList<FileResourceBase *> &lst
            = m_artifactLookupTable[fileres->fileName()][fileres->dirPath()];
    if (!lst.contains(fileres))
        lst.append(fileres);
}

void ProjectBuildData::removeFromLookupTable(FileResourceBase *fileres)
{
    m_artifactLookupTable[fileres->fileName()][fileres->dirPath()].removeOne(fileres);
}

QList<FileResourceBase *> ProjectBuildData::lookupFiles(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupFiles(dirPath, fileName);
}

QList<FileResourceBase *> ProjectBuildData::lookupFiles(const QString &dirPath,
        const QString &fileName) const
{
    return m_artifactLookupTable.value(fileName).value(dirPath);
}

QList<FileResourceBase *> ProjectBuildData::lookupFiles(const Artifact *artifact) const
{
    return lookupFiles(artifact->dirPath(), artifact->fileName());
}

void ProjectBuildData::insertFileDependency(FileDependency *dependency)
{
    fileDependencies += dependency;
    insertIntoLookupTable(dependency);
}

static bool commandsEqual(const TransformerConstPtr &t1, const TransformerConstPtr &t2)
{
    if (t1->commands.count() != t2->commands.count())
        return false;
    for (int i = 0; i < t1->commands.count(); ++i)
        if (!t1->commands.at(i)->equals(t2->commands.at(i)))
            return false;
    return true;
}

static void disconnectArtifactChildren(Artifact *artifact, const Logger &logger)
{
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] disconnectChildren: '%1'")
                             .arg(relativeArtifactFileName(artifact));
    }
    foreach (Artifact * const child, artifact->children)
        child->parents.remove(artifact);
    artifact->children.clear();
}

static void disconnectArtifactParents(Artifact *artifact, ProjectBuildData *projectBuildData,
                               const Logger &logger)
{
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] disconnectParents: '%1'")
                             .arg(relativeArtifactFileName(artifact));
    }
    foreach (Artifact * const parent, artifact->parents) {
        parent->children.remove(artifact);
        if (parent->transformer) {
            parent->transformer->inputs.remove(artifact);
            projectBuildData->artifactsThatMustGetNewTransformers += parent;
        }
    }

    artifact->parents.clear();
}

static void disconnectArtifact(Artifact *artifact, ProjectBuildData *projectBuildData,
                               const Logger &logger)
{
    disconnectArtifactChildren(artifact, logger);
    disconnectArtifactParents(artifact, projectBuildData, logger);
}

void ProjectBuildData::removeArtifact(Artifact *artifact, ProjectBuildData *projectBuildData,
                                      const Logger &logger)
{
    if (logger.traceEnabled())
        logger.qbsTrace() << "[BG] remove artifact " << relativeArtifactFileName(artifact);

    removeGeneratedArtifactFromDisk(artifact, logger);
    artifact->product->buildData->artifacts.remove(artifact);
    removeFromLookupTable(artifact);
    artifact->product->buildData->targetArtifacts.remove(artifact);
    disconnectArtifact(artifact, projectBuildData, logger);
    projectBuildData->artifactsThatMustGetNewTransformers -= artifact;
    isDirty = true;
}

void ProjectBuildData::removeArtifact(Artifact *artifact, const Logger &logger)
{
    removeArtifact(artifact, artifact->product->topLevelProject()->buildData.data(), logger);
}

void ProjectBuildData::updateNodesThatMustGetNewTransformer(const Logger &logger)
{
    RulesEvaluationContext::Scope s(evaluationContext.data());
    foreach (Artifact *artifact, artifactsThatMustGetNewTransformers)
        updateNodeThatMustGetNewTransformer(artifact, logger);
    artifactsThatMustGetNewTransformers.clear();
}

void ProjectBuildData::updateNodeThatMustGetNewTransformer(Artifact *artifact, const Logger &logger)
{
    QBS_CHECK(artifact->transformer);

    if (logger.debugEnabled()) {
        logger.qbsDebug() << "[BG] updating transformer for "
                            << relativeArtifactFileName(artifact);
    }

    removeGeneratedArtifactFromDisk(artifact, logger);
    artifact->autoMocTimestamp.clear();
    artifact->clearTimestamp();

    const RuleConstPtr rule = artifact->transformer->rule;
    isDirty = true;
    artifact->transformer = TransformerPtr();

    ArtifactsPerFileTagMap artifactsPerFileTag;
    foreach (Artifact *input, artifact->children) {
        foreach (const FileTag &fileTag, input->fileTags)
            artifactsPerFileTag[fileTag] += input;
    }
    RulesApplicator rulesApplier(artifact->product, artifactsPerFileTag, logger);
    rulesApplier.applyRule(rule);
}

void ProjectBuildData::load(PersistentPool &pool)
{
    int count;
    pool.stream() >> count;
    fileDependencies.clear();
    fileDependencies.reserve(count);
    for (; --count >= 0;) {
        FileDependency *fileDependency = pool.idLoad<FileDependency>();
        insertFileDependency(fileDependency);
    }
}

void ProjectBuildData::store(PersistentPool &pool) const
{
    pool.storeContainer(fileDependencies);
}


BuildDataResolver::BuildDataResolver(const Logger &logger) : m_logger(logger)
{
}

void BuildDataResolver::resolveBuildData(const TopLevelProjectPtr &resolvedProject,
                                          const RulesEvaluationContextPtr &evalContext)
{
    QBS_CHECK(!resolvedProject->buildData);
    m_project = resolvedProject;
    resolvedProject->buildData.reset(new ProjectBuildData);
    resolvedProject->buildData->evaluationContext = evalContext;
    const QList<ResolvedProductPtr> allProducts = resolvedProject->allProducts();
    evalContext->initializeObserver(Tr::tr("Setting up build graph for configuration %1")
                                    .arg(resolvedProject->id()), allProducts.count() + 1);
    foreach (ResolvedProductPtr rProduct, allProducts) {
        if (rProduct->enabled)
            resolveProductBuildData(rProduct);
        evalContext->incrementProgressValue();
    }
    evalContext->incrementProgressValue();
    CycleDetector(m_logger).visitProject(m_project);
}

void BuildDataResolver::resolveProductBuildDataForExistingProject(const TopLevelProjectPtr &project,
        const QList<ResolvedProductPtr> &freshProducts)
{
    m_project = project;
    foreach (const ResolvedProductPtr &product, freshProducts)
        resolveProductBuildData(product);
}

/**
 * Rescues the following data from the source to target:
 *    - dependencies between artifacts,
 *    - time stamps of artifacts, if their commands have not changed.
 */
void BuildDataResolver::rescueBuildData(const TopLevelProjectConstPtr &source,
                                      const TopLevelProjectPtr &target, Logger logger)
{
    QHash<QString, ResolvedProductConstPtr> sourceProductsByName;
    foreach (const ResolvedProductConstPtr &product, source->allProducts())
        sourceProductsByName.insert(product->name, product);

    foreach (const ResolvedProductPtr &product, target->allProducts()) {
        ResolvedProductConstPtr sourceProduct = sourceProductsByName.value(product->name);
        if (!sourceProduct)
            continue;

        if (!product->enabled || !sourceProduct->enabled)
            continue;
        QBS_CHECK(product->buildData);

        if (logger.traceEnabled()) {
            logger.qbsTrace() << QString::fromLocal8Bit("[BG] rescue data of "
                    "product '%1'").arg(product->name);
        }

        foreach (Artifact *artifact, product->buildData->artifacts) {
            if (logger.traceEnabled()) {
                logger.qbsTrace() << QString::fromLocal8Bit("[BG]    artifact '%1'")
                                       .arg(artifact->fileName());
            }

            Artifact *otherArtifact = lookupArtifact(sourceProduct, artifact);
            if (!otherArtifact || !otherArtifact->transformer) {
                if (logger.traceEnabled())
                    logger.qbsTrace() << QString::fromLocal8Bit("[BG]    no transformer data");
                continue;
            }

            if (artifact->transformer
                    && !commandsEqual(artifact->transformer, otherArtifact->transformer)) {
                if (logger.traceEnabled())
                    logger.qbsTrace() << QString::fromLocal8Bit("[BG]    artifact invalidated");
                continue;
            }
            artifact->setTimestamp(otherArtifact->timestamp());

            foreach (Artifact *otherChild, otherArtifact->children) {
                // skip transform edges
                if (otherArtifact->transformer->inputs.contains(otherChild))
                    continue;

                foreach (FileResourceBase *childFileRes,
                         target->buildData->lookupFiles(otherChild)) {
                    Artifact *child = dynamic_cast<Artifact *>(childFileRes);
                    if (child && !artifact->children.contains(child))
                        safeConnect(artifact, child, logger);
                }
            }
        }
    }
}

void BuildDataResolver::resolveProductBuildData(const ResolvedProductPtr &product)
{
    if (product->buildData)
        return;

    evalContext()->checkForCancelation();

    product->buildData.reset(new ProductBuildData);
    ArtifactsPerFileTagMap artifactsPerFileTag;

    foreach (ResolvedProductPtr dependency, product->dependencies) {
        if (Q_UNLIKELY(!dependency->enabled)) {
            QString msg = Tr::tr("Product '%1' depends on '%2' but '%2' is disabled.");
            throw ErrorInfo(msg.arg(product->name, dependency->name));
        }
        resolveProductBuildData(dependency);
    }

    //add qbsFile artifact
    Artifact *qbsFileArtifact = lookupArtifact(product, product->location.fileName());
    if (!qbsFileArtifact) {
        qbsFileArtifact = new Artifact;
        qbsFileArtifact->artifactType = Artifact::SourceFile;
        qbsFileArtifact->setFilePath(product->location.fileName());
        qbsFileArtifact->properties = product->properties;
        insertArtifact(product, qbsFileArtifact, m_logger);
    }
    qbsFileArtifact->fileTags.insert("qbs");
    artifactsPerFileTag["qbs"].insert(qbsFileArtifact);

    // read sources
    foreach (const SourceArtifactConstPtr &sourceArtifact, product->allEnabledFiles()) {
        QString filePath = sourceArtifact->absoluteFilePath;
        if (lookupArtifact(product, filePath))
            continue; // ignore duplicate artifacts

        Artifact *artifact = createArtifact(product, sourceArtifact, m_logger);
        foreach (const FileTag &fileTag, artifact->fileTags)
            artifactsPerFileTag[fileTag].insert(artifact);
    }

    // read manually added transformers
    foreach (const ResolvedTransformer::Ptr rtrafo, product->transformers) {
        ArtifactList inputArtifacts;
        foreach (const QString &inputFileName, rtrafo->inputs) {
            Artifact *artifact = lookupArtifact(product, inputFileName);
            if (Q_UNLIKELY(!artifact))
                throw ErrorInfo(QString("Can't find artifact '%0' in the list of source files.").arg(inputFileName));
            inputArtifacts += artifact;
        }
        TransformerPtr transformer = Transformer::create();
        transformer->inputs = inputArtifacts;
        const RulePtr rule = Rule::create();
        rule->jsImports = rtrafo->jsImports;
        rule->jsExtensions = rtrafo->jsExtensions;
        ResolvedModulePtr module = ResolvedModule::create();
        module->name = rtrafo->module->name;
        rule->module = module;
        rule->script = rtrafo->transform;
        foreach (const SourceArtifactConstPtr &sourceArtifact, rtrafo->outputs) {
            Artifact *outputArtifact = createArtifact(product, sourceArtifact, m_logger);
            outputArtifact->artifactType = Artifact::Generated;
            outputArtifact->transformer = transformer;
            transformer->outputs += outputArtifact;
            product->buildData->targetArtifacts += outputArtifact;
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
        setupScriptEngineForProduct(engine(), product, transformer->rule, scope());
        transformer->setupInputs(engine(), scope());
        transformer->setupOutputs(engine(), scope());
        transformer->createCommands(rtrafo->transform, evalContext());
        if (Q_UNLIKELY(transformer->commands.isEmpty()))
            throw ErrorInfo(QString("There's a transformer without commands."), rtrafo->transform->location);
    }

    RulesApplicator(product, artifactsPerFileTag, m_logger).applyAllRules();
    addTargetArtifacts(product, artifactsPerFileTag, m_logger);
}

RulesEvaluationContextPtr BuildDataResolver::evalContext() const
{
    return m_project->buildData->evaluationContext;
}

ScriptEngine *BuildDataResolver::engine() const
{
    return evalContext()->engine();
}

QScriptValue BuildDataResolver::scope() const
{
    return evalContext()->scope();
}

} // namespace Internal
} // namespace qbs
