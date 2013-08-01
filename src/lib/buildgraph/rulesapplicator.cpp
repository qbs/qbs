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
#include "rulesapplicator.h"

#include "artifact.h"
#include "buildgraph.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>

#include <QDir>

namespace qbs {
namespace Internal {

RulesApplicator::RulesApplicator(const ResolvedProductPtr &product,
                                 ArtifactsPerFileTagMap &artifactsPerFileTag, const Logger &logger)
    : m_product(product)
    , m_artifactsPerFileTag(artifactsPerFileTag)
    , m_logger(logger)
    , m_productObjectId(-1)
{
}

void RulesApplicator::applyAllRules()
{
    RulesEvaluationContext::Scope s(m_product->topLevelProject()->buildData->evaluationContext.data());
    foreach (const RuleConstPtr &rule, m_product->topSortedRules())
        applyRule(rule);
}

void RulesApplicator::applyRule(const RuleConstPtr &rule)
{
    m_rule = rule;
    QScriptValue scopeValue = scope();
    setupScriptEngineForProduct(engine(), m_product, m_rule, scopeValue, this);
    Q_ASSERT_X(scope().property("product").strictlyEquals(engine()->evaluate("product")),
               "BG", "Product object is not in current scope.");
    m_productObjectId = scopeValue.property(QLatin1String("product")).objectId();

    ArtifactList inputArtifacts;
    foreach (const FileTag &fileTag, m_rule->inputs)
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

    if (m_logger.debugEnabled()) {
        m_logger.qbsDebug() << "[BG] apply rule " << m_rule->toString() << " "
                   << toStringList(inputArtifacts).join(",\n            ");
    }

    QList<QPair<const RuleArtifact *, Artifact *> > ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    ArtifactList usingArtifacts;
    if (!m_rule->usings.isEmpty()) {
        const FileTags usingsFileTags = m_rule->usings;
        foreach (const ResolvedProductPtr &dep, m_product->dependencies) {
            QBS_CHECK(dep->buildData);
            ArtifactList artifactsToCheck;
            foreach (Artifact *targetArtifact, dep->buildData->targetArtifacts)
                artifactsToCheck.unite(targetArtifact->transformer->outputs);
            foreach (Artifact *artifact, artifactsToCheck) {
                if (artifact->fileTags.matches(usingsFileTags))
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
        foreach (const FileTag &fileTag, outputArtifact->fileTags)
            m_artifactsPerFileTag[fileTag].insert(outputArtifact);

        // connect artifacts that match the file tags in explicitlyDependsOn
        foreach (const FileTag &fileTag, m_rule->explicitlyDependsOn)
            foreach (Artifact *dependency, m_artifactsPerFileTag.value(fileTag))
                loggedConnect(outputArtifact, dependency, m_logger);

        // Transformer setup
        for (ArtifactList::const_iterator it = usingArtifacts.constBegin();
             it != usingArtifacts.constEnd(); ++it)
        {
            Artifact *dep = *it;
            loggedConnect(outputArtifact, dep, m_logger);
            m_transformer->inputs.insert(dep);
        }
        m_transformer->outputs.insert(outputArtifact);

        m_product->topLevelProject()->buildData->artifactsThatMustGetNewTransformers
                -= outputArtifact;
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
        scope().setProperty("fileTags",
                            toScriptValue(engine(), outputArtifact->fileTags.toStringList()));

        QVariantMap artifactModulesCfg = outputArtifact->properties->value().value("modules").toMap();
        for (int i=0; i < ra->bindings.count(); ++i) {
            const RuleArtifact::Binding &binding = ra->bindings.at(i);
            scriptValue = engine()->evaluate(binding.code);
            if (Q_UNLIKELY(scriptValue.isError())) {
                QString msg = QLatin1String("evaluating rule binding '%1': %2");
                throw ErrorInfo(msg.arg(binding.name.join(QLatin1String(".")), scriptValue.toString()), binding.location);
            }
            setConfigProperty(artifactModulesCfg, binding.name, scriptValue.toVariant());
        }
        QVariantMap outputArtifactConfig = outputArtifact->properties->value();
        outputArtifactConfig.insert("modules", artifactModulesCfg);
        outputArtifact->properties->setValue(outputArtifactConfig);
    }

    m_transformer->setupOutputs(engine(), scope());
    m_transformer->createCommands(m_rule->script, evalContext());
    if (Q_UNLIKELY(m_transformer->commands.isEmpty()))
        throw ErrorInfo(QString("There's a rule without commands: %1.").arg(m_rule->toString()), m_rule->script->location);
}

void RulesApplicator::setupScriptEngineForArtifact(Artifact *artifact)
{
    QString inFileName = artifact->fileName();
    QString inBaseName = FileInfo::baseName(artifact->filePath());
    QString inCompleteBaseName = FileInfo::completeBaseName(artifact->filePath());

    QString basedir;
    if (artifact->artifactType == Artifact::SourceFile) {
        QDir sourceDir(m_product->sourceDirectory);
        basedir = FileInfo::path(sourceDir.relativeFilePath(artifact->filePath()));
    } else {
        QDir buildDir(m_product->topLevelProject()->buildDirectory);
        basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
    }

    // expose per file properties we want to use in an Artifact within a Rule
    QScriptValue scriptValue = engine()->newObject();
    ModuleProperties::init(scriptValue, artifact);
    scriptValue.setProperty("fileName", inFileName);
    scriptValue.setProperty("baseName", inBaseName);
    scriptValue.setProperty("completeBaseName", inCompleteBaseName);
    scriptValue.setProperty("baseDir", basedir);

    scope().setProperty("input", scriptValue);
    Q_ASSERT_X(scriptValue.strictlyEquals(engine()->evaluate("input")),
               "BG", "The input object is not in current scope.");
}

Artifact *RulesApplicator::createOutputArtifact(const RuleArtifactConstPtr &ruleArtifact,
        const ArtifactList &inputArtifacts)
{
    QScriptValue scriptValue = engine()->evaluate(ruleArtifact->fileName);
    if (Q_UNLIKELY(scriptValue.isError() || engine()->hasUncaughtException()))
        throw ErrorInfo("Error in Rule.Artifact fileName: " + scriptValue.toString());
    QString outputPath = scriptValue.toString();
    outputPath.replace("..", "dotdot");     // don't let the output artifact "escape" its build dir
    outputPath = resolveOutPath(outputPath);

    Artifact *outputArtifact = lookupArtifact(m_product, outputPath);
    if (outputArtifact) {
        if (outputArtifact->transformer && outputArtifact->transformer != m_transformer) {
            // This can happen when applying rules after scanning for additional file tags.
            // We just regenerate the transformer.
            if (m_logger.traceEnabled()) {
                m_logger.qbsTrace() << QString::fromLocal8Bit("[BG] regenerating transformer "
                        "for '%1'").arg(relativeArtifactFileName(outputArtifact));
            }
            m_transformer = outputArtifact->transformer;
            m_transformer->inputs.unite(inputArtifacts);

            if (Q_UNLIKELY(m_transformer->inputs.count() > 1 && !m_rule->multiplex)) {
                QString th = "[" + outputArtifact->fileTags.toStringList().join(", ") + "]";
                QString e = Tr::tr("Conflicting rules for producing %1 %2 \n").arg(outputArtifact->filePath(), th);
                th = "[" + m_rule->inputs.toStringList().join(", ")
                   + "] -> [" + outputArtifact->fileTags.toStringList().join(", ") + "]";

                e += QString("  while trying to apply:   %1:%2:%3  %4\n")
                    .arg(m_rule->script->location.fileName())
                    .arg(m_rule->script->location.line())
                    .arg(m_rule->script->location.column())
                    .arg(th);

                e += QString("  was already defined in:  %1:%2:%3  %4\n")
                    .arg(outputArtifact->transformer->rule->script->location.fileName())
                    .arg(outputArtifact->transformer->rule->script->location.line())
                    .arg(outputArtifact->transformer->rule->script->location.column())
                    .arg(th);
                throw ErrorInfo(e);
            }
        }
        outputArtifact->fileTags += ruleArtifact->fileTags;
    } else {
        outputArtifact = new Artifact;
        outputArtifact->artifactType = Artifact::Generated;
        outputArtifact->setFilePath(outputPath);
        outputArtifact->fileTags = ruleArtifact->fileTags;
        outputArtifact->alwaysUpdated = ruleArtifact->alwaysUpdated;
        outputArtifact->properties = m_product->properties;
        insertArtifact(m_product, outputArtifact, m_logger);
    }

    if (outputArtifact->fileTags.isEmpty())
        outputArtifact->fileTags = m_product->fileTagsForFileName(outputArtifact->fileName());

    for (int i = 0; i < m_product->artifactProperties.count(); ++i) {
        const ArtifactPropertiesConstPtr &props = m_product->artifactProperties.at(i);
        if (outputArtifact->fileTags.matches(props->fileTagsFilter())) {
            outputArtifact->properties = props->propertyMap();
            break;
        }
    }

    foreach (Artifact *inputArtifact, inputArtifacts) {
        QBS_CHECK(outputArtifact != inputArtifact);
        loggedConnect(outputArtifact, inputArtifact, m_logger);
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
    QString buildDir = m_product->topLevelProject()->buildDirectory;
    QString result = FileInfo::resolvePath(buildDir, path);
    result = QDir::cleanPath(result);
    return result;
}

RulesEvaluationContextPtr RulesApplicator::evalContext() const
{
    return m_product->topLevelProject()->buildData->evaluationContext;
}

ScriptEngine *RulesApplicator::engine() const
{
    return evalContext()->engine();
}

QScriptValue RulesApplicator::scope() const
{
    return evalContext()->scope();
}

void RulesApplicator::onPropertyRead(const QScriptValue &object, const QString &name,
                                     const QScriptValue &value)
{
    if (object.objectId() == m_productObjectId)
        engine()->addProperty(
                    Property(QString(), name, value.toVariant(), Property::PropertyInProduct));
}

} // namespace Internal
} // namespace qbs
