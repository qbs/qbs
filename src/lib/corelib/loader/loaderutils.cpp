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

#include "itemreader.h"

#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/itempool.h>
#include <language/language.h>
#include <language/resolvedfilecontext.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

namespace qbs::Internal {

QString fullProductDisplayName(const QString &name, const QString &multiplexId)
{
    static const auto multiplexIdToString =[](const QString &id) {
        return QString::fromUtf8(QByteArray::fromBase64(id.toUtf8()));
    };
    QString result = name;
    if (!multiplexId.isEmpty())
        result.append(QLatin1Char(' ')).append(multiplexIdToString(multiplexId));
    return result;
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
    return fullProductDisplayName(name, multiplexConfigurationId);
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
    project->topLevelProject->addDisabledItem(item);
}

TopLevelProjectContext::~TopLevelProjectContext() { qDeleteAll(m_projects); }

bool TopLevelProjectContext::checkItemCondition(Item *item, Evaluator &evaluator)
{
    if (evaluator.boolValue(item, StringConstants::conditionProperty()))
        return true;
    addDisabledItem(item);
    return false;
}

void TopLevelProjectContext::checkCancelation()
{
    if (m_progressObserver && m_progressObserver->canceled())
        m_canceled = true;
    if (m_canceled)
        throw CancelException();
}

QString TopLevelProjectContext::sourceCodeForEvaluation(const JSSourceValueConstPtr &value)
{
    std::lock_guard lock(m_sourceCode.mutex);
    QString &code = m_sourceCode.data[value->sourceCode()];
    if (!code.isNull())
        return code;
    code = value->sourceCodeForEvaluation();
    return code;
}

ScriptFunctionPtr TopLevelProjectContext::scriptFunctionValue(Item *item, const QString &name)
{
    const JSSourceValuePtr value = item->sourceProperty(name);
    QBS_CHECK(value);
    std::lock_guard lock(m_scriptFunctionMap.mutex);
    ScriptFunctionPtr &script = m_scriptFunctionMap.data[value->location()];
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
    std::lock_guard lock(m_scriptFunctions.mutex);
    QString &scriptFunction = m_scriptFunctions.data[std::make_pair(value->sourceCode(),
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
    ResolvedFileContextPtr &result = m_fileContextMap[ctx];
    if (!result)
        result = ResolvedFileContext::create(*ctx);
    return result;
}

void TopLevelProjectContext::removeProductToHandle(const ProductContext &product)
{
    std::unique_lock lock(m_productsToHandle.mutex);
    m_productsToHandle.data.remove(&product);
}

bool TopLevelProjectContext::isProductQueuedForHandling(const ProductContext &product) const
{
    std::shared_lock lock(m_productsToHandle.mutex);
    return m_productsToHandle.data.contains(&product);
}

void TopLevelProjectContext::addDisabledItem(Item *item)
{
    std::unique_lock lock(m_disabledItems.mutex);
    m_disabledItems.data << item;
}

bool TopLevelProjectContext::isDisabledItem(const Item *item) const
{
    std::shared_lock lock(m_disabledItems.mutex);
    return m_disabledItems.data.contains(item);
}

void TopLevelProjectContext::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

ProgressObserver *TopLevelProjectContext::progressObserver() const { return m_progressObserver; }

void TopLevelProjectContext::addQueuedError(const ErrorInfo &error)
{
    std::lock_guard lock(m_queuedErrors.mutex);
    m_queuedErrors.data << error;
}

void TopLevelProjectContext::addProfileConfig(const QString &profileName,
                                              const QVariantMap &profileConfig)
{
    m_profileConfigs.insert(profileName, profileConfig);
}

std::optional<QVariantMap> TopLevelProjectContext::profileConfig(const QString &profileName) const
{
    const auto it = m_profileConfigs.constFind(profileName);
    if (it == m_profileConfigs.constEnd())
        return {};
    return it.value().toMap();
}

void TopLevelProjectContext::addProjectLevelProbe(const ProbeConstPtr &probe)
{
    m_probesInfo.projectLevelProbes << probe;
}

const std::vector<ProbeConstPtr> TopLevelProjectContext::projectLevelProbes() const
{
    return m_probesInfo.projectLevelProbes;
}

void TopLevelProjectContext::addProduct(ProductContext &product)
{
    m_productsByName.insert({product.name, &product});
}

void TopLevelProjectContext::addProductByType(ProductContext &product, const FileTags &tags)
{
    std::unique_lock lock(m_productsByType.mutex);
    for (const FileTag &tag : tags)
        m_productsByType.data.insert({tag, &product});
}

ProductContext *TopLevelProjectContext::productWithNameAndConstraint(
        const QString &name, const std::function<bool (ProductContext &)> &constraint)
{
    const auto candidates = m_productsByName.equal_range(name);
    for (auto it = candidates.first; it != candidates.second; ++it) {
        ProductContext * const candidate = it->second;
        if (constraint(*candidate))
            return candidate;
    }
    return nullptr;
}

std::vector<ProductContext *> TopLevelProjectContext::productsWithNameAndConstraint(
        const QString &name, const std::function<bool (ProductContext &)> &constraint)
{
    std::vector<ProductContext *> result;
    const auto candidates = m_productsByName.equal_range(name);
    for (auto it = candidates.first; it != candidates.second; ++it) {
        ProductContext * const candidate = it->second;
        if (constraint(*candidate))
            result << candidate;
    }
    return result;
}

std::vector<ProductContext *> TopLevelProjectContext::productsWithTypeAndConstraint(
        const FileTags &tags, const std::function<bool (ProductContext &)> &constraint)
{
    std::shared_lock lock(m_productsByType.mutex);
    std::vector<ProductContext *> matchingProducts;
    for (const FileTag &typeTag : tags) {
        const auto range = m_productsByType.data.equal_range(typeTag);
        for (auto it = range.first; it != range.second; ++it) {
            if (constraint(*it->second))
                matchingProducts.push_back(it->second);
        }
    }
    return matchingProducts;
}

std::vector<std::pair<ProductContext *, CodeLocation>>
TopLevelProjectContext::finishedProductsWithBulkDependency(const FileTag &tag) const
{
    return m_reverseBulkDependencies.value(tag);
}

void TopLevelProjectContext::registerBulkDependencies(ProductContext &product)
{
    for (const auto &tagAndLoc : product.bulkDependencies) {
        for (const FileTag &tag : tagAndLoc.first)
            m_reverseBulkDependencies[tag].emplace_back(&product, tagAndLoc.second);
    }
}

void TopLevelProjectContext::addProjectNameUsedInOverrides(const QString &name)
{
    m_projectNamesUsedInOverrides << name;
}

const Set<QString> &TopLevelProjectContext::projectNamesUsedInOverrides() const
{
    return m_projectNamesUsedInOverrides;
}

void TopLevelProjectContext::addProductNameUsedInOverrides(const QString &name)
{
    m_productNamesUsedInOverrides << name;
}

const Set<QString> &TopLevelProjectContext::productNamesUsedInOverrides() const
{
    return m_productNamesUsedInOverrides;
}

void TopLevelProjectContext::addMultiplexConfiguration(const QString &id, const QVariantMap &config)
{
    m_multiplexConfigsById.insert(std::make_pair(id, config));
}

QVariantMap TopLevelProjectContext::multiplexConfiguration(const QString &id) const
{
    if (id.isEmpty())
        return {};
    const auto it = m_multiplexConfigsById.find(id);
    QBS_CHECK(it != m_multiplexConfigsById.end() && !it->second.isEmpty());
    return it->second;
}

std::lock_guard<std::mutex> TopLevelProjectContext::moduleProvidersCacheLock()
{
    return std::lock_guard<std::mutex>(m_moduleProvidersCacheMutex);
}

void TopLevelProjectContext::setModuleProvidersCache(const ModuleProvidersCache &cache)
{
    m_moduleProvidersCache = cache;
}

ModuleProviderInfo *TopLevelProjectContext::moduleProvider(const ModuleProvidersCacheKey &key)
{
    if (const auto it = m_moduleProvidersCache.find(key); it != m_moduleProvidersCache.end())
        return &(*it);
    return nullptr;
}

ModuleProviderInfo &TopLevelProjectContext::addModuleProvider(const ModuleProvidersCacheKey &key,
                                                              const ModuleProviderInfo &provider)
{
    return m_moduleProvidersCache[key] = provider;
}

void TopLevelProjectContext::addParameterDeclarations(const Item *moduleProto,
                                                      const Item::PropertyDeclarationMap &decls)
{
    std::unique_lock lock(m_parameterDeclarations.mutex);
    m_parameterDeclarations.data.insert({moduleProto, decls});
}

Item::PropertyDeclarationMap TopLevelProjectContext::parameterDeclarations(Item *moduleProto) const
{
    std::shared_lock lock(m_parameterDeclarations.mutex);
    if (const auto it = m_parameterDeclarations.data.find(moduleProto);
            it != m_parameterDeclarations.data.end()) {
        return it->second;
    }
    return {};
}

void TopLevelProjectContext::setParameters(const Item *moduleProto, const QVariantMap &parameters)
{
    std::unique_lock lock(m_parameters.mutex);
    m_parameters.data.insert({moduleProto, parameters});
}

QVariantMap TopLevelProjectContext::parameters(Item *moduleProto) const
{
    std::shared_lock lock(m_parameters.mutex);
    if (const auto it = m_parameters.data.find(moduleProto); it != m_parameters.data.end()) {
        return it->second;
    }
    return {};
}

QString TopLevelProjectContext::findModuleDirectory(
        const QualifiedId &module, const QString &searchPath,
        const std::function<QString()> &findOnDisk)
{
    std::lock_guard lock(m_modulePathCache.mutex);
    auto &path = m_modulePathCache.data[{searchPath, module}];
    if (!path)
        path = findOnDisk();
    return *path;
}

QStringList TopLevelProjectContext::getModuleFilesForDirectory(
        const QString &dir, const std::function<QStringList ()> &findOnDisk)
{
    std::lock_guard lock(m_moduleFilesPerDirectory.mutex);
    auto &list = m_moduleFilesPerDirectory.data[dir];
    if (!list)
        list = findOnDisk();
    return *list;
}

void TopLevelProjectContext::removeModuleFileFromDirectoryCache(const QString &filePath)
{
    std::lock_guard lock(m_moduleFilesPerDirectory.mutex);
    const auto it = m_moduleFilesPerDirectory.data.find(FileInfo::path(filePath));
    QBS_CHECK(it != m_moduleFilesPerDirectory.data.end());
    it->second->removeOne(filePath);
}

void TopLevelProjectContext::addUnknownProfilePropertyError(const Item *moduleProto,
                                                            const ErrorInfo &error)
{
    std::unique_lock lock(m_unknownProfilePropertyErrors.mutex);
    m_unknownProfilePropertyErrors.data[moduleProto].push_back(error);
}

const std::vector<ErrorInfo> &TopLevelProjectContext::unknownProfilePropertyErrors(
        const Item *moduleProto) const
{
    std::shared_lock lock(m_unknownProfilePropertyErrors.mutex);
    if (const auto it = m_unknownProfilePropertyErrors.data.find(moduleProto);
            it != m_unknownProfilePropertyErrors.data.end()) {
        return it->second;
    }
    static const std::vector<ErrorInfo> empty;
    return empty;
}

Item *TopLevelProjectContext::getModulePrototype(const QString &filePath, const QString &profile,
                                                 const std::function<Item *()> &produce)
{
    std::lock_guard lock(m_modulePrototypes.mutex);
    auto &prototypeList = m_modulePrototypes.data[filePath];
    for (const auto &prototype : prototypeList) {
        if (prototype.second == profile)
            return prototype.first;
    }
    Item * const module = produce();
    if (module)
        prototypeList.emplace_back(module, profile);
    return module;
}

void TopLevelProjectContext::addLocalProfile(const QString &name, const QVariantMap &values,
                                             const CodeLocation &location)
{
    if (m_localProfiles.contains(name))
        throw ErrorInfo(Tr::tr("Local profile '%1' redefined.").arg(name), location);
    m_localProfiles.insert(name, values);
}

void TopLevelProjectContext::checkForLocalProfileAsTopLevelProfile(const QString &topLevelProfile)
{
    for (auto it = m_localProfiles.cbegin(); it != m_localProfiles.cend(); ++it) {
        if (it.key() != topLevelProfile)
            continue;

        // This covers the edge case that a locally defined profile was specified as the
        // top-level profile, in which case we must invalidate the qbs module prototype that was
        // created in early setup before local profiles were handled.
        QBS_CHECK(m_modulePrototypes.data.size() == 1);
        m_modulePrototypes.data.clear();
        break;
    }
}

std::lock_guard<std::mutex> TopLevelProjectContext::probesCacheLock()
{
    return std::lock_guard<std::mutex>(m_probesMutex);
}

void TopLevelProjectContext::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    for (const ProbeConstPtr& probe : oldProbes)
        m_probesInfo.oldProjectProbes[probe->globalId()] << probe;
}

ProbeConstPtr TopLevelProjectContext::findOldProjectProbe(const QString &id,
                                                          const ProbeFilter &filter) const
{
    for (const ProbeConstPtr &oldProbe : m_probesInfo.oldProjectProbes.value(id)) {
        if (filter(oldProbe))
            return oldProbe;
    }
    return {};
}

void TopLevelProjectContext::setOldProductProbes(
        const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    m_probesInfo.oldProductProbes = oldProbes;
}

ProbeConstPtr TopLevelProjectContext::findOldProductProbe(const QString &productName,
                                                          const ProbeFilter &filter) const
{
    for (const ProbeConstPtr &oldProbe : m_probesInfo.oldProductProbes.value(productName)) {
        if (filter(oldProbe))
            return oldProbe;
    }
    return {};
}

void TopLevelProjectContext::addNewlyResolvedProbe(const ProbeConstPtr &probe)
{
    m_probesInfo.currentProbes[probe->location()] << probe;
}

ProbeConstPtr TopLevelProjectContext::findCurrentProbe(const CodeLocation &location,
                                                       const ProbeFilter &filter) const
{
    for (const ProbeConstPtr &probe : m_probesInfo.currentProbes.value(location)) {
        if (filter(probe))
            return probe;
    }
    return {};
}

void TopLevelProjectContext::collectDataFromEngine(const ScriptEngine &engine)
{
    const auto project = dynamic_cast<TopLevelProject *>(m_projects.front()->project.get());
    QBS_CHECK(project);
    project->canonicalFilePathResults.insert(engine.canonicalFilePathResults());
    project->fileExistsResults.insert(engine.fileExistsResults());
    project->directoryEntriesResults.insert(engine.directoryEntriesResults());
    project->fileLastModifiedResults.insert(engine.fileLastModifiedResults());
    project->environment.insert(engine.environment());
    project->buildSystemFiles.unite(engine.imports());
}

ItemPool &TopLevelProjectContext::createItemPool()
{
    m_itemPools.push_back(std::make_unique<ItemPool>());
    return *m_itemPools.back();
}

class LoaderState::Private
{
public:
    Private(LoaderState &q, const SetupProjectParameters &parameters,
            TopLevelProjectContext &topLevelProject, ItemPool &itemPool, ScriptEngine &engine,
            Logger &&logger)
        : parameters(parameters), topLevelProject(topLevelProject), itemPool(itemPool),
          logger(std::move(logger)), itemReader(q), evaluator(&engine)
    {
        this->logger.clearWarnings();
        this->logger.storeWarnings();
    }

    const SetupProjectParameters &parameters;
    TopLevelProjectContext &topLevelProject;
    ItemPool &itemPool;

    Logger logger;
    ItemReader itemReader;
    Evaluator evaluator;
};

LoaderState::LoaderState(const SetupProjectParameters &parameters,
                         TopLevelProjectContext &topLevelProject, ItemPool &itemPool,
                         ScriptEngine &engine, Logger logger)
    : d(makePimpl<Private>(*this, parameters, topLevelProject, itemPool, engine, std::move(logger)))
{
    d->itemReader.init();
}

LoaderState::~LoaderState() = default;
const SetupProjectParameters &LoaderState::parameters() const { return d->parameters; }
ItemPool &LoaderState::itemPool() { return d->itemPool; }
Evaluator &LoaderState::evaluator() { return d->evaluator; }
Logger &LoaderState::logger() { return d->logger; }
ItemReader &LoaderState::itemReader() { return d->itemReader; }
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

std::pair<ProductDependency, ProductContext *> ProductContext::pendingDependency() const
{
    return dependenciesContext ? dependenciesContext->pendingDependency()
                               : std::make_pair(ProductDependency::None, nullptr);
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
    resolvingProducts += other.resolvingProducts;
    probes += other.probes;
    propertyEvaluation += other.propertyEvaluation;
    propertyChecking += other.propertyChecking;
    return *this;
}

DependenciesContext::~DependenciesContext() = default;

ItemReaderCache::AstCacheEntry &ItemReaderCache::retrieveOrSetupCacheEntry(
    const QString &filePath, const std::function<void (AstCacheEntry &)> &setup)
{
    std::lock_guard lock(m_astCache.mutex);
    AstCacheEntry &entry = m_astCache.data[filePath];
    if (!entry.ast) {
        setup(entry);
        m_filesRead << filePath;
    }
    return entry;
}

const QStringList &ItemReaderCache::retrieveOrSetDirectoryEntries(
    const QString &dir, const std::function<QStringList ()> &findOnDisk)
{
    std::lock_guard lock(m_directoryEntries.mutex);
    auto &entries = m_directoryEntries.data[dir];
    if (!entries)
        entries = findOnDisk();
    return *entries;
}

bool ItemReaderCache::AstCacheEntry::addProcessingThread()
{
    std::lock_guard lock(m_processingThreads.mutex);
    return m_processingThreads.data.insert(std::this_thread::get_id()).second;
}

void ItemReaderCache::AstCacheEntry::removeProcessingThread()
{
    std::lock_guard lock(m_processingThreads.mutex);
    m_processingThreads.data.remove(std::this_thread::get_id());
}

class DependencyParametersMerger
{
public:
    DependencyParametersMerger(std::vector<Item::Module::ParametersWithPriority> &&candidates)
        : m_candidates(std::move(candidates)) { }
    QVariantMap merge();

private:
    void merge(QVariantMap &current, const QVariantMap &next, int nextPrio);

    const std::vector<Item::Module::ParametersWithPriority> m_candidates;

    struct Conflict {
        Conflict(QStringList path, QVariant val1, QVariant val2, int prio)
            : path(std::move(path)), val1(std::move(val1)), val2(std::move(val2)), priority(prio) {}
        QStringList path;
        QVariant val1;
        QVariant val2;
        int priority;
    };
    std::vector<Conflict> m_conflicts;
    QVariantMap m_currentValue;
    int m_currentPrio = INT_MIN;
    QStringList m_path;
};

QVariantMap mergeDependencyParameters(std::vector<Item::Module::ParametersWithPriority> &&candidates)
{
    return DependencyParametersMerger(std::move(candidates)).merge();
}

QVariantMap mergeDependencyParameters(const QVariantMap &m1, const QVariantMap &m2)
{
    return mergeDependencyParameters({std::make_pair(m1, 0), std::make_pair(m2, 0)});
}

QVariantMap DependencyParametersMerger::merge()
{
    for (const auto &next : m_candidates) {
        merge(m_currentValue, next.first, next.second);
        m_currentPrio = next.second;
    }

    if (!m_conflicts.empty()) {
        ErrorInfo error(Tr::tr("Conflicting parameter values encountered:"));
        for (const Conflict &conflict : m_conflicts) {
            // TODO: Location would be nice ...
            error.append(Tr::tr("  Parameter '%1' cannot be both '%2' and '%3'.")
                         .arg(conflict.path.join(QLatin1Char('.')),
                              conflict.val1.toString(), conflict.val2.toString()));
        }
        throw error;
    }

    return m_currentValue;
}

void DependencyParametersMerger::merge(QVariantMap &current, const QVariantMap &next, int nextPrio)
{
    for (auto it = next.begin(); it != next.end(); ++it) {
        m_path << it.key();
        const QVariant &newValue = it.value();
        QVariant &currentValue = current[it.key()];
        if (newValue.userType() == QMetaType::QVariantMap) {
            QVariantMap mdst = currentValue.toMap();
            merge(mdst, it.value().toMap(), nextPrio);
            currentValue = mdst;
        } else {
            if (m_currentPrio == nextPrio) {
                if (currentValue.isValid() && currentValue != newValue)
                    m_conflicts.emplace_back(m_path, currentValue, newValue, m_currentPrio);
            } else {
                removeIf(m_conflicts, [this](const Conflict &conflict) {
                    return m_path == conflict.path;
                });
            }
            currentValue = newValue;
        }
        m_path.removeLast();
    }
}

} // namespace qbs::Internal
