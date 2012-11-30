/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#include "rulesapplicator.h"

#include "artifact.h"
#include "buildgraph.h"
#include "buildproduct.h"
#include "buildproject.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/scripttools.h>

#include <QDir>

namespace qbs {
namespace Internal {

RulesApplicator::RulesApplicator(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag)
    : m_buildProduct(product)
    , m_artifactsPerFileTag(artifactsPerFileTag)
{
}

void RulesApplicator::applyAllRules()
{
    RulesEvaluationContext::Scope s(m_buildProduct->project->evaluationContext().data());
    foreach (const RuleConstPtr &rule, m_buildProduct->topSortedRules())
        applyRule(rule);
}

void RulesApplicator::applyRule(const RuleConstPtr &rule)
{
    m_rule = rule;
    BuildGraph::setupScriptEngineForProduct(engine(), m_buildProduct->rProduct, m_rule, scope());
    Q_ASSERT_X(scope().property("product").strictlyEquals(engine()->evaluate("product")),
               "BG", "Product object is not in current scope.");

    ArtifactList inputArtifacts;
    foreach (const QString &fileTag, m_rule->inputs)
        inputArtifacts.unite(m_artifactsPerFileTag.value(fileTag));
    if (m_rule->multiplex) { // apply the rule once for a set of inputs
        if (!inputArtifacts.isEmpty())
            doApply(inputArtifacts);
    } else { // apply the rule once for each input
        ArtifactList lst;
        foreach (Artifact * const inputArtifact, inputArtifacts) {
            setupScriptEngineForArtifact(inputArtifact);
            lst += inputArtifact;
            doApply(lst);
            lst.clear();
        }
    }
}

void RulesApplicator::doApply(const ArtifactList &inputArtifacts)
{
    evalContext()->checkForCancelation();

    if (qbsLogLevel(LoggerDebug))
        qbsDebug() << "[BG] apply rule " << m_rule->toString() << " "
                   << BuildGraph::toStringList(inputArtifacts).join(",\n            ");

    QList<QPair<const RuleArtifact *, Artifact *> > ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    ArtifactList usingArtifacts;
    if (!m_rule->usings.isEmpty()) {
        const QSet<QString> usingsFileTags = m_rule->usings.toSet();
        foreach (const BuildProductPtr &dep, m_buildProduct->dependencies) {
            ArtifactList artifactsToCheck;
            foreach (Artifact *targetArtifact, dep->targetArtifacts)
                artifactsToCheck.unite(targetArtifact->transformer->outputs);
            foreach (Artifact *artifact, artifactsToCheck) {
                QSet<QString> matchingFileTags = artifact->fileTags;
                matchingFileTags.intersect(usingsFileTags);
                if (!matchingFileTags.isEmpty())
                    usingArtifacts.insert(artifact);
            }
        }
    }

    m_transformer.clear();
    // create the output artifacts from the set of input artifacts
    foreach (const RuleArtifactConstPtr &ruleArtifact, m_rule->artifacts) {
        Artifact * const outputArtifact = createOutputArtifact(ruleArtifact, inputArtifacts);
        outputArtifacts << outputArtifact;
        ruleArtifactArtifactMap << qMakePair(ruleArtifact.data(), outputArtifact);
    }

    foreach (Artifact *outputArtifact, outputArtifacts) {
        // insert the output artifacts into the pool of artifacts
        foreach (const QString &fileTag, outputArtifact->fileTags)
            m_artifactsPerFileTag[fileTag].insert(outputArtifact);

        // connect artifacts that match the file tags in explicitlyDependsOn
        foreach (const QString &fileTag, m_rule->explicitlyDependsOn)
            foreach (Artifact *dependency, m_artifactsPerFileTag.value(fileTag))
                BuildGraph::loggedConnect(outputArtifact, dependency);

        // Transformer setup
        for (ArtifactList::const_iterator it = usingArtifacts.constBegin();
             it != usingArtifacts.constEnd(); ++it)
        {
            Artifact *dep = *it;
            BuildGraph::loggedConnect(outputArtifact, dep);
            m_transformer->inputs.insert(dep);
        }
        m_transformer->outputs.insert(outputArtifact);

        m_buildProduct->project->removeFromArtifactsThatMustGetNewTransformers(outputArtifact);
    }

    m_transformer->setupInputs(engine(), scope());

    // change the transformer outputs according to the bindings in Artifact
    QScriptValue scriptValue;
    for (int i = ruleArtifactArtifactMap.count(); --i >= 0;) {
        const RuleArtifact *ra = ruleArtifactArtifactMap.at(i).first;
        if (ra->bindings.isEmpty())
            continue;

        // expose attributes of this artifact
        Artifact *outputArtifact = ruleArtifactArtifactMap.at(i).second;
        outputArtifact->properties = outputArtifact->properties->clone();

        scope().setProperty("fileName", engine()->toScriptValue(outputArtifact->filePath()));
        scope().setProperty("fileTags", toScriptValue(engine(), outputArtifact->fileTags));

        QVariantMap artifactModulesCfg = outputArtifact->properties->value().value("modules").toMap();
        for (int i=0; i < ra->bindings.count(); ++i) {
            const RuleArtifact::Binding &binding = ra->bindings.at(i);
            scriptValue = engine()->evaluate(binding.code);
            if (scriptValue.isError()) {
                QString msg = QLatin1String("evaluating rule binding '%1': %2");
                throw Error(msg.arg(binding.name.join(QLatin1String(".")), scriptValue.toString()), binding.location);
            }
            setConfigProperty(artifactModulesCfg, binding.name, scriptValue.toVariant());
        }
        QVariantMap outputArtifactConfig = outputArtifact->properties->value();
        outputArtifactConfig.insert("modules", artifactModulesCfg);
        outputArtifact->properties->setValue(outputArtifactConfig);
    }

    m_transformer->setupOutputs(engine(), scope());
    m_transformer->createCommands(m_rule->script, evalContext());
    if (m_transformer->commands.isEmpty())
        throw Error(QString("There's a rule without commands: %1.").arg(m_rule->toString()), m_rule->script->location);
}

void RulesApplicator::setupScriptEngineForArtifact(Artifact *artifact)
{
    QString inFileName = artifact->fileName();
    QString inBaseName = FileInfo::baseName(artifact->filePath());
    QString inCompleteBaseName = FileInfo::completeBaseName(artifact->filePath());

    QString basedir;
    if (artifact->artifactType == Artifact::SourceFile) {
        QDir sourceDir(m_buildProduct->rProduct->sourceDirectory);
        basedir = FileInfo::path(sourceDir.relativeFilePath(artifact->filePath()));
    } else {
        QDir buildDir(m_buildProduct->project->resolvedProject()->buildDirectory);
        basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
    }

    QScriptValue modulesScriptValue = artifact->properties->toScriptValue(engine());
    modulesScriptValue = modulesScriptValue.property("modules");

    // expose per file properties we want to use in an Artifact within a Rule
    QScriptValue scriptValue = engine()->newObject();
    scriptValue.setProperty("fileName", inFileName);
    scriptValue.setProperty("baseName", inBaseName);
    scriptValue.setProperty("completeBaseName", inCompleteBaseName);
    scriptValue.setProperty("baseDir", basedir);
    scriptValue.setProperty("modules", modulesScriptValue);

    scope().setProperty("input", scriptValue);
    Q_ASSERT_X(scriptValue.strictlyEquals(engine()->evaluate("input")),
               "BG", "The input object is not in current scope.");
}

Artifact *RulesApplicator::createOutputArtifact(const RuleArtifactConstPtr &ruleArtifact,
        const ArtifactList &inputArtifacts)
{
    QScriptValue scriptValue = engine()->evaluate(ruleArtifact->fileName);
    if (scriptValue.isError() || engine()->hasUncaughtException())
        throw Error("Error in Rule.Artifact fileName: " + scriptValue.toString());
    QString outputPath = scriptValue.toString();
    outputPath.replace("..", "dotdot");     // don't let the output artifact "escape" its build dir
    outputPath = resolveOutPath(outputPath);

    Artifact *outputArtifact = m_buildProduct->lookupArtifact(outputPath);
    if (outputArtifact) {
        if (outputArtifact->transformer && outputArtifact->transformer != m_transformer) {
            // This can happen when applying rules after scanning for additional file tags.
            // We just regenerate the transformer.
            if (qbsLogLevel(LoggerTrace))
                qbsTrace("[BG] regenerating transformer for '%s'", qPrintable(BuildGraph::fileName(outputArtifact)));
            m_transformer = outputArtifact->transformer;
            m_transformer->inputs.unite(inputArtifacts);

            if (m_transformer->inputs.count() > 1 && !m_rule->multiplex) {
                QString th = "[" + QStringList(outputArtifact->fileTags.toList()).join(", ") + "]";
                QString e = Tr::tr("Conflicting rules for producing %1 %2 \n").arg(outputArtifact->filePath(), th);
                th = "[" + m_rule->inputs.join(", ")
                   + "] -> [" + QStringList(outputArtifact->fileTags.toList()).join(", ") + "]";

                e += QString("  while trying to apply:   %1:%2:%3  %4\n")
                    .arg(m_rule->script->location.fileName)
                    .arg(m_rule->script->location.line)
                    .arg(m_rule->script->location.column)
                    .arg(th);

                e += QString("  was already defined in:  %1:%2:%3  %4\n")
                    .arg(outputArtifact->transformer->rule->script->location.fileName)
                    .arg(outputArtifact->transformer->rule->script->location.line)
                    .arg(outputArtifact->transformer->rule->script->location.column)
                    .arg(th);
                throw Error(e);
            }
        }
        outputArtifact->fileTags += ruleArtifact->fileTags.toSet();
    } else {
        outputArtifact = new Artifact(m_buildProduct->project);
        outputArtifact->artifactType = Artifact::Generated;
        outputArtifact->setFilePath(outputPath);
        outputArtifact->fileTags = ruleArtifact->fileTags.toSet();
        outputArtifact->alwaysUpdated = ruleArtifact->alwaysUpdated;
        m_buildProduct->insertArtifact(outputArtifact);
    }

    if (outputArtifact->fileTags.isEmpty())
        outputArtifact->fileTags = m_buildProduct->rProduct->fileTagsForFileName(outputArtifact->fileName());

    if (m_rule->multiplex)
        outputArtifact->properties = m_buildProduct->rProduct->properties;
    else
        outputArtifact->properties= (*inputArtifacts.constBegin())->properties;

    foreach (Artifact *inputArtifact, inputArtifacts) {
        Q_ASSERT(outputArtifact != inputArtifact);
        BuildGraph::loggedConnect(outputArtifact, inputArtifact);
    }

    // create transformer if not already done so
    if (!m_transformer) {
        m_transformer = Transformer::create();
        m_transformer->rule = m_rule;
        m_transformer->inputs = inputArtifacts;
    }
    outputArtifact->transformer = m_transformer;

    return outputArtifact;
}

QString RulesApplicator::resolveOutPath(const QString &path) const
{
    QString buildDir = m_buildProduct->project->resolvedProject()->buildDirectory;
    QString result = FileInfo::resolvePath(buildDir, path);
    result = QDir::cleanPath(result);
    return result;
}

RulesEvaluationContextPtr RulesApplicator::evalContext() const
{
    return m_buildProduct->project->evaluationContext();
}

ScriptEngine *RulesApplicator::engine() const
{
    return evalContext()->engine();
}

QScriptValue RulesApplicator::scope() const
{
    return evalContext()->scope();
}

} // namespace Internal
} // namespace qbs
