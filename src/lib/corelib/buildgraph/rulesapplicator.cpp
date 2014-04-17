/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "qtmocscanner.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
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
    , m_mocScanner(0)
    , m_logger(logger)
{
}

RulesApplicator::~RulesApplicator()
{
    delete m_mocScanner;
}

NodeSet RulesApplicator::applyRuleInEvaluationContext(const RuleConstPtr &rule)
{
    m_createdArtifacts.clear();
    RulesEvaluationContext::Scope s(m_product->topLevelProject()->buildData->evaluationContext.data());
    applyRule(rule);
    return m_createdArtifacts;
}

void RulesApplicator::applyRule(const RuleConstPtr &rule)
{
    m_rule = rule;
    if (rule->name == QLatin1String("QtCoreMocRule")) {
        delete m_mocScanner;
        m_mocScanner = new QtMocScanner(m_product, scope(), m_logger);
    }
    QScriptValue prepareScriptContext = engine()->newObject();
    PrepareScriptObserver observer(engine());
    setupScriptEngineForFile(engine(), m_rule->prepareScript->fileContext, scope());
    setupScriptEngineForProduct(engine(), m_product, m_rule->module, prepareScriptContext, &observer);

    ArtifactSet inputArtifacts;
    foreach (const FileTag &fileTag, m_rule->inputs)
        inputArtifacts.unite(m_artifactsPerFileTag.value(fileTag));
    if (m_rule->multiplex) { // apply the rule once for a set of inputs
        if (!inputArtifacts.isEmpty())
            doApply(inputArtifacts, prepareScriptContext);
    } else { // apply the rule once for each input
        ArtifactSet lst;
        foreach (Artifact * const inputArtifact, inputArtifacts) {
            setupScriptEngineForArtifact(inputArtifact);
            lst += inputArtifact;
            doApply(lst, prepareScriptContext);
            lst.clear();
        }
    }
}

void RulesApplicator::handleRemovedRuleOutputs(ArtifactSet outputArtifactsToRemove,
        const Logger &logger)
{
    ArtifactSet artifactsToRemove;
    foreach (Artifact *removedArtifact, outputArtifactsToRemove) {
        if (logger.traceEnabled()) {
            logger.qbsTrace() << "[BG] dynamic rule removed output artifact "
                                << removedArtifact->toString();
        }
        removedArtifact->product->topLevelProject()
                ->buildData->removeArtifactAndExclusiveDependents(removedArtifact, logger, true,
                                                                  &artifactsToRemove);
    }
    // parents of removed artifacts must update their transformers
    foreach (Artifact *removedArtifact, artifactsToRemove) {
        foreach (Artifact *parent, removedArtifact->parentArtifacts())
            parent->product->registerArtifactWithChangedInputs(parent);
    }
    qDeleteAll(artifactsToRemove);
}

static void copyProperty(const QString &name, const QScriptValue &src, QScriptValue dst)
{
    dst.setProperty(name, src.property(name));
}

static QStringList toStringList(const ArtifactSet &artifacts)
{
    QStringList lst;
    foreach (const Artifact *artifact, artifacts) {
        const QString str = artifact->filePath() + QLatin1String(" [")
                + artifact->fileTags.toStringList().join(QLatin1String(", ")) + QLatin1Char(']');
        lst << str;
    }
    return lst;
}

void RulesApplicator::doApply(ArtifactSet inputArtifacts, QScriptValue &prepareScriptContext)
{
    evalContext()->checkForCancelation();

    if (m_logger.debugEnabled()) {
        m_logger.qbsDebug() << QString::fromLatin1("[BG] apply rule ") << m_rule->toString()
                            << QString::fromLatin1(" ")
                            << toStringList(inputArtifacts).join(QLatin1String(",\n            "));
    }

    QList<QPair<const RuleArtifact *, Artifact *> > ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    if (!m_rule->usings.isEmpty()) {
        const FileTags usingsFileTags = m_rule->usings;
        foreach (const ResolvedProductPtr &dep, m_product->dependencies) {
            QBS_CHECK(dep->buildData);
            ArtifactSet artifactsToCheck;
            foreach (Artifact *targetArtifact, dep->targetArtifacts())
                artifactsToCheck.unite(targetArtifact->transformer->outputs);
            foreach (Artifact *artifact, artifactsToCheck) {
                if (artifact->fileTags.matches(usingsFileTags))
                    inputArtifacts.insert(artifact);
            }
        }
    }

    m_transformer.clear();
    // create the output artifacts from the set of input artifacts
    Transformer::setupInputs(prepareScriptContext, inputArtifacts, m_rule->module->name);
    copyProperty(QLatin1String("inputs"), prepareScriptContext, scope());
    if (m_rule->multiplex) {
        // ### awful! Revisit how the "input" property is set up!
        copyProperty(QLatin1String("input"), prepareScriptContext, scope());
    }
    copyProperty(QLatin1String("product"), prepareScriptContext, scope());
    copyProperty(QLatin1String("project"), prepareScriptContext, scope());
    if (m_rule->isDynamic()) {
        outputArtifacts = runOutputArtifactsScript(inputArtifacts,
                    ScriptEngine::argumentList(m_rule->outputArtifactsScript->argumentNames,
                                               scope()));
        ArtifactSet newOutputs = ArtifactSet::fromNodeList(outputArtifacts);
        const ArtifactSet oldOutputs = collectOldOutputArtifacts(inputArtifacts);
        handleRemovedRuleOutputs(oldOutputs - newOutputs, m_logger);
    } else {
        foreach (const RuleArtifactConstPtr &ruleArtifact, m_rule->artifacts) {
            Artifact * const outputArtifact
                    = createOutputArtifactFromRuleArtifact(ruleArtifact, inputArtifacts);
            outputArtifacts << outputArtifact;
            ruleArtifactArtifactMap << qMakePair(ruleArtifact.data(), outputArtifact);
        }
    }

    if (outputArtifacts.isEmpty())
        return;

    foreach (Artifact *outputArtifact, outputArtifacts) {
        // insert the output artifacts into the pool of artifacts
        foreach (const FileTag &fileTag, outputArtifact->fileTags)
            m_artifactsPerFileTag[fileTag].insert(outputArtifact);

        // connect artifacts that match the file tags in explicitlyDependsOn
        foreach (const FileTag &fileTag, m_rule->explicitlyDependsOn)
            foreach (Artifact *dependency, m_artifactsPerFileTag.value(fileTag))
                loggedConnect(outputArtifact, dependency, m_logger);

        m_transformer->outputs.insert(outputArtifact);
        outputArtifact->product->unregisterArtifactWithChangedInputs(outputArtifact);
    }

    if (inputArtifacts != m_transformer->inputs)
        m_transformer->setupInputs(prepareScriptContext);

    // change the transformer outputs according to the bindings in Artifact
    QScriptValue scriptValue;
    if (!ruleArtifactArtifactMap.isEmpty())
        engine()->currentContext()->pushScope(prepareScriptContext);
    for (int i = ruleArtifactArtifactMap.count(); --i >= 0;) {
        const RuleArtifact *ra = ruleArtifactArtifactMap.at(i).first;
        if (ra->bindings.isEmpty())
            continue;

        // expose attributes of this artifact
        Artifact *outputArtifact = ruleArtifactArtifactMap.at(i).second;
        outputArtifact->properties = outputArtifact->properties->clone();

        scope().setProperty(QLatin1String("fileName"),
                            engine()->toScriptValue(outputArtifact->filePath()));
        scope().setProperty(QLatin1String("fileTags"),
                            toScriptValue(engine(), outputArtifact->fileTags.toStringList()));

        QVariantMap artifactModulesCfg = outputArtifact->properties->value()
                .value(QLatin1String("modules")).toMap();
        for (int i=0; i < ra->bindings.count(); ++i) {
            const RuleArtifact::Binding &binding = ra->bindings.at(i);
            scriptValue = engine()->evaluate(binding.code);
            if (Q_UNLIKELY(engine()->hasErrorOrException(scriptValue))) {
                QString msg = QLatin1String("evaluating rule binding '%1': %2");
                throw ErrorInfo(msg.arg(binding.name.join(QLatin1String(".")),
                                        scriptValue.toString()), binding.location);
            }
            setConfigProperty(artifactModulesCfg, binding.name, scriptValue.toVariant());
        }
        QVariantMap outputArtifactConfig = outputArtifact->properties->value();
        outputArtifactConfig.insert(QLatin1String("modules"), artifactModulesCfg);
        outputArtifact->properties->setValue(outputArtifactConfig);
    }
    if (!ruleArtifactArtifactMap.isEmpty())
        engine()->currentContext()->popScope();

    m_transformer->setupOutputs(engine(), prepareScriptContext);
    m_transformer->createCommands(m_rule->prepareScript, evalContext(),
            ScriptEngine::argumentList(m_rule->prepareScript->argumentNames, prepareScriptContext));
    if (Q_UNLIKELY(m_transformer->commands.isEmpty()))
        throw ErrorInfo(Tr::tr("There's a rule without commands: %1.")
                        .arg(m_rule->toString()), m_rule->prepareScript->location);
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
    scriptValue.setProperty(QLatin1String("fileName"), inFileName);
    scriptValue.setProperty(QLatin1String("filePath"), artifact->filePath());
    scriptValue.setProperty(QLatin1String("baseName"), inBaseName);
    scriptValue.setProperty(QLatin1String("completeBaseName"), inCompleteBaseName);
    scriptValue.setProperty(QLatin1String("baseDir"), basedir);
    scriptValue.setProperty(QLatin1String("fileTags"),
            engine()->toScriptValue(artifact->fileTags.toStringList()));
    attachPointerTo(scriptValue, artifact);

    scope().setProperty(QLatin1String("input"), scriptValue);
    Q_ASSERT_X(scriptValue.strictlyEquals(engine()->evaluate(QLatin1String("input"))),
               "BG", "The input object is not in current scope.");
}

ArtifactSet RulesApplicator::collectOldOutputArtifacts(const ArtifactSet &inputArtifacts) const
{
    ArtifactSet result;
    foreach (Artifact *a, inputArtifacts) {
        foreach (Artifact *p, a->parentArtifacts()) {
            QBS_CHECK(p->transformer);
            if (p->transformer->rule == m_rule && p->transformer->inputs.contains(a))
                result += p;
        }
    }
    return result;
}

Artifact *RulesApplicator::createOutputArtifactFromRuleArtifact(
        const RuleArtifactConstPtr &ruleArtifact, const ArtifactSet &inputArtifacts)
{
    QScriptValue scriptValue = engine()->evaluate(ruleArtifact->fileName);
    if (Q_UNLIKELY(engine()->hasErrorOrException(scriptValue)))
        throw ErrorInfo(Tr::tr("Error in Rule.Artifact fileName: ") + scriptValue.toString());
    QString outputPath = scriptValue.toString();
    return createOutputArtifact(outputPath, ruleArtifact->fileTags, ruleArtifact->alwaysUpdated,
                                inputArtifacts);
}

Artifact *RulesApplicator::createOutputArtifact(const QString &filePath, const FileTags &fileTags,
        bool alwaysUpdated, const ArtifactSet &inputArtifacts)
{
    QString outputPath = filePath;
    // don't let the output artifact "escape" its build dir
    outputPath.replace(QLatin1String(".."), QLatin1String("dotdot"));
    outputPath = resolveOutPath(outputPath);

    Artifact *outputArtifact = lookupArtifact(m_product, outputPath);
    if (outputArtifact) {
        if (outputArtifact->transformer && outputArtifact->transformer != m_transformer) {
            QBS_CHECK(!m_transformer);

            // This can happen when applying rules after scanning for additional file tags.
            // We just regenerate the transformer.
            if (m_logger.traceEnabled()) {
                m_logger.qbsTrace() << QString::fromLocal8Bit("[BG] regenerating transformer "
                        "for '%1'").arg(relativeArtifactFileName(outputArtifact));
            }
            m_transformer = outputArtifact->transformer;
            m_transformer->inputs.unite(inputArtifacts);

            if (Q_UNLIKELY(m_transformer->inputs.count() > 1 && !m_rule->multiplex)) {
                QString th = QLatin1Char('[') + outputArtifact->fileTags.toStringList()
                        .join(QLatin1String(", ")) + QLatin1Char(']');
                QString e = Tr::tr("Conflicting rules for producing %1 %2 \n")
                        .arg(outputArtifact->filePath(), th);
                th = QLatin1Char('[') + m_rule->inputs.toStringList().join(QLatin1String(", "))
                   + QLatin1String("] -> [") + outputArtifact->fileTags.toStringList()
                        .join(QLatin1String(", ")) + QLatin1Char(']');

                e += QString::fromLatin1("  while trying to apply:   %1:%2:%3  %4\n")
                    .arg(m_rule->prepareScript->location.fileName())
                    .arg(m_rule->prepareScript->location.line())
                    .arg(m_rule->prepareScript->location.column())
                    .arg(th);

                e += QString::fromLatin1("  was already defined in:  %1:%2:%3  %4\n")
                    .arg(outputArtifact->transformer->rule->prepareScript->location.fileName())
                    .arg(outputArtifact->transformer->rule->prepareScript->location.line())
                    .arg(outputArtifact->transformer->rule->prepareScript->location.column())
                    .arg(th);

                QStringList inputFilePaths;
                foreach (const Artifact * const a, m_transformer->inputs)
                    inputFilePaths << a->filePath();
                e.append(Tr::tr("The input artifacts are: %1")
                         .arg(inputFilePaths.join(QLatin1String(", "))));
                throw ErrorInfo(e);
            }
        }
        outputArtifact->fileTags += fileTags;
        outputArtifact->clearTimestamp();
    } else {
        outputArtifact = new Artifact;
        outputArtifact->artifactType = Artifact::Generated;
        outputArtifact->setFilePath(outputPath);
        outputArtifact->fileTags = fileTags;
        outputArtifact->alwaysUpdated = alwaysUpdated;
        outputArtifact->properties = m_product->moduleProperties;
        insertArtifact(m_product, outputArtifact, m_logger);
        m_createdArtifacts += outputArtifact;
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

QList<Artifact *> RulesApplicator::runOutputArtifactsScript(const ArtifactSet &inputArtifacts,
        const QScriptValueList &args)
{
    QList<Artifact *> lst;
    QScriptValue fun = engine()->evaluate(m_rule->outputArtifactsScript->sourceCode);
    if (!fun.isFunction())
        throw ErrorInfo(QLatin1String("Function expected."),
                        m_rule->outputArtifactsScript->location);
    QScriptValue res = fun.call(QScriptValue(), args);
    if (res.isError() || engine()->hasUncaughtException())
        throw ErrorInfo(Tr::tr("Error while calling Rule.outputArtifacts: %1").arg(res.toString()),
                        m_rule->outputArtifactsScript->location);
    if (!res.isArray())
        throw ErrorInfo(Tr::tr("Rule.outputArtifacts must return an array of objects."),
                        m_rule->outputArtifactsScript->location);
    const quint32 c = res.property(QLatin1String("length")).toUInt32();
    for (quint32 i = 0; i < c; ++i)
        lst += createOutputArtifactFromScriptValue(res.property(i), inputArtifacts);
    return lst;
}

Artifact *RulesApplicator::createOutputArtifactFromScriptValue(const QScriptValue &obj,
        const ArtifactSet &inputArtifacts)
{
    QBS_CHECK(obj.isObject());
    const QString filePath = obj.property(QLatin1String("filePath")).toVariant().toString();
    const FileTags fileTags = FileTags::fromStringList(
                obj.property(QLatin1String("fileTags")).toVariant().toStringList());
    const QVariant alwaysUpdatedVar = obj.property(QLatin1String("alwaysUpdated")).toVariant();
    const bool alwaysUpdated = alwaysUpdatedVar.isValid() ? alwaysUpdatedVar.toBool() : true;
    Artifact *output = createOutputArtifact(filePath, fileTags, alwaysUpdated, inputArtifacts);
    const FileTags explicitlyDependsOn = FileTags::fromStringList(
                obj.property(QLatin1String("explicitlyDependsOn")).toVariant().toStringList());
    foreach (const FileTag &tag, explicitlyDependsOn) {
        foreach (Artifact *dependency, m_product->lookupArtifactsByFileTag(tag)) {
            loggedConnect(output, dependency, m_logger);
        }
    }
    return output;
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

} // namespace Internal
} // namespace qbs
