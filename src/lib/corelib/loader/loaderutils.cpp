/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#include "loaderutils.h"

#include "dependenciesresolver.h"
#include "itemreader.h"
#include "localprofiles.h"
#include "moduleinstantiator.h"
#include "modulepropertymerger.h"
#include "probesresolver.h"
#include "productitemmultiplexer.h"

#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/item.h>
#include <language/language.h>
#include <language/resolvedfilecontext.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/progressobserver.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

namespace qbs::Internal {

void mergeParameters(QVariantMap &dst, const QVariantMap &src)
{
    for (auto it = src.begin(); it != src.end(); ++it) {
        if (it.value().userType() == QMetaType::QVariantMap) {
            QVariant &vdst = dst[it.key()];
            QVariantMap mdst = vdst.toMap();
            mergeParameters(mdst, it.value().toMap());
            vdst = mdst;
        } else {
            dst[it.key()] = it.value();
        }
    }
}

void adjustParametersScopes(Item *item, Item *scope)
{
    if (item->type() == ItemType::ModuleParameters) {
        item->setScope(scope);
        return;
    }

    for (const auto &value : item->properties()) {
        if (value->type() == Value::ItemValueType)
            adjustParametersScopes(std::static_pointer_cast<ItemValue>(value)->item(), scope);
    }
}

QString ProductContext::uniqueName() const
{
    return ResolvedProduct::uniqueName(name, multiplexConfigurationId);
}

QString ProductContext::displayName() const
{
    return ProductItemMultiplexer::fullProductDisplayName(name, multiplexConfigurationId);
}

void ProductContext::handleError(const ErrorInfo &error)
{
    const bool alreadyHadError = delayedError.hasError();
    if (!alreadyHadError) {
        delayedError.append(Tr::tr("Error while handling product '%1':")
                                .arg(name), item->location());
    }
    if (error.isInternalError()) {
        if (alreadyHadError) {
            qCDebug(lcModuleLoader()) << "ignoring subsequent internal error" << error.toString()
                                      << "in product" << name;
            return;
        }
    }
    const auto errorItems = error.items();
    for (const ErrorItem &ei : errorItems)
        delayedError.append(ei.description(), ei.codeLocation());
    project->topLevelProject->disabledItems << item;
    project->topLevelProject->erroneousProducts.insert(name);
}

bool TopLevelProjectContext::checkItemCondition(Item *item, Evaluator &evaluator)
{
    if (evaluator.boolValue(item, StringConstants::conditionProperty()))
        return true;
    disabledItems += item;
    return false;
}

void TopLevelProjectContext::checkCancelation()
{
    if (progressObserver && progressObserver->canceled())
        throw CancelException();
}

QString TopLevelProjectContext::sourceCodeForEvaluation(const JSSourceValueConstPtr &value)
{
    QString &code = sourceCode[value->sourceCode()];
    if (!code.isNull())
        return code;
    code = value->sourceCodeForEvaluation();
    return code;
}

ScriptFunctionPtr TopLevelProjectContext::scriptFunctionValue(Item *item, const QString &name)
{
    JSSourceValuePtr value = item->sourceProperty(name);
    ScriptFunctionPtr &script = scriptFunctionMap[value ? value->location() : CodeLocation()];
    if (!script.get()) {
        script = ScriptFunction::create();
        const PropertyDeclaration decl = item->propertyDeclaration(name);
        script->sourceCode = sourceCodeAsFunction(value, decl);
        script->location = value->location();
        script->fileContext = resolvedFileContext(value->file());
    }
    return script;
}

QString TopLevelProjectContext::sourceCodeAsFunction(const JSSourceValueConstPtr &value,
                                                     const PropertyDeclaration &decl)
{
    QString &scriptFunction = scriptFunctions[std::make_pair(value->sourceCode(),
                                                             decl.functionArgumentNames())];
    if (!scriptFunction.isNull())
        return scriptFunction;
    const QString args = decl.functionArgumentNames().join(QLatin1Char(','));
    if (value->hasFunctionForm()) {
        // Insert the argument list.
        scriptFunction = value->sourceCodeForEvaluation();
        scriptFunction.insert(10, args);
        // Remove the function application "()" that has been
        // added in ItemReaderASTVisitor::visitStatement.
        scriptFunction.chop(2);
    } else {
        scriptFunction = QLatin1String("(function(") + args + QLatin1String("){return ")
                + value->sourceCode().toString() + QLatin1String(";})");
    }
    return scriptFunction;
}

const ResolvedFileContextPtr &TopLevelProjectContext::resolvedFileContext(
        const FileContextConstPtr &ctx)
{
    ResolvedFileContextPtr &result = fileContextMap[ctx];
    if (!result)
        result = ResolvedFileContext::create(*ctx);
    return result;
}

class LoaderState::Private
{
public:
    Private(LoaderState &q, const SetupProjectParameters &parameters, ItemPool &itemPool,
            Evaluator &evaluator, Logger &logger)
        : parameters(parameters), itemPool(itemPool), evaluator(evaluator), logger(logger),
          itemReader(q), probesResolver(q), propertyMerger(q), localProfiles(q),
          moduleInstantiator(q), dependenciesResolver(q),
          multiplexer(q, [this](Item *productItem) {
            return moduleInstantiator.retrieveQbsItem(productItem);
          })
    {}

    const SetupProjectParameters &parameters;
    ItemPool &itemPool;
    Evaluator &evaluator;
    Logger &logger;

    TopLevelProjectContext topLevelProject;
    ItemReader itemReader;
    ProbesResolver probesResolver;
    ModulePropertyMerger propertyMerger;
    LocalProfiles localProfiles;
    ModuleInstantiator moduleInstantiator;
    DependenciesResolver dependenciesResolver;
    ProductItemMultiplexer multiplexer;
};

LoaderState::LoaderState(const SetupProjectParameters &parameters, ItemPool &itemPool,
                         Evaluator &evaluator, Logger &logger)
    : d(makePimpl<Private>(*this, parameters, itemPool, evaluator, logger))
{
    d->itemReader.init();
}

LoaderState::~LoaderState() = default;
const SetupProjectParameters &LoaderState::parameters() const { return d->parameters; }
DependenciesResolver &LoaderState::dependenciesResolver() { return d->dependenciesResolver; }
ItemPool &LoaderState::itemPool() { return d->itemPool; }
Evaluator &LoaderState::evaluator() { return d->evaluator; }
Logger &LoaderState::logger() { return d->logger; }
ModuleInstantiator &LoaderState::moduleInstantiator() { return d->moduleInstantiator; }
ProductItemMultiplexer &LoaderState::multiplexer() { return d->multiplexer; }
ItemReader &LoaderState::itemReader() { return d->itemReader; }
LocalProfiles &LoaderState::localProfiles() { return d->localProfiles; }
ProbesResolver &LoaderState::probesResolver() { return d->probesResolver; }
ModulePropertyMerger &LoaderState::propertyMerger() { return d->propertyMerger; }
TopLevelProjectContext &LoaderState::topLevelProject() { return d->topLevelProject; }

static QString verbatimValue(LoaderState &state, const ValueConstPtr &value)
{
    QString result;
    if (value && value->type() == Value::JSSourceValueType) {
        const JSSourceValueConstPtr sourceValue = std::static_pointer_cast<const JSSourceValue>(
                    value);
        result = state.topLevelProject().sourceCodeForEvaluation(sourceValue);
    }
    return result;
}

static void resolveRuleArtifactBinding(
        LoaderState &state, const RuleArtifactPtr &ruleArtifact, Item *item,
        const QStringList &namePrefix, QualifiedIdSet *seenBindings)
{
    for (auto it = item->properties().constBegin(); it != item->properties().constEnd(); ++it) {
        const QStringList name = QStringList(namePrefix) << it.key();
        if (it.value()->type() == Value::ItemValueType) {
            resolveRuleArtifactBinding(state, ruleArtifact,
                                       std::static_pointer_cast<ItemValue>(it.value())->item(),
                                       name, seenBindings);
        } else if (it.value()->type() == Value::JSSourceValueType) {
            const auto insertResult = seenBindings->insert(name);
            if (!insertResult.second)
                continue;
            JSSourceValuePtr sourceValue = std::static_pointer_cast<JSSourceValue>(it.value());
            RuleArtifact::Binding rab;
            rab.name = name;
            rab.code = state.topLevelProject().sourceCodeForEvaluation(sourceValue);
            rab.location = sourceValue->location();
            ruleArtifact->bindings.push_back(rab);
        } else {
            QBS_ASSERT(!"unexpected value type", continue);
        }
    }
}

static void resolveRuleArtifact(LoaderState &state, const RulePtr &rule, Item *item)
{
    RuleArtifactPtr artifact = RuleArtifact::create();
    rule->artifacts.push_back(artifact);
    artifact->location = item->location();

    if (const auto sourceProperty = item->sourceProperty(StringConstants::filePathProperty()))
        artifact->filePathLocation = sourceProperty->location();

    artifact->filePath = verbatimValue(state, item->property(StringConstants::filePathProperty()));
    artifact->fileTags = state.evaluator().fileTagsValue(item, StringConstants::fileTagsProperty());
    artifact->alwaysUpdated = state.evaluator().boolValue(
                item, StringConstants::alwaysUpdatedProperty());

    QualifiedIdSet seenBindings;
    for (Item *obj = item; obj; obj = obj->prototype()) {
        for (QMap<QString, ValuePtr>::const_iterator it = obj->properties().constBegin();
             it != obj->properties().constEnd(); ++it)
        {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            resolveRuleArtifactBinding(
                        state, artifact, std::static_pointer_cast<ItemValue>(it.value())->item(),
                        QStringList(it.key()), &seenBindings);
        }
    }
}

void resolveRule(LoaderState &state, Item *item, ProjectContext *projectContext,
                 ProductContext *productContext, ModuleContext *moduleContext)
{
    Evaluator &evaluator = state.evaluator();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty()))
        return;

    RulePtr rule = Rule::create();

    // read artifacts
    bool hasArtifactChildren = false;
    for (Item * const child : item->children()) {
        if (Q_UNLIKELY(child->type() != ItemType::Artifact)) {
            throw ErrorInfo(Tr::tr("'Rule' can only have children of type 'Artifact'."),
                               child->location());
        }
        hasArtifactChildren = true;
        resolveRuleArtifact(state, rule, child);
    }

    rule->name = evaluator.stringValue(item, StringConstants::nameProperty());
    rule->prepareScript.initialize(state.topLevelProject().scriptFunctionValue(
                                       item, StringConstants::prepareProperty()));
    rule->outputArtifactsScript.initialize(state.topLevelProject().scriptFunctionValue(
                                               item, StringConstants::outputArtifactsProperty()));
    rule->outputFileTags = evaluator.fileTagsValue(item, StringConstants::outputFileTagsProperty());
    if (rule->outputArtifactsScript.isValid()) {
        if (hasArtifactChildren)
            throw ErrorInfo(Tr::tr("The Rule.outputArtifacts script is not allowed in rules "
                                   "that contain Artifact items."),
                        item->location());
    }
    if (!hasArtifactChildren && rule->outputFileTags.empty()) {
        throw ErrorInfo(Tr::tr("A rule needs to have Artifact items or a non-empty "
                               "outputFileTags property."), item->location());
    }
    rule->multiplex = evaluator.boolValue(item, StringConstants::multiplexProperty());
    rule->alwaysRun = evaluator.boolValue(item, StringConstants::alwaysRunProperty());
    rule->inputs = evaluator.fileTagsValue(item, StringConstants::inputsProperty());
    rule->inputsFromDependencies
        = evaluator.fileTagsValue(item, StringConstants::inputsFromDependenciesProperty());
    bool requiresInputsSet = false;
    rule->requiresInputs = evaluator.boolValue(item, StringConstants::requiresInputsProperty(),
                                               &requiresInputsSet);
    if (!requiresInputsSet)
        rule->requiresInputs = rule->declaresInputs();
    rule->auxiliaryInputs
        = evaluator.fileTagsValue(item, StringConstants::auxiliaryInputsProperty());
    rule->excludedInputs
        = evaluator.fileTagsValue(item, StringConstants::excludedInputsProperty());
    if (rule->excludedInputs.empty()) {
        rule->excludedInputs = evaluator.fileTagsValue(
                    item, StringConstants::excludedAuxiliaryInputsProperty());
    }
    rule->explicitlyDependsOn
        = evaluator.fileTagsValue(item, StringConstants::explicitlyDependsOnProperty());
    rule->explicitlyDependsOnFromDependencies = evaluator.fileTagsValue(
                item, StringConstants::explicitlyDependsOnFromDependenciesProperty());
    rule->module = moduleContext ? moduleContext->module : projectContext->dummyModule;
    if (!rule->multiplex && !rule->declaresInputs()) {
        throw ErrorInfo(Tr::tr("Rule has no inputs, but is not a multiplex rule."),
                        item->location());
    }
    if (!rule->multiplex && !rule->requiresInputs) {
        throw ErrorInfo(Tr::tr("Rule.requiresInputs is false for non-multiplex rule."),
                        item->location());
    }
    if (!rule->declaresInputs() && rule->requiresInputs) {
        throw ErrorInfo(Tr::tr("Rule.requiresInputs is true, but the rule "
                               "does not declare any input tags."), item->location());
    }
    if (productContext) {
        rule->product = productContext->product.get();
        productContext->product->rules.push_back(rule);
    } else {
        projectContext->rules.push_back(rule);
    }
}

void resolveFileTagger(LoaderState &state, Item *item, ProjectContext *projectContext,
                       ProductContext *productContext)
{
    Evaluator &evaluator = state.evaluator();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty()))
        return;
    std::vector<FileTaggerConstPtr> &fileTaggers = productContext
            ? productContext->product->fileTaggers
            : projectContext->fileTaggers;
    const QStringList patterns = evaluator.stringListValue(item,
                                                           StringConstants::patternsProperty());
    if (patterns.empty())
        throw ErrorInfo(Tr::tr("FileTagger.patterns must be a non-empty list."), item->location());

    const FileTags fileTags = evaluator.fileTagsValue(item, StringConstants::fileTagsProperty());
    if (fileTags.empty())
        throw ErrorInfo(Tr::tr("FileTagger.fileTags must not be empty."), item->location());

    for (const QString &pattern : patterns) {
        if (pattern.isEmpty())
            throw ErrorInfo(Tr::tr("A FileTagger pattern must not be empty."), item->location());
    }

    const int priority = evaluator.intValue(item, StringConstants::priorityProperty());
    fileTaggers.push_back(FileTagger::create(patterns, fileTags, priority));
}

void resolveJobLimit(LoaderState &state, Item *item, ProjectContext *projectContext,
                     ProductContext *productContext, ModuleContext *moduleContext)
{
    Evaluator &evaluator = state.evaluator();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty()))
        return;
    const QString jobPool = evaluator.stringValue(item, StringConstants::jobPoolProperty());
    if (jobPool.isEmpty())
        throw ErrorInfo(Tr::tr("A JobLimit item needs to have a non-empty '%1' property.")
                        .arg(StringConstants::jobPoolProperty()), item->location());
    bool jobCountWasSet;
    const int jobCount = evaluator.intValue(item, StringConstants::jobCountProperty(), -1,
                                            &jobCountWasSet);
    if (!jobCountWasSet) {
        throw ErrorInfo(Tr::tr("A JobLimit item needs to have a '%1' property.")
                        .arg(StringConstants::jobCountProperty()), item->location());
    }
    if (jobCount < 0) {
        throw ErrorInfo(Tr::tr("A JobLimit item must have a non-negative '%1' property.")
                        .arg(StringConstants::jobCountProperty()), item->location());
    }
    JobLimits &jobLimits = moduleContext
            ? moduleContext->jobLimits
            : productContext ? productContext->product->jobLimits
            : projectContext->jobLimits;
    JobLimit jobLimit(jobPool, jobCount);
    const int oldLimit = jobLimits.getLimit(jobPool);
    if (oldLimit == -1 || oldLimit > jobCount)
        jobLimits.setJobLimit(jobLimit);
}

const FileTag unknownFileTag()
{
    static const FileTag tag("unknown-file-tag");
    return tag;
}

bool ProductContext::dependenciesResolvingPending() const
{
    return (!dependenciesContext || !dependenciesContext->dependenciesResolved)
            && !product && !delayedError.hasError();
}

TimingData &TimingData::operator+=(const TimingData &other)
{
    dependenciesResolving += other.dependenciesResolving;
    moduleProviders += other.moduleProviders;
    moduleInstantiation += other.moduleInstantiation;
    propertyMerging += other.propertyMerging;
    groupsSetup += other.groupsSetup;
    groupsResolving += other.groupsResolving;
    preparingProducts += other.preparingProducts;
    probes += other.probes;
    propertyEvaluation += other.propertyEvaluation;
    propertyChecking += other.propertyChecking;
    return *this;
}

DependenciesContext::~DependenciesContext() = default;

} // namespace qbs::Internal
