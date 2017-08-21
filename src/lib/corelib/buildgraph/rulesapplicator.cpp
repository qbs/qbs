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
#include "rulesapplicator.h"

#include "buildgraph.h"
#include "emptydirectoriesremover.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "qtmocscanner.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/builtindeclarations.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qscopedpointer.h>

#include <QtScript/qscriptvalueiterator.h>

#include <vector>

namespace qbs {
namespace Internal {

RulesApplicator::RulesApplicator(const ResolvedProductPtr &product, const Logger &logger)
    : m_product(product)
    , m_mocScanner(0)
    , m_logger(logger)
{
}

RulesApplicator::~RulesApplicator()
{
    delete m_mocScanner;
}

void RulesApplicator::applyRule(const RuleConstPtr &rule, const ArtifactSet &inputArtifacts)
{
    if (inputArtifacts.isEmpty() && rule->declaresInputs() && rule->requiresInputs)
        return;

    m_createdArtifacts.clear();
    m_invalidatedArtifacts.clear();
    RulesEvaluationContext::Scope s(evalContext().get());

    m_rule = rule;
    m_completeInputSet = inputArtifacts;
    if (rule->name == QLatin1String("QtCoreMocRule")) {
        delete m_mocScanner;
        m_mocScanner = new QtMocScanner(m_product, scope(), m_logger);
    }
    QScriptValue prepareScriptContext = engine()->newObject();
    prepareScriptContext.setPrototype(engine()->globalObject());
    PrepareScriptObserver observer(engine());
    setupScriptEngineForFile(engine(), m_rule->prepareScript->fileContext, scope());
    setupScriptEngineForProduct(engine(), m_product, m_rule->module, prepareScriptContext, &observer);

    if (m_rule->multiplex) { // apply the rule once for a set of inputs
        doApply(inputArtifacts, prepareScriptContext);
    } else { // apply the rule once for each input
        for (Artifact * const inputArtifact : inputArtifacts) {
            ArtifactSet lst;
            lst += inputArtifact;
            doApply(lst, prepareScriptContext);
        }
    }
}

void RulesApplicator::handleRemovedRuleOutputs(const ArtifactSet &inputArtifacts,
        const ArtifactSet &outputArtifactsToRemove, const Logger &logger)
{
    ArtifactSet artifactsToRemove;
    const TopLevelProject *project = 0;
    for (Artifact * const removedArtifact : outputArtifactsToRemove) {
        if (logger.traceEnabled()) {
            logger.qbsTrace() << "[BG] dynamic rule removed output artifact "
                                << removedArtifact->toString();
        }
        if (!project)
            project = removedArtifact->product->topLevelProject();
        project->buildData->removeArtifactAndExclusiveDependents(removedArtifact, logger, true,
                                                                 &artifactsToRemove);
    }
    // parents of removed artifacts must update their transformers
    for (Artifact *removedArtifact : qAsConst(artifactsToRemove)) {
        for (Artifact *parent : removedArtifact->parentArtifacts())
            parent->product->registerArtifactWithChangedInputs(parent);
    }
    EmptyDirectoriesRemover(project, logger).removeEmptyParentDirectories(artifactsToRemove);
    for (Artifact * const artifact : qAsConst(artifactsToRemove)) {
        QBS_CHECK(!inputArtifacts.contains(artifact));
        delete artifact;
    }
}

static void copyProperty(const QString &name, const QScriptValue &src, QScriptValue dst)
{
    dst.setProperty(name, src.property(name));
}

static QStringList toStringList(const ArtifactSet &artifacts)
{
    QStringList lst;
    for (const Artifact * const artifact : artifacts) {
        const QString str = artifact->filePath() + QLatin1String(" [")
                + artifact->fileTags().toStringList().join(QLatin1String(", ")) + QLatin1Char(']');
        lst << str;
    }
    return lst;
}

void RulesApplicator::doApply(const ArtifactSet &inputArtifacts, QScriptValue &prepareScriptContext)
{
    evalContext()->checkForCancelation();

    if (m_logger.debugEnabled()) {
        m_logger.qbsDebug() << QString::fromLatin1("[BG] apply rule ") << m_rule->toString()
                            << QString::fromLatin1(" ")
                            << toStringList(inputArtifacts).join(QLatin1String(",\n            "));
    }

    QList<std::pair<const RuleArtifact *, Artifact *> > ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    m_transformer = Transformer::create();
    m_transformer->rule = m_rule;
    m_transformer->inputs = inputArtifacts;
    m_transformer->explicitlyDependsOn = collectExplicitlyDependsOn();
    m_transformer->alwaysRun = m_rule->alwaysRun;

    // create the output artifacts from the set of input artifacts
    m_transformer->setupInputs(prepareScriptContext);
    m_transformer->setupExplicitlyDependsOn(prepareScriptContext);
    copyProperty(QLatin1String("inputs"), prepareScriptContext, scope());
    copyProperty(QLatin1String("input"), prepareScriptContext, scope());
    copyProperty(QLatin1String("explicitlyDependsOn"), prepareScriptContext, scope());
    copyProperty(QLatin1String("product"), prepareScriptContext, scope());
    copyProperty(QLatin1String("project"), prepareScriptContext, scope());
    if (m_rule->isDynamic()) {
        outputArtifacts = runOutputArtifactsScript(inputArtifacts,
                    ScriptEngine::argumentList(m_rule->outputArtifactsScript->argumentNames,
                                               scope()));
        ArtifactSet newOutputs = ArtifactSet::fromList(outputArtifacts);
        const ArtifactSet oldOutputs = collectOldOutputArtifacts(inputArtifacts);
        handleRemovedRuleOutputs(m_completeInputSet, oldOutputs - newOutputs, m_logger);
    } else {
        Set<QString> outputFilePaths;
        for (const RuleArtifactConstPtr &ruleArtifact : qAsConst(m_rule->artifacts)) {
            Artifact * const outputArtifact
                    = createOutputArtifactFromRuleArtifact(ruleArtifact, inputArtifacts,
                                                           &outputFilePaths);
            if (!outputArtifact)
                continue;
            outputArtifacts << outputArtifact;
            ruleArtifactArtifactMap << std::make_pair(ruleArtifact.get(), outputArtifact);
        }
    }

    if (outputArtifacts.isEmpty())
        return;

    for (Artifact * const outputArtifact : qAsConst(outputArtifacts)) {
        for (Artifact * const dependency : qAsConst(m_transformer->explicitlyDependsOn))
            loggedConnect(outputArtifact, dependency, m_logger);
        outputArtifact->product->unregisterArtifactWithChangedInputs(outputArtifact);
    }

    if (inputArtifacts != m_transformer->inputs)
        m_transformer->setupInputs(prepareScriptContext);

    // change the transformer outputs according to the bindings in Artifact
    QScriptValue scriptValue;
    if (!ruleArtifactArtifactMap.isEmpty())
        engine()->setGlobalObject(prepareScriptContext);
    for (int i = ruleArtifactArtifactMap.count(); --i >= 0;) {
        const RuleArtifact *ra = ruleArtifactArtifactMap.at(i).first;
        if (ra->bindings.empty())
            continue;

        // expose attributes of this artifact
        Artifact *outputArtifact = ruleArtifactArtifactMap.at(i).second;
        outputArtifact->properties = outputArtifact->properties->clone();

        scope().setProperty(QLatin1String("fileName"),
                            engine()->toScriptValue(outputArtifact->filePath()));
        scope().setProperty(QLatin1String("fileTags"),
                            toScriptValue(engine(), outputArtifact->fileTags().toStringList()));

        QVariantMap artifactModulesCfg = outputArtifact->properties->value()
                .value(QLatin1String("modules")).toMap();
        for (const auto &binding : ra->bindings) {
            scriptValue = engine()->evaluate(binding.code);
            if (Q_UNLIKELY(engine()->hasErrorOrException(scriptValue))) {
                QString msg = QLatin1String("evaluating rule binding '%1': %2");
                throw ErrorInfo(msg.arg(binding.name.join(QLatin1Char('.')),
                                        engine()->lastErrorString(scriptValue)),
                                engine()->lastErrorLocation(scriptValue, binding.location));
            }
            setConfigProperty(artifactModulesCfg, binding.name, scriptValue.toVariant());
        }
        QVariantMap outputArtifactConfig = outputArtifact->properties->value();
        outputArtifactConfig.insert(QLatin1String("modules"), artifactModulesCfg);
        outputArtifact->properties->setValue(outputArtifactConfig);
    }
    if (!ruleArtifactArtifactMap.isEmpty())
        engine()->setGlobalObject(prepareScriptContext.prototype());

    m_transformer->setupOutputs(prepareScriptContext);
    m_transformer->createCommands(engine(), m_rule->prepareScript,
            ScriptEngine::argumentList(m_rule->prepareScript->argumentNames, prepareScriptContext));
    if (Q_UNLIKELY(m_transformer->commands.isEmpty()))
        throw ErrorInfo(Tr::tr("There is a rule without commands: %1.")
                        .arg(m_rule->toString()), m_rule->prepareScript->location);
}

ArtifactSet RulesApplicator::collectOldOutputArtifacts(const ArtifactSet &inputArtifacts) const
{
    ArtifactSet result;
    for (Artifact * const a : inputArtifacts) {
        for (Artifact *p : a->parentArtifacts()) {
            QBS_CHECK(p->transformer);
            if (p->transformer->rule == m_rule && p->transformer->inputs.contains(a))
                result += p;
        }
    }
    return result;
}

ArtifactSet RulesApplicator::collectExplicitlyDependsOn()
{
    ArtifactSet artifacts;
    for (const FileTag &fileTag : qAsConst(m_rule->explicitlyDependsOn)) {
        for (Artifact *dependency : m_product->lookupArtifactsByFileTag(fileTag))
            artifacts << dependency;
        for (const ResolvedProductConstPtr &depProduct : qAsConst(m_product->dependencies)) {
            for (Artifact * const ta : depProduct->targetArtifacts()) {
                if (ta->fileTags().contains(fileTag))
                    artifacts << ta;
            }
        }
    }
    return artifacts;
}

Artifact *RulesApplicator::createOutputArtifactFromRuleArtifact(
        const RuleArtifactConstPtr &ruleArtifact, const ArtifactSet &inputArtifacts,
        Set<QString> *outputFilePaths)
{
    QScriptValue scriptValue = engine()->evaluate(ruleArtifact->filePath,
                                                  ruleArtifact->filePathLocation.filePath(),
                                                  ruleArtifact->filePathLocation.line());
    if (Q_UNLIKELY(engine()->hasErrorOrException(scriptValue)))
        throw engine()->lastError(scriptValue, ruleArtifact->filePathLocation);
    QString outputPath = FileInfo::resolvePath(m_product->buildDirectory(), scriptValue.toString());
    if (Q_UNLIKELY(!outputFilePaths->insert(outputPath).second)) {
        throw ErrorInfo(Tr::tr("Rule %1 already created '%2'.")
                        .arg(m_rule->toString(), outputPath));
    }
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
        const Transformer * const transformer = outputArtifact->transformer.get();
        if (transformer && transformer->rule != m_rule) {
            QString e = Tr::tr("Conflicting rules for producing %1 %2 \n")
                    .arg(outputArtifact->filePath(),
                         QLatin1Char('[') +
                         outputArtifact->fileTags().toStringList().join(QLatin1String(", "))
                         + QLatin1Char(']'));
            QString str = QLatin1Char('[') + m_rule->inputs.toStringList().join(QLatin1String(", "))
               + QLatin1String("] -> [") + outputArtifact->fileTags().toStringList()
                    .join(QLatin1String(", ")) + QLatin1Char(']');

            e += QString::fromLatin1("  while trying to apply:   %1:%2:%3  %4\n")
                .arg(m_rule->prepareScript->location.filePath())
                .arg(m_rule->prepareScript->location.line())
                .arg(m_rule->prepareScript->location.column())
                .arg(str);

            e += QString::fromLatin1("  was already defined in:  %1:%2:%3  %4\n")
                .arg(transformer->rule->prepareScript->location.filePath())
                .arg(transformer->rule->prepareScript->location.line())
                .arg(transformer->rule->prepareScript->location.column())
                .arg(str);

            throw ErrorInfo(e);
        }
        if (transformer && !m_rule->multiplex && transformer->inputs != inputArtifacts) {
            QBS_CHECK(inputArtifacts.size() == 1);
            QBS_CHECK(transformer->inputs.size() == 1);
            ErrorInfo error(Tr::tr("Conflicting instances of rule '%1':").arg(m_rule->toString()),
                            m_rule->prepareScript->location);
            error.append(Tr::tr("Output artifact '%1' is to be produced from input "
                                "artifacts '%2' and '%3', but the rule is not a multiplex rule.")
                         .arg(outputArtifact->filePath(),
                              (*transformer->inputs.cbegin())->filePath(),
                              (*inputArtifacts.cbegin())->filePath()));
            throw error;
        }
        if (m_rule->declaresInputs() && m_rule->requiresInputs)
            outputArtifact->clearTimestamp();
        m_invalidatedArtifacts += outputArtifact;
        m_transformer->rescueChangeTrackingData(outputArtifact->transformer);
    } else {
        QScopedPointer<Artifact> newArtifact(new Artifact);
        newArtifact->artifactType = Artifact::Generated;
        newArtifact->setFilePath(outputPath);
        insertArtifact(m_product, newArtifact.data(), m_logger);
        m_createdArtifacts += newArtifact.data();
        outputArtifact = newArtifact.take();
    }

    outputArtifact->setFileTags(
                fileTags.isEmpty() ? m_product->fileTagsForFileName(outputArtifact->fileName())
                                   : fileTags);
    outputArtifact->alwaysUpdated = alwaysUpdated;
    outputArtifact->properties = m_product->moduleProperties;

    for (int i = 0; i < m_product->artifactProperties.count(); ++i) {
        const ArtifactPropertiesConstPtr &props = m_product->artifactProperties.at(i);
        if (outputArtifact->fileTags().intersects(props->fileTagsFilter())) {
            outputArtifact->properties = props->propertyMap();
            break;
        }
    }

    // Let a positive value of qbs.install imply the file tag "installable".
    if (outputArtifact->properties->qbsPropertyValue(QLatin1String("install")).toBool())
        outputArtifact->addFileTag("installable");

    for (Artifact * const inputArtifact : inputArtifacts) {
        QBS_CHECK(outputArtifact != inputArtifact);
        loggedConnect(outputArtifact, inputArtifact, m_logger);
    }

    outputArtifact->transformer = m_transformer;
    m_transformer->outputs.insert(outputArtifact);
    QBS_CHECK(m_rule->multiplex || m_transformer->inputs.count() == 1);

    return outputArtifact;
}

QList<Artifact *> RulesApplicator::runOutputArtifactsScript(const ArtifactSet &inputArtifacts,
        const QScriptValueList &args)
{
    QList<Artifact *> lst;
    QScriptValue fun = engine()->evaluate(m_rule->outputArtifactsScript->sourceCode,
                                          m_rule->outputArtifactsScript->location.filePath(),
                                          m_rule->outputArtifactsScript->location.line());
    if (!fun.isFunction())
        throw ErrorInfo(QLatin1String("Function expected."),
                        m_rule->outputArtifactsScript->location);
    QScriptValue res = fun.call(QScriptValue(), args);
    if (engine()->hasErrorOrException(res))
        throw engine()->lastError(res, m_rule->outputArtifactsScript->location);
    if (!res.isArray())
        throw ErrorInfo(Tr::tr("Rule.outputArtifacts must return an array of objects."),
                        m_rule->outputArtifactsScript->location);
    const quint32 c = res.property(QLatin1String("length")).toUInt32();
    for (quint32 i = 0; i < c; ++i)
        lst += createOutputArtifactFromScriptValue(res.property(i), inputArtifacts);
    return lst;
}

class ArtifactBindingsExtractor
{
    struct Entry
    {
        Entry(const QString &module, const QString &name, const QVariant &value)
            : module(module), name(name), value(value)
        {}

        QString module;
        QString name;
        QVariant value;
    };
    std::vector<Entry> m_propertyValues;

    static Set<QString> getArtifactItemPropertyNames()
    {
        Set<QString> s;
        for (const PropertyDeclaration &pd :
                 BuiltinDeclarations::instance().declarationsForType(
                     ItemType::Artifact).properties()) {
            s.insert(pd.name());
        }
        s.insert(QLatin1String("explicitlyDependsOn"));
        return s;
    }

    void extractPropertyValues(const QScriptValue &obj, const QString &moduleName = QString())
    {
        QScriptValueIterator svit(obj);
        while (svit.hasNext()) {
            svit.next();
            const QString name = svit.name();
            if (moduleName.isEmpty()) {
                // Ignore property names that are part of the Artifact item.
                static const Set<QString> artifactItemPropertyNames
                        = getArtifactItemPropertyNames();
                if (artifactItemPropertyNames.contains(name))
                    continue;
            }

            const QScriptValue value = svit.value();
            if (value.isObject() && !value.isArray() && !value.isError() && !value.isRegExp()) {
                QString newModuleName;
                if (!moduleName.isEmpty())
                    newModuleName.append(moduleName + QLatin1Char('.'));
                newModuleName.append(name);
                extractPropertyValues(value, newModuleName);
            } else {
                m_propertyValues.emplace_back(moduleName, name, value.toVariant());
            }
        }
    }
public:
    void apply(Artifact *outputArtifact, const QScriptValue &obj)
    {
        extractPropertyValues(obj);
        if (m_propertyValues.empty())
            return;

        outputArtifact->properties = outputArtifact->properties->clone();
        QVariantMap artifactCfg = outputArtifact->properties->value();
        for (const auto &e : m_propertyValues)
            setConfigProperty(artifactCfg, {QStringLiteral("modules"), e.module, e.name}, e.value);
        outputArtifact->properties->setValue(artifactCfg);
    }
};

Artifact *RulesApplicator::createOutputArtifactFromScriptValue(const QScriptValue &obj,
        const ArtifactSet &inputArtifacts)
{
    if (!obj.isObject()) {
        throw ErrorInfo(Tr::tr("Elements of the Rule.outputArtifacts array must be "
                               "of Object type."), m_rule->outputArtifactsScript->location);
    }
    const QString filePath = FileInfo::resolvePath(m_product->buildDirectory(),
            obj.property(QLatin1String("filePath")).toVariant().toString());
    const FileTags fileTags = FileTags::fromStringList(
                obj.property(QLatin1String("fileTags")).toVariant().toStringList());
    const QVariant alwaysUpdatedVar = obj.property(QLatin1String("alwaysUpdated")).toVariant();
    const bool alwaysUpdated = alwaysUpdatedVar.isValid() ? alwaysUpdatedVar.toBool() : true;
    Artifact *output = createOutputArtifact(filePath, fileTags, alwaysUpdated, inputArtifacts);
    const FileTags explicitlyDependsOn = FileTags::fromStringList(
                obj.property(QLatin1String("explicitlyDependsOn")).toVariant().toStringList());
    for (const FileTag &tag : explicitlyDependsOn) {
        for (Artifact * const dependency : m_product->lookupArtifactsByFileTag(tag))
            loggedConnect(output, dependency, m_logger);
    }
    ArtifactBindingsExtractor().apply(output, obj);
    return output;
}

QString RulesApplicator::resolveOutPath(const QString &path) const
{
    QString buildDir = m_product->topLevelProject()->buildDirectory;
    QString result = FileInfo::resolvePath(buildDir, path);
    result = QDir::cleanPath(result);
    return result;
}

const RulesEvaluationContextPtr &RulesApplicator::evalContext() const
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
