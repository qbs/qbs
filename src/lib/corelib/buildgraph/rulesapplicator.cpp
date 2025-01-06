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
#include "cppmodulesscanner.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "qtmocscanner.h"
#include "rulecommands.h"
#include "rulenode.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"
#include "transformerchangetracking.h"

#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/builtindeclarations.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qcryptographichash.h>
#include <QtCore/qdir.h>

#include <memory>
#include <vector>

namespace qbs {
namespace Internal {

RulesApplicator::RulesApplicator(
    ResolvedProductPtr product,
    const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
    const std::unordered_map<QString, const ResolvedProject *> &projectsByName,
    Logger logger)
    : m_product(std::move(product))
    // m_productsByName and m_projectsByName are references, cannot move-construct
    , m_productsByName(productsByName)
    , m_projectsByName(projectsByName)
    , m_logger(std::move(logger))
{}

RulesApplicator::~RulesApplicator()
{
    delete m_mocScanner;
    delete m_cxxModulesScanner;
}

void RulesApplicator::applyRule(RuleNode *ruleNode, const ArtifactSet &inputArtifacts,
                                const ArtifactSet &explicitlyDependsOn)
{
    m_ruleNode = ruleNode;
    m_rule = ruleNode->rule();
    QBS_CHECK(!inputArtifacts.empty() || !m_rule->declaresInputs() || !m_rule->requiresInputs);

    m_product->topLevelProject()->buildData->setDirty();
    m_createdArtifacts.clear();
    m_invalidatedArtifacts.clear();
    m_removedArtifacts.clear();
    m_explicitlyDependsOn = explicitlyDependsOn;
    RulesEvaluationContext::Scope s(evalContext().get());

    m_completeInputSet = inputArtifacts;
    if (m_rule->name.startsWith(QLatin1String("QtCoreMocRule"))) {
        delete m_mocScanner;
        m_mocScanner = new QtMocScanner(m_product, engine(), scope());
    }
    if (m_rule->name.startsWith(QLatin1String("cpp_compiler"))) {
        delete m_cxxModulesScanner;
        m_cxxModulesScanner = new CppModulesScanner(engine(), scope());
    }
    ScopedJsValue prepareScriptContext(jsContext(), engine()->newObject());
    JS_SetPrototype(jsContext(), prepareScriptContext, engine()->globalObject());
    setupScriptEngineForFile(engine(), m_rule->prepareScript.fileContext(), scope(),
                             ObserveMode::Enabled);
    setupScriptEngineForProduct(engine(), m_product.get(), m_rule->module.get(),
                                prepareScriptContext, true);

    engine()->clearUsesIo();
    if (m_rule->multiplex) { // apply the rule once for a set of inputs
        doApply(inputArtifacts, prepareScriptContext);
    } else { // apply the rule once for each input
        for (Artifact * const inputArtifact : inputArtifacts) {
            ArtifactSet lst;
            lst += inputArtifact;
            doApply(lst, prepareScriptContext);
        }
    }
    if (engine()->usesIo())
        m_ruleUsesIo = true;
    engine()->releaseInputArtifactScriptValues(ruleNode);
}

void RulesApplicator::handleRemovedRuleOutputs(const ArtifactSet &inputArtifacts,
        const ArtifactSet &outputArtifactsToRemove, QStringList &removedArtifacts,
        const Logger &logger)
{
    ArtifactSet artifactsToRemove;
    const TopLevelProject *project = nullptr;
    for (Artifact * const removedArtifact : outputArtifactsToRemove) {
        qCDebug(lcBuildGraph).noquote() << "dynamic rule removed output artifact"
                                        << removedArtifact->toString();
        if (!project)
            project = removedArtifact->product->topLevelProject();
        project->buildData->removeArtifactAndExclusiveDependents(removedArtifact, logger, true,
                                                                 &artifactsToRemove);
    }
    for (Artifact * const artifact : std::as_const(artifactsToRemove)) {
        QBS_CHECK(!inputArtifacts.contains(artifact));
        removedArtifacts << artifact->filePath();
        delete artifact;
    }
}

static void copyProperty(JSContext *ctx, const QString &name, const JSValue &src, JSValue dst)
{
    setJsProperty(ctx, dst, name, getJsProperty(ctx, src, name));
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

void RulesApplicator::doApply(const ArtifactSet &inputArtifacts, JSValue prepareScriptContext)
{
    evalContext()->checkForCancelation();
    for (const Artifact *inputArtifact : inputArtifacts)
        QBS_CHECK(!inputArtifact->fileTags().intersects(m_rule->excludedInputs));

    qCDebug(lcBuildGraph) << "apply rule" << m_rule->toString()
                          << toStringList(inputArtifacts).join(QLatin1String(",\n            "));

    std::vector<std::pair<const RuleArtifact *, OutputArtifactInfo>> ruleArtifactArtifactMap;
    QList<Artifact *> outputArtifacts;

    m_transformer = Transformer::create();
    m_transformer->rule = m_rule;
    m_transformer->inputs = inputArtifacts;
    m_transformer->explicitlyDependsOn = m_explicitlyDependsOn;
    m_transformer->alwaysRun = m_rule->alwaysRun;
    m_oldTransformer.reset();

    engine()->clearRequestedProperties();

    // create the output artifacts from the set of input artifacts
    m_transformer->setupInputs(engine(), prepareScriptContext);
    m_transformer->setupExplicitlyDependsOn(engine(), prepareScriptContext);
    copyProperty(jsContext(), StringConstants::inputsVar(), prepareScriptContext, scope());
    copyProperty(jsContext(), StringConstants::inputVar(), prepareScriptContext, scope());
    copyProperty(jsContext(), StringConstants::explicitlyDependsOnVar(),
                 prepareScriptContext, scope());
    copyProperty(jsContext(), StringConstants::productVar(), prepareScriptContext, scope());
    copyProperty(jsContext(), StringConstants::projectVar(), prepareScriptContext, scope());
    if (m_rule->isDynamic()) {
        const ScopedJsValueList argList
                = engine()->argumentList(Rule::argumentNamesForOutputArtifacts(), scope());
        outputArtifacts = runOutputArtifactsScript(inputArtifacts, argList);
    } else {
        Set<QString> outputFilePaths;
        for (const auto &ruleArtifact : m_rule->artifacts) {
            const OutputArtifactInfo &outputInfo = createOutputArtifactFromRuleArtifact(
                        ruleArtifact, inputArtifacts, &outputFilePaths);
            if (!outputInfo.artifact)
                continue;
            outputArtifacts.push_back(outputInfo.artifact);
            ruleArtifactArtifactMap.emplace_back(ruleArtifact.get(), outputInfo);
        }
        if (m_rule->artifacts.empty()) {
            outputArtifacts.push_back(createOutputArtifactFromRuleArtifact(
                                          nullptr, inputArtifacts, &outputFilePaths).artifact);
        }
    }

    const auto newOutputs = rangeTo<ArtifactSet>(outputArtifacts);
    const ArtifactSet oldOutputs = collectOldOutputArtifacts(inputArtifacts);
    handleRemovedRuleOutputs(m_completeInputSet, oldOutputs - newOutputs, m_removedArtifacts,
                             m_logger);

    // The inputs become children of the rule node. Generated artifacts in the same product
    // already are children, because output artifacts become children of the producing
    // rule node's parent rule node.
    for (Artifact * const input : inputArtifacts) {
        if (input->artifactType == Artifact::SourceFile || input->product != m_ruleNode->product
                || input->producer()->rule()->collectedOutputFileTags().intersects(
                    m_ruleNode->rule()->excludedInputs)) {
            connect(m_ruleNode, input);
        } else {
            QBS_CHECK(m_ruleNode->children.contains(input));
        }
    }

    if (outputArtifacts.empty())
        return;

    for (Artifact * const outputArtifact : std::as_const(outputArtifacts)) {
        for (Artifact * const dependency : std::as_const(m_transformer->explicitlyDependsOn))
            connect(outputArtifact, dependency);
    }

    if (inputArtifacts != m_transformer->inputs)
        m_transformer->setupInputs(engine(), prepareScriptContext);

    // change the transformer outputs according to the bindings in Artifact
    if (!ruleArtifactArtifactMap.empty()) {
        const TemporaryGlobalObjectSetter gos(engine(), prepareScriptContext);
        for (auto it = ruleArtifactArtifactMap.crbegin(), end = ruleArtifactArtifactMap.crend();
             it != end; ++it) {
            const RuleArtifact *ra = it->first;
            if (ra->bindings.empty())
                continue;

            // expose attributes of this artifact
            const OutputArtifactInfo &outputInfo = it->second;
            Artifact *outputArtifact = outputInfo.artifact;
            outputArtifact->properties = outputArtifact->properties->clone();

            setJsProperty(jsContext(), scope(), StringConstants::fileNameProperty(),
                          engine()->toScriptValue(outputArtifact->filePath()));
            setJsProperty(jsContext(), scope(), StringConstants::fileTagsProperty(),
                          makeJsStringList(engine()->context(),
                                           outputArtifact->fileTags().toStringList()));

            QVariantMap artifactModulesCfg = outputArtifact->properties->value();
            for (const auto &binding : ra->bindings) {
                const ScopedJsValue scriptValue(jsContext(), engine()->evaluate(
                                                    JsValueOwner::Caller, binding.code,
                                                    binding.location.filePath(),
                                                    binding.location.line()));
                if (engine()->checkForJsError(binding.location)) {
                    ErrorInfo err = engine()->getAndClearJsError();
                    err.prepend(QStringLiteral("evaluating rule binding '%1'")
                                .arg(binding.name.join(QLatin1Char('.'))));
                    throw err;
                }
                const QVariant value = getJsVariant(jsContext(), scriptValue);
                setConfigProperty(artifactModulesCfg, binding.name, value);
                outputArtifact->pureProperties.emplace_back(binding.name, value);
            }
            outputArtifact->properties->setValue(artifactModulesCfg);
            if (!outputInfo.newlyCreated
                && (outputArtifact->fileTags() != outputInfo.oldFileTags
                    || !qVariantMapsEqual(
                        outputArtifact->properties->value(), outputInfo.oldProperties))) {
                invalidateArtifactAsRuleInputIfNecessary(outputArtifact);
            }
        }
    }

    m_transformer->setupOutputs(engine(), prepareScriptContext);
    const ScopedJsValueList argList = engine()->argumentList(Rule::argumentNamesForPrepare(),
                                                             prepareScriptContext);
    m_transformer->createCommands(engine(), m_rule->prepareScript, argList);
    if (Q_UNLIKELY(m_transformer->commands.empty()))
        throw ErrorInfo(Tr::tr("There is a rule without commands: %1.")
                        .arg(m_rule->toString()), m_rule->prepareScript.location());
    if (!m_oldTransformer || m_oldTransformer->outputs != m_transformer->outputs
            || m_oldTransformer->inputs != m_transformer->inputs
            || m_oldTransformer->explicitlyDependsOn != m_transformer->explicitlyDependsOn
            || m_oldTransformer->commands != m_transformer->commands
            || commandsNeedRerun(m_transformer.get(), m_product.get(), m_productsByName,
                                 m_projectsByName)) {
        for (Artifact * const output : std::as_const(outputArtifacts)) {
            output->clearTimestamp();
            m_invalidatedArtifacts += output;
        }
    }
    m_transformer->commandsNeedChangeTracking = false;
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

ArtifactSet RulesApplicator::collectAdditionalInputs(const FileTags &tags, const Rule *rule,
                                                     const ResolvedProduct *product,
                                                     InputsSources inputsSources)
{
    ArtifactSet artifacts;
    for (const FileTag &fileTag : tags) {
        for (Artifact *dependency : product->lookupArtifactsByFileTag(fileTag)) {
            // Skip excluded inputs.
            if (dependency->fileTags().intersects(rule->excludedInputs))
                continue;

            // Two cases are considered:
            // 1) An artifact is considered a dependency when it's part of the current product.
            // 2) An artifact marked with filesAreTargets: true inside a Group inside of a
            // Module also ends up in the results returned by product->lookupArtifactsByFileTag,
            // so it should be considered conceptually as a "dependent product artifact".
            if ((inputsSources == CurrentProduct && !dependency->isTargetOfModule())
                || (inputsSources == Dependencies && dependency->isTargetOfModule())) {
                artifacts << dependency;
            }
        }

        if (inputsSources == Dependencies) {
            for (const auto &depProduct : product->dependencies) {
                for (Artifact * const ta :
                     filterByType<Artifact>(depProduct->buildData->allNodes())) {
                    if (ta->fileTags().contains(fileTag)
                            && !ta->fileTags().intersects(rule->excludedInputs)) {
                        artifacts << ta;
                    }
                }
            }
        }
    }
    return artifacts;
}

ArtifactSet RulesApplicator::collectExplicitlyDependsOn(const Rule *rule,
                                                        const ResolvedProduct *product)
{
   ArtifactSet first = collectAdditionalInputs(
               rule->explicitlyDependsOn, rule, product, CurrentProduct);
   ArtifactSet second = collectAdditionalInputs(
               rule->explicitlyDependsOnFromDependencies, rule, product, Dependencies);
   return first.unite(second);
}

RulesApplicator::OutputArtifactInfo RulesApplicator::createOutputArtifactFromRuleArtifact(
        const RuleArtifactConstPtr &ruleArtifact, const ArtifactSet &inputArtifacts,
        Set<QString> *outputFilePaths)
{
    QString outputPath;
    FileTags fileTags;
    bool alwaysUpdated;
    if (ruleArtifact) {
        const ScopedJsValue scriptValue(
                    jsContext(),
                    engine()->evaluate(JsValueOwner::Caller, ruleArtifact->filePath,
                                       ruleArtifact->filePathLocation.filePath(),
                                       ruleArtifact->filePathLocation.line()));
        engine()->throwOnJsError(ruleArtifact->filePathLocation);
        outputPath = getJsString(jsContext(), scriptValue);
        fileTags = ruleArtifact->fileTags;
        alwaysUpdated = ruleArtifact->alwaysUpdated;
    } else {
        outputPath = QStringLiteral("__dummyoutput__");
        QByteArray hashInput = m_rule->toString().toLatin1();
        for (const Artifact * const input : inputArtifacts)
            hashInput += input->filePath().toLatin1();
        outputPath += QLatin1String(QCryptographicHash::hash(hashInput, QCryptographicHash::Sha1)
                                    .toHex().left(16));
        fileTags = m_rule->outputFileTags;
        alwaysUpdated = false;
    }
    outputPath = FileInfo::resolvePath(m_product->buildDirectory(), outputPath);
    if (Q_UNLIKELY(!outputFilePaths->insert(outputPath).second)) {
        throw ErrorInfo(Tr::tr("Rule %1 already created '%2'.")
                        .arg(m_rule->toString(), outputPath));
    }
    return createOutputArtifact(outputPath, fileTags, alwaysUpdated, inputArtifacts);
}

RulesApplicator::OutputArtifactInfo RulesApplicator::createOutputArtifact(const QString &filePath,
        const FileTags &fileTags, bool alwaysUpdated, const ArtifactSet &inputArtifacts)
{
    const QString outputPath = resolveOutPath(filePath);
    if (m_rule->isDynamic()) {
        const Set<FileTag> undeclaredTags = fileTags - m_rule->collectedOutputFileTags();
        if (!undeclaredTags.empty()) {
            throw ErrorInfo(Tr::tr("Artifact '%1' has undeclared file tags [\"%2\"].")
                            .arg(outputPath, undeclaredTags.toStringList()
                                 .join(QLatin1String("\",\""))),
                            m_rule->prepareScript.location());
        }
    }

    OutputArtifactInfo outputInfo;
    Artifact *& outputArtifact = outputInfo.artifact;
    outputArtifact = lookupArtifact(m_product, outputPath);
    outputInfo.newlyCreated = !outputArtifact;
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

            e += QStringLiteral("  while trying to apply:   %1:%2:%3  %4\n")
                .arg(m_rule->prepareScript.location().filePath())
                .arg(m_rule->prepareScript.location().line())
                .arg(m_rule->prepareScript.location().column())
                .arg(str);

            e += QStringLiteral("  was already defined in:  %1:%2:%3  %4\n")
                .arg(transformer->rule->prepareScript.location().filePath())
                .arg(transformer->rule->prepareScript.location().line())
                .arg(transformer->rule->prepareScript.location().column())
                .arg(str);

            throw ErrorInfo(e);
        }
        if (transformer && !m_rule->multiplex && transformer->inputs != inputArtifacts) {
            QBS_CHECK(inputArtifacts.size() == 1);
            QBS_CHECK(transformer->inputs.size() == 1);
            ErrorInfo error(Tr::tr("Conflicting instances of rule '%1':").arg(m_rule->toString()),
                            m_rule->prepareScript.location());
            error.append(Tr::tr("Output artifact '%1' is to be produced from input "
                                "artifacts '%2' and '%3', but the rule is not a multiplex rule.")
                         .arg(outputArtifact->filePath(),
                              (*transformer->inputs.cbegin())->filePath(),
                              (*inputArtifacts.cbegin())->filePath()));
            throw error;
        }
        m_transformer->rescueChangeTrackingData(outputArtifact->transformer);
        m_oldTransformer = outputArtifact->transformer;
        outputInfo.oldFileTags = outputArtifact->fileTags();
        outputInfo.oldProperties = outputArtifact->properties->value();
    } else {
        std::unique_ptr<Artifact> newArtifact(new Artifact);
        newArtifact->artifactType = Artifact::Generated;
        newArtifact->setFilePath(outputPath);
        insertArtifact(m_product, newArtifact.get());
        m_createdArtifacts += newArtifact.get();
        outputArtifact = newArtifact.release();
        qCDebug(lcExec).noquote() << "rule created" << outputArtifact->toString();
        connect(outputArtifact, m_ruleNode);
    }

    outputArtifact->alwaysUpdated = alwaysUpdated;
    outputArtifact->pureFileTags = fileTags;
    provideFullFileTagsAndProperties(outputArtifact);
    if (outputInfo.newlyCreated || outputInfo.oldFileTags != outputArtifact->fileTags()) {
        for (RuleNode * const parentRule : filterByType<RuleNode>(m_ruleNode->parents))
            connect(parentRule, outputArtifact);
    }

    for (Artifact * const inputArtifact : inputArtifacts) {
        QBS_CHECK(outputArtifact != inputArtifact);
        connect(outputArtifact, inputArtifact);
    }

    outputArtifact->transformer = m_transformer;
    m_transformer->outputs.insert(outputArtifact);
    QBS_CHECK(m_rule->multiplex || m_transformer->inputs.size() == 1);

    return outputInfo;
}

class RuleOutputArtifactsException : public ErrorInfo
{
public:
    using ErrorInfo::ErrorInfo;
};

QList<Artifact *> RulesApplicator::runOutputArtifactsScript(const ArtifactSet &inputArtifacts,
        const JSValueList &args)
{
    QList<Artifact *> lst;
    const ScopedJsValue fun(jsContext(),
                            engine()->evaluate(JsValueOwner::Caller,
                                               m_rule->outputArtifactsScript.sourceCode(),
                                               m_rule->outputArtifactsScript.location().filePath(),
                                               m_rule->outputArtifactsScript.location().line()));
    if (!JS_IsFunction(jsContext(), fun))
        throw ErrorInfo(QStringLiteral("Function expected."),
                        m_rule->outputArtifactsScript.location());
    JSValueList argv(args.begin(), args.end());
    const ScopedJsValue res(
                jsContext(),
                JS_Call(jsContext(), fun, engine()->globalObject(), int(args.size()), argv.data()));
    engine()->throwOnJsError(m_rule->outputArtifactsScript.location());
    if (!JS_IsArray(jsContext(), res))
        throw ErrorInfo(Tr::tr("Rule.outputArtifacts must return an array of objects."),
                        m_rule->outputArtifactsScript.location());
    const quint32 c = getJsIntProperty(jsContext(), res, StringConstants::lengthProperty());
    for (quint32 i = 0; i < c; ++i) {
        try {
            ScopedJsValue val(engine()->context(), JS_GetPropertyUint32(jsContext(), res, i));
            lst.push_back(createOutputArtifactFromScriptValue(val, inputArtifacts));
        } catch (const RuleOutputArtifactsException &roae) {
            ErrorInfo ei = roae;
            ei.prepend(Tr::tr("Error in Rule.outputArtifacts[%1]").arg(i),
                       m_rule->outputArtifactsScript.location());
            throw ei;
        }
    }
    return lst;
}

class ArtifactBindingsExtractor
{
    struct Entry
    {
        Entry(QString module, QString name, QVariant value)
            : module(std::move(module)), name(std::move(name)), value(std::move(value))
        {}

        QString module;
        QString name;
        QVariant value;
    };
    ScriptEngine *m_engine = nullptr;
    JSContext *m_ctx = nullptr;
    std::vector<Entry> m_propertyValues;

    static Set<QString> getArtifactItemPropertyNames()
    {
        Set<QString> s;
        const auto properties = BuiltinDeclarations::instance().declarationsForType(
                ItemType::Artifact).properties();
        for (const PropertyDeclaration &pd : properties) {
            s.insert(pd.name());
        }
        s.insert(StringConstants::explicitlyDependsOnProperty());
        return s;
    }

    void extractPropertyValues(const JSValue &obj, const QString &moduleName = QString())
    {
        handleJsProperties(m_ctx, obj, [&](const JSAtom &prop, const JSPropertyDescriptor &desc) {
            const QString name = getJsString(m_ctx, prop);
            if (moduleName.isEmpty()) {
                // Ignore property names that are part of the Artifact item.
                static const Set<QString> artifactItemPropertyNames
                        = getArtifactItemPropertyNames();
                if (artifactItemPropertyNames.contains(name))
                    return;
            }

            const JSValue value = desc.value;
            if (JS_IsObject(value) && !JS_IsArray(m_ctx, value) && !JS_IsError(m_ctx, value)
                && !JS_IsRegExp(value)) {
                QString newModuleName;
                if (!moduleName.isEmpty())
                    newModuleName.append(moduleName + QLatin1Char('.'));
                newModuleName.append(name);
                extractPropertyValues(value, newModuleName);
            } else {
                m_propertyValues.emplace_back(moduleName, name, getJsVariant(m_ctx, value));
            }
        });
    }
public:
    void apply(ScriptEngine *engine, Artifact *outputArtifact, const JSValue &obj)
    {
        m_engine = engine;
        m_ctx = m_engine->context();
        extractPropertyValues(obj);
        if (m_propertyValues.empty())
            return;

        outputArtifact->properties = outputArtifact->properties->clone();
        QVariantMap artifactCfg = outputArtifact->properties->value();
        for (const auto &e : m_propertyValues) {
            const QStringList key{e.module, e.name};
            setConfigProperty(artifactCfg, key, e.value);
            outputArtifact->pureProperties.emplace_back(key, e.value);
        }
        outputArtifact->properties->setValue(artifactCfg);
    }
};

Artifact *RulesApplicator::createOutputArtifactFromScriptValue(const JSValue &obj,
        const ArtifactSet &inputArtifacts)
{
    if (!JS_IsObject(obj)) {
        throw ErrorInfo(Tr::tr("Elements of the Rule.outputArtifacts array must be "
                               "of Object type."), m_rule->outputArtifactsScript.location());
    }
    QString unresolvedFilePath;
    const ScopedJsValue jsFilePath(jsContext(), getJsProperty(jsContext(), obj,
                                                              StringConstants::filePathProperty()));
    if (JS_IsString(jsFilePath))
        unresolvedFilePath = getJsString(jsContext(), jsFilePath);
    if (unresolvedFilePath.isEmpty()) {
        throw RuleOutputArtifactsException(
                Tr::tr("Property filePath must be a non-empty string."));
    }
    const QString filePath = FileInfo::resolvePath(m_product->buildDirectory(), unresolvedFilePath);
    const FileTags fileTags = FileTags::fromStringList(
                getJsStringListProperty(jsContext(), obj, StringConstants::fileTagsProperty()));
    const QVariant alwaysUpdatedVar = getJsVariantProperty(jsContext(), obj,
                                                           StringConstants::alwaysUpdatedProperty());
    const bool alwaysUpdated = alwaysUpdatedVar.isValid() ? alwaysUpdatedVar.toBool() : true;
    OutputArtifactInfo outputInfo = createOutputArtifact(filePath, fileTags, alwaysUpdated,
                                                         inputArtifacts);
    if (outputInfo.artifact->fileTags().empty()) {
        // Check the file tags after file taggers were run.
        throw RuleOutputArtifactsException(
                Tr::tr("Property fileTags for artifact '%1' must be a non-empty string list. "
                       "Alternatively, a FileTagger can be provided.")
                    .arg(unresolvedFilePath));
    }
    const FileTags explicitlyDependsOn = FileTags::fromStringList(getJsStringListProperty(
            jsContext(), obj, StringConstants::explicitlyDependsOnProperty()));
    for (const FileTag &tag : explicitlyDependsOn) {
        for (Artifact * const dependency : m_product->lookupArtifactsByFileTag(tag))
            connect(outputInfo.artifact, dependency);
    }
    ArtifactBindingsExtractor().apply(engine(), outputInfo.artifact, obj);
    if (!outputInfo.newlyCreated
        && (outputInfo.artifact->fileTags() != outputInfo.oldFileTags
            || !qVariantMapsEqual(
                outputInfo.artifact->properties->value(), outputInfo.oldProperties))) {
        invalidateArtifactAsRuleInputIfNecessary(outputInfo.artifact);
    }
    return outputInfo.artifact;
}

QString RulesApplicator::resolveOutPath(const QString &path) const
{
    const QString buildDir = m_product->topLevelProject()->buildDirectory;
    QString result = QDir::cleanPath(FileInfo::resolvePath(buildDir, path));
    if (!result.startsWith(buildDir + QLatin1Char('/'))) {
        throw ErrorInfo(
            Tr::tr("Refusing to create artifact '%1' outside of build directory '%2'.")
                .arg(QDir::toNativeSeparators(result), QDir::toNativeSeparators(buildDir)),
            m_rule->prepareScript.location());
    }
    return result;
}

const RulesEvaluationContextPtr &RulesApplicator::evalContext() const
{
    return m_product->topLevelProject()->buildData->evaluationContext;
}

ScriptEngine *RulesApplicator::engine() const { return evalContext()->engine(); }
JSContext *RulesApplicator::jsContext() const { return engine()->context(); }
JSValue RulesApplicator::scope() const { return evalContext()->scope(); }

} // namespace Internal
} // namespace qbs
