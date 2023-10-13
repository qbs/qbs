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

#include "productresolver.h"

#include "dependenciesresolver.h"
#include "groupshandler.h"
#include "loaderutils.h"
#include "modulepropertymerger.h"
#include "probesresolver.h"

#include <jsextensions/jsextensions.h>
#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/item.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/profiling.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <algorithm>

namespace qbs::Internal {

class PropertiesEvaluator
{
public:
    PropertiesEvaluator(ProductContext &product, LoaderState &loaderState)
        : m_product(product), m_loaderState(loaderState) {}

    QVariantMap evaluateProperties(Item *item, bool lookupPrototype, bool checkErrors);
    QVariantMap evaluateProperties(const Item *item, const Item *propertiesContainer,
                                   const QVariantMap &tmplt, bool lookupPrototype,
                                   bool checkErrors);
    void evaluateProperty(const Item *item, const QString &propName, const ValuePtr &propValue,
                          QVariantMap &result, bool checkErrors);

private:
    ProductContext &m_product;
    LoaderState &m_loaderState;
};

// Dependency resolving, Probe execution.
// Run for real products and shadow products.
class ProductResolverStage1
{
public:
    ProductResolverStage1(ProductContext &product, Deferral deferral, LoaderState &loaderState)
        : m_product(product), m_loaderState(loaderState), m_deferral(deferral) {}
    void start();

private:
    void resolveProbes();
    void resolveProbes(Item *item);
    void runModuleProbes(const Item::Module &module);
    void updateModulePresentState(const Item::Module &module);
    bool validateModule(const Item::Module &module);
    void handleModuleSetupError(const Item::Module &module, const ErrorInfo &error);
    void checkPropertyDeclarations();
    void mergeDependencyParameters();
    void checkDependencyParameterDeclarations(const Item *productItem,
                                              const QString &productName) const;

    ProductContext &m_product;
    LoaderState &m_loaderState;
    const Deferral m_deferral;
};

// Setting up ResolvedProduct, incuding property evaluation and handling Product child items.
// Run only for real products.
class ProductResolverStage2
{
public:
    ProductResolverStage2(ProductContext &product, LoaderState &loaderState)
        : m_product(product), m_loaderState(loaderState) {}
    void start();

private:
    void resolveProductFully();
    void createProductConfig();
    void resolveGroup(Item *item);
    void resolveGroupFully(Item *item, bool isEnabled);
    QVariantMap resolveAdditionalModuleProperties(const Item *group,
                                                  const QVariantMap &currentValues);
    SourceArtifactPtr createSourceArtifact(const QString &fileName, const GroupPtr &group,
            bool wildcard, const CodeLocation &filesLocation,
            ErrorInfo *errorInfo);
    void resolveExport(Item *exportItem);
    std::unique_ptr<ExportedItem> resolveExportChild(const Item *item,
                                                     const ExportedModule &module);
    void setupExportedProperties(const Item *item, const QString &namePrefix,
                                 std::vector<ExportedProperty> &properties);
    QVariantMap evaluateModuleValues(Item *item, bool lookupPrototype = true);

    void resolveScanner(Item *item, ModuleContext &moduleContext);
    void resolveModules();
    void resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                       const QVariantMap &parameters, JobLimits &jobLimits);
    void applyFileTaggers();
    void finalizeArtifactProperties();
    void collectProductDependencies();

    ProductContext &m_product;
    LoaderState &m_loaderState;
    GroupConstPtr m_currentGroup;
    FileLocations m_sourceArtifactLocations;
    PropertiesEvaluator m_propertiesEvaluator{m_product, m_loaderState};

    using ArtifactPropertiesInfo = std::pair<ArtifactPropertiesPtr, std::vector<CodeLocation>>;
    QHash<QStringList, ArtifactPropertiesInfo> m_artifactPropertiesPerFilter;
};

class ExportsResolver
{
public:
    ExportsResolver(ProductContext &product, LoaderState &loaderState)
        : m_product(product), m_loaderState(loaderState) {}
    void start();

private:
    void resolveShadowProduct();
    void collectPropertiesForExportItem(Item *productModuleInstance);
    void collectPropertiesForExportItem(const QualifiedId &moduleName, const ValuePtr &value,
                                        Item *moduleInstance, QVariantMap &moduleProps);
    void collectPropertiesForModuleInExportItem(const Item::Module &module);
    void adaptExportedPropertyValues();
    void collectExportedProductDependencies();

    ProductContext &m_product;
    LoaderState &m_loaderState;
    PropertiesEvaluator m_propertiesEvaluator{m_product, m_loaderState};
};

void resolveProduct(ProductContext &product, Deferral deferral, LoaderState &loaderState)
{
    try {
        ProductResolverStage1(product, deferral, loaderState).start();
    } catch (const ErrorInfo &err) {
        if (err.isCancelException()) {
            loaderState.topLevelProject().setCanceled();
            return;
        }
        product.handleError(err);
    }

    if (product.dependenciesResolvingPending())
        return;

    if (product.name.startsWith(StringConstants::shadowProductPrefix()))
        return;

    // TODO: The weird double-forwarded error handling can hopefully be simplified now.
    try {
        ProductResolverStage2(product, loaderState).start();
    } catch (const ErrorInfo &err) {
        if (err.isCancelException()) {
            loaderState.topLevelProject().setCanceled();
            return;
        }
        loaderState.topLevelProject().addQueuedError(err);
    }
}

void setupExports(ProductContext &product, LoaderState &loaderState)
{
    ExportsResolver(product, loaderState).start();
}

void ProductResolverStage1::start()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();
    topLevelProject.checkCancelation();

    if (m_product.delayedError.hasError())
        return;

    resolveDependencies(m_product, m_deferral, m_loaderState);
    QBS_CHECK(m_product.dependenciesContext);
    if (!m_product.dependenciesContext->dependenciesResolved)
        return;
    if (m_product.delayedError.hasError()
            && m_loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict) {
        return;
    }

    // Run probes for modules and product.
    resolveProbes();

    // After the probes have run, we can switch on the evaluator cache.
    Evaluator &evaluator = m_loaderState.evaluator();
    FileTags fileTags = evaluator.fileTagsValue(m_product.item, StringConstants::typeProperty());
    EvalCacheEnabler cacheEnabler(&evaluator, evaluator.stringValue(
                                      m_product.item,
                                      StringConstants::sourceDirectoryProperty()));

    // Run module validation scripts.
    for (const Item::Module &module : m_product.item->modules()) {
        if (!validateModule(module))
            return;
        fileTags += evaluator.fileTagsValue(
            module.item, StringConstants::additionalProductTypesProperty());
    }

    // Disable modules that have been pulled in only by now-disabled modules.
    // Note that this has to happen in the reverse order compared to the other loops,
    // with the leaves checked last.
    for (auto it = m_product.item->modules().rbegin(); it != m_product.item->modules().rend(); ++it)
        updateModulePresentState(*it);

    // Now do the canonical module property values merge. Note that this will remove
    // previously attached values from modules that failed validation.
    // Evaluator cache entries that could potentially change due to this will be purged.
    doFinalMerge(m_product, m_loaderState);

    const bool enabled = topLevelProject.checkItemCondition(m_product.item, evaluator);

    mergeDependencyParameters();
    checkDependencyParameterDeclarations(m_product.item, m_product.name);

    setupGroups(m_product, m_loaderState);

    // Collect the full list of fileTags, including the values contributed by modules.
    if (!m_product.delayedError.hasError() && enabled
        && !m_product.name.startsWith(StringConstants::shadowProductPrefix())) {
        topLevelProject.addProductByType(m_product, fileTags);
        m_product.item->setProperty(StringConstants::typeProperty(),
                                    VariantValue::create(sorted(fileTags.toStringList())));
    }

    checkPropertyDeclarations();
}

void ProductResolverStage1::resolveProbes()
{
    for (const Item::Module &module : m_product.item->modules()) {
        runModuleProbes(module);
        if (m_product.delayedError.hasError())
            return;
    }
    resolveProbes(m_product.item);
}

void ProductResolverStage1::resolveProbes(Item *item)
{
    ProbesResolver(m_loaderState).resolveProbes(m_product, item);
}

void ProductResolverStage1::runModuleProbes(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    if (module.product && m_loaderState.topLevelProject().isDisabledItem(module.product->item)) {
        createNonPresentModule(m_loaderState.itemPool(), module.name.toString(),
                               QLatin1String("module's exporting product is disabled"),
                               module.item);
        return;
    }
    try {
        resolveProbes(module.item);
        if (module.versionRange.minimum.isValid()
            || module.versionRange.maximum.isValid()) {
            if (module.versionRange.maximum.isValid()
                && module.versionRange.minimum >= module.versionRange.maximum) {
                throw ErrorInfo(Tr::tr("Impossible version constraint [%1,%2) set for module "
                                       "'%3'").arg(module.versionRange.minimum.toString(),
                                         module.versionRange.maximum.toString(),
                                         module.name.toString()));
            }
            const Version moduleVersion = Version::fromString(
                        m_loaderState.evaluator().stringValue(module.item,
                                                              StringConstants::versionProperty()));
            if (moduleVersion < module.versionRange.minimum) {
                throw ErrorInfo(Tr::tr("Module '%1' has version %2, but it needs to be "
                                       "at least %3.").arg(module.name.toString(),
                                         moduleVersion.toString(),
                                         module.versionRange.minimum.toString()));
            }
            if (module.versionRange.maximum.isValid()
                && moduleVersion >= module.versionRange.maximum) {
                throw ErrorInfo(Tr::tr("Module '%1' has version %2, but it needs to be "
                                       "lower than %3.").arg(module.name.toString(),
                                         moduleVersion.toString(),
                                         module.versionRange.maximum.toString()));
            }
        }
    } catch (const ErrorInfo &error) {
        handleModuleSetupError(module, error);
    }
}

void ProductResolverStage1::updateModulePresentState(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    bool hasPresentLoadingItem = false;
    for (const auto &loadingItemInfo : module.loadingItems) {
        const Item * const loadingItem = loadingItemInfo.first;
        if (loadingItem == m_product.item) {
            hasPresentLoadingItem = true;
            break;
        }
        if (!loadingItem->isPresentModule())
            continue;
        if (loadingItem->prototype() && loadingItem->prototype()->type() == ItemType::Export) {
            QBS_CHECK(loadingItem->prototype()->parent()->type() == ItemType::Product);
            if (m_loaderState.topLevelProject().isDisabledItem(loadingItem->prototype()->parent()))
                continue;
        }
        hasPresentLoadingItem = true;
        break;
    }
    if (!hasPresentLoadingItem) {
        createNonPresentModule(m_loaderState.itemPool(), module.name.toString(),
                               QLatin1String("imported only by disabled module(s)"),
                               module.item);
    }
}

bool ProductResolverStage1::validateModule(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return true;
    try {
        m_loaderState.evaluator().boolValue(module.item, StringConstants::validateProperty());
        for (const auto &dep : module.item->modules()) {
            if (dep.required && !dep.item->isPresentModule()) {
                throw ErrorInfo(Tr::tr("Module '%1' depends on module '%2', which was not "
                                       "loaded successfully")
                                    .arg(module.name.toString(), dep.name.toString()));
            }
        }
    } catch (const ErrorInfo &error) {
        handleModuleSetupError(module, error);
        if (m_product.delayedError.hasError())
            return false;
    }
    return true;
}

void ProductResolverStage1::handleModuleSetupError(const Item::Module &module,
                                                   const ErrorInfo &error)
{
    if (module.required) {
        m_product.handleError(error);
    } else {
        qCDebug(lcModuleLoader()) << "non-required module" << module.name.toString()
                                  << "found, but not usable in product" << m_product.name
                                  << error.toString();
        createNonPresentModule(m_loaderState.itemPool(), module.name.toString(),
                               QStringLiteral("failed validation"), module.item);
    }
}

void ProductResolverStage1::checkPropertyDeclarations()
{
    AccumulatingTimer timer(m_loaderState.parameters().logElapsedTime()
                            ? &m_product.timingData.propertyChecking : nullptr);
    qbs::Internal::checkPropertyDeclarations(m_product.item, m_loaderState);
}

void ProductResolverStage1::mergeDependencyParameters()
{
    if (m_loaderState.topLevelProject().isDisabledItem(m_product.item))
        return;

    for (Item::Module &module : m_product.item->modules()) {
        if (module.name.toString() == StringConstants::qbsModule())
            continue;
        if (!module.item->isPresentModule())
            continue;

        using PP = Item::Module::ParametersWithPriority;
        std::vector<PP> priorityList;
        const QVariantMap defaultParameters = module.product
                ? module.product->defaultParameters
                : m_loaderState.topLevelProject().parameters(module.item->prototype());
        priorityList.emplace_back(defaultParameters, INT_MIN);
        for (const Item::Module::LoadingItemInfo &info : module.loadingItems) {
            const QVariantMap &parameters = info.second.first;

            // Empty parameter maps and inactive loading modules do not contribute to the
            // final parameter map.
            if (parameters.isEmpty())
                continue;
            if (info.first->type() == ItemType::ModuleInstance && !info.first->isPresentModule())
                continue;

            // Build a list sorted by priority.
            static const auto cmp = [](const PP &elem, int prio) { return elem.second < prio; };
            const auto it = std::lower_bound(priorityList.begin(), priorityList.end(),
                                             info.second.second, cmp);
            priorityList.insert(it, info.second);
        }

        module.parameters = qbs::Internal::mergeDependencyParameters(std::move(priorityList));
    }
}

class DependencyParameterDeclarationCheck
{
public:
    DependencyParameterDeclarationCheck(const QString &productName, const Item *productItem,
                                        const TopLevelProjectContext &topLevelProject)
        : m_productName(productName), m_productItem(productItem), m_topLevelProject(topLevelProject)
    {}

    void operator()(const QVariantMap &parameters) const { check(parameters, QualifiedId()); }

private:
    void check(const QVariantMap &parameters, const QualifiedId &moduleName) const
    {
        for (auto it = parameters.begin(); it != parameters.end(); ++it) {
            if (it.value().userType() == QMetaType::QVariantMap) {
                check(it.value().toMap(), QualifiedId(moduleName) << it.key());
            } else {
                const auto &deps = m_productItem->modules();
                auto m = std::find_if(deps.begin(), deps.end(),
                                      [&moduleName] (const Item::Module &module) {
                                          return module.name == moduleName;
                                      });

                if (m == deps.end()) {
                    const QualifiedId fullName = QualifiedId(moduleName) << it.key();
                    throw ErrorInfo(Tr::tr("Cannot set parameter '%1', "
                                           "because '%2' does not have a dependency on '%3'.")
                                        .arg(fullName.toString(), m_productName, moduleName.toString()),
                                    m_productItem->location());
                }

                const auto decls = m_topLevelProject.parameterDeclarations(
                            m->item->rootPrototype());
                if (!decls.contains(it.key())) {
                    const QualifiedId fullName = QualifiedId(moduleName) << it.key();
                    throw ErrorInfo(Tr::tr("Parameter '%1' is not declared.")
                                        .arg(fullName.toString()), m_productItem->location());
                }
            }
        }
    }

    bool moduleExists(const QualifiedId &name) const
    {
        const auto &deps = m_productItem->modules();
        return any_of(deps, [&name](const Item::Module &module) {
            return module.name == name;
        });
    }

    const QString &m_productName;
    const Item * const m_productItem;
    const TopLevelProjectContext &m_topLevelProject;
};

void ProductResolverStage1::checkDependencyParameterDeclarations(const Item *productItem,
                                                                 const QString &productName) const
{
    DependencyParameterDeclarationCheck dpdc(productName, productItem,
                                             m_loaderState.topLevelProject());
    for (const Item::Module &dep : productItem->modules()) {
        if (!dep.parameters.empty())
            dpdc(dep.parameters);
    }
}


void ProductResolverStage2::start()
{
    m_loaderState.evaluator().clearPropertyDependencies();

    ResolvedProductPtr product = ResolvedProduct::create();
    product->enabled = m_product.project->project->enabled;
    product->moduleProperties = PropertyMapInternal::create();
    product->project = m_product.project->project;
    m_product.product = product;
    product->location = m_product.item->location();
    const auto errorFromDelayedError = [&] {
        if (m_product.delayedError.hasError()) {
            ErrorInfo errorInfo;

            // First item is "main error", gets prepended again in the catch clause.
            const QList<ErrorItem> &items = m_product.delayedError.items();
            for (int i = 1; i < items.size(); ++i)
                errorInfo.append(items.at(i));

            m_product.delayedError.clear();
            return errorInfo;
        }
        return ErrorInfo();
    };

    // Even if we previously encountered an error, try to continue for as long as possible
    // to provide IDEs with useful data (e.g. the list of files).
    // If we encounter a follow-up error, suppress it and report the original one instead.
    try {
        resolveProductFully();
        if (const ErrorInfo error = errorFromDelayedError(); error.hasError())
            throw error;
    } catch (ErrorInfo e) {
        if (const ErrorInfo error = errorFromDelayedError(); error.hasError())
            e = error;
        QString mainErrorString = !product->name.isEmpty()
                ? Tr::tr("Error while handling product '%1':").arg(product->name)
                : Tr::tr("Error while handling product:");
        ErrorInfo fullError(mainErrorString, product->location);
        appendError(fullError, e);
        if (!product->enabled) {
            qCDebug(lcProjectResolver) << fullError.toString();
            return;
        }
        if (m_loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
            throw fullError;
        m_loaderState.logger().printWarning(fullError);
        m_loaderState.logger().printWarning(
                    ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                              .arg(product->name), product->location));
        product->enabled = false;
    }
}

void ProductResolverStage2::resolveProductFully()
{
    Item * const item = m_product.item;
    const ResolvedProductPtr product = m_product.product;
    Evaluator &evaluator = m_loaderState.evaluator();
    product->name = evaluator.stringValue(item, StringConstants::nameProperty());

    // product->buildDirectory() isn't valid yet, because the productProperties map is not ready.
    m_product.buildDirectory = evaluator.stringValue(
                item, StringConstants::buildDirectoryProperty());
    product->multiplexConfigurationId = evaluator.stringValue(
                item, StringConstants::multiplexConfigurationIdProperty());
    qCDebug(lcProjectResolver) << "resolveProduct" << product->uniqueName();
    product->enabled = product->enabled
            && evaluator.boolValue(item, StringConstants::conditionProperty());
    const VariantValuePtr type = item->variantProperty(StringConstants::typeProperty());
    if (type)
        product->fileTags = FileTags::fromStringList(type->value().toStringList());
    product->targetName = evaluator.stringValue(item, StringConstants::targetNameProperty());
    product->sourceDirectory = evaluator.stringValue(
                item, StringConstants::sourceDirectoryProperty());
    product->destinationDirectory = evaluator.stringValue(
                item, StringConstants::destinationDirProperty());

    if (product->destinationDirectory.isEmpty()) {
        product->destinationDirectory = m_product.buildDirectory;
    } else {
        product->destinationDirectory = FileInfo::resolvePath(
                    product->topLevelProject()->buildDirectory,
                    product->destinationDirectory);
    }
    product->probes = m_product.probes;
    createProductConfig();
    product->productProperties.insert(StringConstants::destinationDirProperty(),
                                      product->destinationDirectory);
    ModuleProperties::init(evaluator.engine(), evaluator.scriptValue(item), product.get());

    QList<Item *> subItems = item->children();
    const ValuePtr filesProperty = item->property(StringConstants::filesProperty());
    if (filesProperty) {
        Item *fakeGroup = Item::create(&m_loaderState.itemPool(), ItemType::Group);
        fakeGroup->setFile(item->file());
        fakeGroup->setLocation(item->location());
        fakeGroup->setScope(item);
        fakeGroup->setProperty(StringConstants::nameProperty(), VariantValue::create(product->name));
        fakeGroup->setProperty(StringConstants::filesProperty(), filesProperty);
        fakeGroup->setProperty(StringConstants::excludeFilesProperty(),
                               item->property(StringConstants::excludeFilesProperty()));
        fakeGroup->setProperty(StringConstants::overrideTagsProperty(),
                               VariantValue::falseValue());
        fakeGroup->setupForBuiltinType(m_loaderState.parameters().deprecationWarningMode(),
                                       m_loaderState.logger());
        subItems.prepend(fakeGroup);
    }

    for (Item * const child : std::as_const(subItems)) {
        switch (child->type()) {
        case ItemType::Rule:
            resolveRule(m_loaderState, child, m_product.project, &m_product, nullptr);
            break;
        case ItemType::FileTagger:
            resolveFileTagger(m_loaderState, child, nullptr, &m_product);
            break;
        case ItemType::JobLimit:
            resolveJobLimit(m_loaderState, child, nullptr, &m_product, nullptr);
            break;
        case ItemType::Group:
            resolveGroup(child);
            break;
        case ItemType::Export:
            resolveExport(child);
            break;
        default:
            break;
        }
    }

    for (const ProjectContext *p = m_product.project; p; p = p->parent) {
        JobLimits tempLimits = p->jobLimits;
        product->jobLimits = tempLimits.update(product->jobLimits);
    }

    resolveModules();
    applyFileTaggers();
    finalizeArtifactProperties();

    for (const RulePtr &rule : m_product.project->rules) {
        RulePtr clonedRule = rule->clone();
        clonedRule->product = product.get();
        product->rules.push_back(clonedRule);
    }

    collectProductDependencies();
}

void ProductResolverStage2::createProductConfig()
{
    EvalCacheEnabler cachingEnabler(&m_loaderState.evaluator(),
                                    m_product.product->sourceDirectory);
    m_product.product->moduleProperties->setValue(evaluateModuleValues(m_product.item));
    m_product.product->productProperties = m_propertiesEvaluator.evaluateProperties(
        m_product.item, m_product.item, QVariantMap(), true, true);
}

void ProductResolverStage2::resolveGroup(Item *item)
{
    const bool parentEnabled = m_currentGroup ? m_currentGroup->enabled
                                              : m_product.product->enabled;
    const bool isEnabled = parentEnabled
            && m_loaderState.evaluator().boolValue(item, StringConstants::conditionProperty());
    try {
        resolveGroupFully(item, isEnabled);
    } catch (const ErrorInfo &error) {
        if (!isEnabled) {
            qCDebug(lcProjectResolver) << "error resolving group at" << item->location()
                                       << error.toString();
            return;
        }
        if (m_loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        m_loaderState.logger().printWarning(error);
    }
}

void ProductResolverStage2::resolveGroupFully(Item *item, bool isEnabled)
{
    AccumulatingTimer groupTimer(m_loaderState.parameters().logElapsedTime()
                                 ? &m_product.timingData.groupsResolving
                                 : nullptr);

    const auto getGroupPropertyMap = [&](const ArtifactProperties *existingProps) {
        PropertyMapPtr moduleProperties;
        bool newPropertyMapRequired = false;
        if (existingProps)
            moduleProperties = existingProps->propertyMap();
        if (!moduleProperties) {
            newPropertyMapRequired = true;
            moduleProperties = m_currentGroup
                    ? m_currentGroup->properties
                    : m_product.product->moduleProperties;
        }
        const QVariantMap newModuleProperties = resolveAdditionalModuleProperties(
                    item, moduleProperties->value());
        if (!newModuleProperties.empty()) {
            if (newPropertyMapRequired)
                moduleProperties = PropertyMapInternal::create();
            moduleProperties->setValue(newModuleProperties);
        }
        return moduleProperties;
    };

    Evaluator &evaluator = m_loaderState.evaluator();
    QStringList files = evaluator.stringListValue(item, StringConstants::filesProperty());
    bool fileTagsSet;
    const FileTags fileTags = evaluator.fileTagsValue(item, StringConstants::fileTagsProperty(),
                                                      &fileTagsSet);
    const QStringList fileTagsFilter
        = evaluator.stringListValue(item, StringConstants::fileTagsFilterProperty());
    if (!fileTagsFilter.empty()) {
        if (Q_UNLIKELY(!files.empty()))
            throw ErrorInfo(Tr::tr("Group.files and Group.fileTagsFilters are exclusive."),
                        item->location());

        if (!isEnabled)
            return;

        ArtifactPropertiesInfo &apinfo = m_artifactPropertiesPerFilter[fileTagsFilter];
        if (apinfo.first) {
            const auto it = std::find_if(apinfo.second.cbegin(), apinfo.second.cend(),
                                         [item](const CodeLocation &loc) {
                return item->location().filePath() == loc.filePath();
            });
            if (it != apinfo.second.cend()) {
                ErrorInfo error(Tr::tr("Conflicting fileTagsFilter in Group items."));
                error.append(Tr::tr("First item"), *it);
                error.append(Tr::tr("Second item"), item->location());
                throw error;
            }
        } else {
            apinfo.first = ArtifactProperties::create();
            apinfo.first->setFileTagsFilter(FileTags::fromStringList(fileTagsFilter));
            m_product.product->artifactProperties.push_back(apinfo.first);
        }
        apinfo.second.push_back(item->location());
        apinfo.first->setPropertyMapInternal(getGroupPropertyMap(apinfo.first.get()));
        apinfo.first->addExtraFileTags(fileTags);
        return;
    }
    QStringList patterns;
    for (int i = files.size(); --i >= 0;) {
        if (FileInfo::isPattern(files[i]))
            patterns.push_back(files.takeAt(i));
    }
    GroupPtr group = ResolvedGroup::create();
    bool prefixWasSet = false;
    group->prefix = evaluator.stringValue(item, StringConstants::prefixProperty(), QString(),
                                          &prefixWasSet);
    if (!prefixWasSet && m_currentGroup)
        group->prefix = m_currentGroup->prefix;
    if (!group->prefix.isEmpty()) {
        for (auto it = files.rbegin(), end = files.rend(); it != end; ++it)
            it->prepend(group->prefix);
    }
    group->location = item->location();
    group->enabled = isEnabled;
    group->properties = getGroupPropertyMap(nullptr);
    group->fileTags = fileTags;
    group->overrideTags = evaluator.boolValue(item, StringConstants::overrideTagsProperty());
    if (group->overrideTags && fileTagsSet) {
        if (group->fileTags.empty() )
            group->fileTags.insert(unknownFileTag());
    } else if (m_currentGroup) {
        group->fileTags.unite(m_currentGroup->fileTags);
    }

    const CodeLocation filesLocation = item->property(StringConstants::filesProperty())->location();
    const VariantValueConstPtr moduleProp = item->variantProperty(
                StringConstants::modulePropertyInternal());
    if (moduleProp)
        group->targetOfModule = moduleProp->value().toString();
    ErrorInfo fileError;
    if (!patterns.empty()) {
        group->wildcards = std::make_unique<SourceWildCards>();
        SourceWildCards *wildcards = group->wildcards.get();
        wildcards->group = group.get();
        wildcards->excludePatterns = evaluator.stringListValue(
            item, StringConstants::excludeFilesProperty());
        wildcards->patterns = patterns;
        const Set<QString> files = wildcards->expandPatterns(group,
                FileInfo::path(item->file()->filePath()),
                m_product.project->project->topLevelProject()->buildDirectory);
        for (const QString &fileName : files)
            createSourceArtifact(fileName, group, true, filesLocation, &fileError);
    }

    for (const QString &fileName : std::as_const(files))
        createSourceArtifact(fileName, group, false, filesLocation, &fileError);
    if (fileError.hasError()) {
        if (group->enabled) {
            if (m_loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
                throw ErrorInfo(fileError);
            m_loaderState.logger().printWarning(fileError);
        } else {
            qCDebug(lcProjectResolver) << "error for disabled group:" << fileError.toString();
        }
    }
    group->name = evaluator.stringValue(item, StringConstants::nameProperty());
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(m_product.product->groups.size());
    m_product.product->groups.push_back(group);

    class GroupContextSwitcher {
    public:
        GroupContextSwitcher(ProductResolverStage2 &resolver, const GroupConstPtr &newGroup)
            : m_resolver(resolver), m_oldGroup(resolver.m_currentGroup) {
            resolver.m_currentGroup = newGroup;
        }
        ~GroupContextSwitcher() { m_resolver.m_currentGroup = m_oldGroup; }
    private:
        ProductResolverStage2 &m_resolver;
        const GroupConstPtr m_oldGroup;
    };
    GroupContextSwitcher groupSwitcher(*this, group);
    for (Item * const childItem : item->children())
        resolveGroup(childItem);
}

SourceArtifactPtr ProductResolverStage2::createSourceArtifact(
        const QString &fileName, const GroupPtr &group, bool wildcard,
        const CodeLocation &filesLocation, ErrorInfo *errorInfo)
{
    const QString &baseDir = FileInfo::path(group->location.filePath());
    const QString absFilePath = QDir::cleanPath(FileInfo::resolvePath(baseDir, fileName));
    if (!wildcard && !FileInfo(absFilePath).exists()) {
        if (errorInfo)
            errorInfo->append(Tr::tr("File '%1' does not exist.").arg(absFilePath), filesLocation);
        m_product.product->missingSourceFiles << absFilePath;
        return {};
    }
    if (group->enabled) {
        CodeLocation &loc = m_sourceArtifactLocations[
                std::make_pair(group->targetOfModule, absFilePath)];
        if (loc.isValid()) {
            if (errorInfo) {
                errorInfo->append(Tr::tr("Duplicate source file '%1'.").arg(absFilePath));
                errorInfo->append(Tr::tr("First occurrence is here."), loc);
                errorInfo->append(Tr::tr("Next occurrence is here."), filesLocation);
            }
            return {};
        }
        loc = filesLocation;
    }
    SourceArtifactPtr artifact = SourceArtifactInternal::create();
    artifact->absoluteFilePath = absFilePath;
    artifact->fileTags = group->fileTags;
    artifact->overrideFileTags = group->overrideTags;
    artifact->properties = group->properties;
    artifact->targetOfModule = group->targetOfModule;
    (wildcard ? group->wildcards->files : group->files).push_back(artifact);
    return artifact;
}

static QualifiedIdSet propertiesToEvaluate(std::deque<QualifiedId> initialProps,
                                           const PropertyDependencies &deps)
{
    std::deque<QualifiedId> remainingProps = std::move(initialProps);
    QualifiedIdSet allProperties;
    while (!remainingProps.empty()) {
        const QualifiedId prop = remainingProps.front();
        remainingProps.pop_front();
        const auto insertResult = allProperties.insert(prop);
        if (!insertResult.second)
            continue;
        transform(deps.value(prop), remainingProps, [](const QualifiedId &id) { return id; });
    }
    return allProperties;
}

QVariantMap ProductResolverStage2::resolveAdditionalModuleProperties(
        const Item *group, const QVariantMap &currentValues)
{
    // Step 1: Retrieve the properties directly set in the group
    const ModulePropertiesPerGroup &mp = m_product.modulePropertiesSetInGroups;
    const auto it = mp.find(group);
    if (it == mp.end())
        return {};
    const QualifiedIdSet &propsSetInGroup = it->second;

    // Step 2: Gather all properties that depend on these properties.
    const QualifiedIdSet &propsToEval = propertiesToEvaluate(
        rangeTo<std::deque<QualifiedId>>(propsSetInGroup),
                m_loaderState.evaluator().propertyDependencies());

    // Step 3: Evaluate all these properties and replace their values in the map
    QVariantMap modulesMap = currentValues;
    QHash<QString, QStringList> propsPerModule;
    for (auto fullPropName : propsToEval) {
        const QString moduleName
                = QualifiedId(fullPropName.mid(0, fullPropName.size() - 1)).toString();
        propsPerModule[moduleName] << fullPropName.last();
    }
    EvalCacheEnabler cachingEnabler(&m_loaderState.evaluator(),
                                    m_product.product->sourceDirectory);
    for (const Item::Module &module : group->modules()) {
        const QString &fullModName = module.name.toString();
        const QStringList propsForModule = propsPerModule.take(fullModName);
        if (propsForModule.empty())
            continue;
        QVariantMap reusableValues = modulesMap.value(fullModName).toMap();
        for (const QString &prop : std::as_const(propsForModule))
            reusableValues.remove(prop);
        modulesMap.insert(fullModName, m_propertiesEvaluator.evaluateProperties(
                                           module.item, module.item, reusableValues, true, true));
    }
    return modulesMap;
}

static QString getLineAtLocation(const CodeLocation &loc, const QString &content)
{
    int pos = 0;
    int currentLine = 1;
    while (currentLine < loc.line()) {
        while (content.at(pos++) != QLatin1Char('\n'))
            ;
        ++currentLine;
    }
    const int eolPos = content.indexOf(QLatin1Char('\n'), pos);
    return content.mid(pos, eolPos - pos);
}

static bool usesImport(const ExportedProperty &prop, const QRegularExpression &regex)
{
    return prop.sourceCode.indexOf(regex) != -1;
}

static bool usesImport(const ExportedItem &item, const QRegularExpression &regex)
{
    return any_of(item.properties,
                  [regex](const ExportedProperty &p) { return usesImport(p, regex); })
            || any_of(item.children,
                      [regex](const ExportedItemPtr &child) { return usesImport(*child, regex); });
}

static bool usesImport(const ExportedModule &module, const QString &name)
{
    // Imports are used in three ways:
    // (1) var f = new TextFile(...);
    // (2) var path = FileInfo.joinPaths(...)
    // (3) var obj = DataCollection;
    const QString pattern = QStringLiteral("\\b%1\\b");

    const QRegularExpression regex(pattern.arg(name)); // std::regex is much slower
    return any_of(module.m_properties,
                  [regex](const ExportedProperty &p) { return usesImport(p, regex); })
            || any_of(module.children,
                      [regex](const ExportedItemPtr &child) { return usesImport(*child, regex); });
}

void ProductResolverStage2::resolveExport(Item *exportItem)
{
    ExportedModule &exportedModule = m_product.product->exportedModule;
    setupExportedProperties(exportItem, QString(), exportedModule.m_properties);
    static const auto cmpFunc = [](const ExportedProperty &p1, const ExportedProperty &p2) {
        return p1.fullName < p2.fullName;
    };
    std::sort(exportedModule.m_properties.begin(), exportedModule.m_properties.end(), cmpFunc);

    transform(exportItem->children(), exportedModule.children,
              [&exportedModule, this](const auto &child) {
        return resolveExportChild(child, exportedModule); });

    for (const JsImport &jsImport : exportItem->file()->jsImports()) {
        if (usesImport(exportedModule, jsImport.scopeName)) {
            exportedModule.importStatements << getLineAtLocation(jsImport.location,
                                                                 exportItem->file()->content());
        }
    }
    const auto builtInImports = JsExtensions::extensionNames();
    for (const QString &builtinImport: builtInImports) {
        if (usesImport(exportedModule, builtinImport))
            exportedModule.importStatements << QStringLiteral("import qbs.") + builtinImport;
    }
    exportedModule.importStatements.sort();
}

// TODO: This probably wouldn't be necessary if we had item serialization.
std::unique_ptr<ExportedItem> ProductResolverStage2::resolveExportChild(
        const Item *item, const ExportedModule &module)
{
    std::unique_ptr<ExportedItem> exportedItem(new ExportedItem);

    // This is the type of the built-in base item. It may turn out that we need to support
    // derived items under Export. In that case, we probably need a new Item member holding
    // the original type name.
    exportedItem->name = item->typeName();

    transform(item->children(), exportedItem->children, [&module, this](const auto &child) {
        return resolveExportChild(child, module); });

    setupExportedProperties(item, QString(), exportedItem->properties);
    return exportedItem;
}

void ProductResolverStage2::setupExportedProperties(const Item *item, const QString &namePrefix,
                                                    std::vector<ExportedProperty> &properties)
{
    const auto &props = item->properties();
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        const QString qualifiedName = namePrefix.isEmpty()
                ? it.key() : namePrefix + QLatin1Char('.') + it.key();
        if ((item->type() == ItemType::Export || item->type() == ItemType::Properties)
                && qualifiedName == StringConstants::prefixMappingProperty()) {
            continue;
        }
        const ValuePtr &v = it.value();
        if (v->type() == Value::ItemValueType) {
            setupExportedProperties(std::static_pointer_cast<ItemValue>(v)->item(),
                                    qualifiedName, properties);
            continue;
        }
        ExportedProperty exportedProperty;
        exportedProperty.fullName = qualifiedName;
        exportedProperty.type = item->propertyDeclaration(it.key()).type();
        if (v->type() == Value::VariantValueType) {
            exportedProperty.sourceCode = toJSLiteral(
                        std::static_pointer_cast<VariantValue>(v)->value());
        } else {
            QBS_CHECK(v->type() == Value::JSSourceValueType);
            const JSSourceValue * const sv = static_cast<JSSourceValue *>(v.get());
            exportedProperty.sourceCode = sv->sourceCode().toString();
        }
        const ItemDeclaration itemDecl
                = BuiltinDeclarations::instance().declarationsForType(item->type());
        PropertyDeclaration propertyDecl;
        const auto itemProperties = itemDecl.properties();
        for (const PropertyDeclaration &decl : itemProperties) {
            if (decl.name() == it.key()) {
                propertyDecl = decl;
                exportedProperty.isBuiltin = true;
                break;
            }
        }

        // Do not add built-in properties that were left at their default value.
        if (!exportedProperty.isBuiltin
                || m_loaderState.evaluator().isNonDefaultValue(item, it.key())) {
            properties.push_back(exportedProperty);
        }
    }

    // Order the list of properties, so the output won't look so random.
    static const auto less = [](const ExportedProperty &p1, const ExportedProperty &p2) -> bool {
        const int p1ComponentCount = p1.fullName.count(QLatin1Char('.'));
        const int p2ComponentCount = p2.fullName.count(QLatin1Char('.'));
        if (p1.isBuiltin && !p2.isBuiltin)
            return true;
        if (!p1.isBuiltin && p2.isBuiltin)
            return false;
        if (p1ComponentCount < p2ComponentCount)
            return true;
        if (p1ComponentCount > p2ComponentCount)
            return false;
        return p1.fullName < p2.fullName;
    };
    std::sort(properties.begin(), properties.end(), less);
}

QVariantMap ProductResolverStage2::evaluateModuleValues(Item *item, bool lookupPrototype)
{
    QVariantMap moduleValues;
    for (const Item::Module &module : item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        const QString fullName = module.name.toString();
        moduleValues[fullName] = m_propertiesEvaluator.evaluateProperties(
            module.item, lookupPrototype, true);
    }
    return moduleValues;
}

void ProductResolverStage2::resolveScanner(Item *item, ModuleContext &moduleContext)
{
    Evaluator &evaluator = m_loaderState.evaluator();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty())) {
        qCDebug(lcProjectResolver) << "scanner condition is false";
        return;
    }

    ResolvedScannerPtr scanner = ResolvedScanner::create();
    scanner->module = moduleContext.module;
    scanner->inputs = evaluator.fileTagsValue(item, StringConstants::inputsProperty());
    scanner->recursive = evaluator.boolValue(item, StringConstants::recursiveProperty());
    scanner->searchPathsScript.initialize(m_loaderState.topLevelProject().scriptFunctionValue(
                                              item, StringConstants::searchPathsProperty()));
    scanner->scanScript.initialize(m_loaderState.topLevelProject().scriptFunctionValue(
                                       item, StringConstants::scanProperty()));
    m_product.product->scanners.push_back(scanner);
}

void ProductResolverStage2::resolveModules()
{
    JobLimits jobLimits;
    for (const Item::Module &m : m_product.item->modules())
        resolveModule(m.name, m.item, m.product, m.parameters, jobLimits);
    for (int i = 0; i < jobLimits.count(); ++i) {
        const JobLimit &moduleJobLimit = jobLimits.jobLimitAt(i);
        if (m_product.product->jobLimits.getLimit(moduleJobLimit.pool()) == -1)
            m_product.product->jobLimits.setJobLimit(moduleJobLimit);
    }
}

void ProductResolverStage2::resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                                          const QVariantMap &parameters, JobLimits &jobLimits)
{
    if (!item->isPresentModule())
        return;

    ModuleContext moduleContext;
    moduleContext.module = ResolvedModule::create();

    const ResolvedModulePtr &module = moduleContext.module;
    module->name = moduleName.toString();
    module->isProduct = isProduct;
    module->product = m_product.product.get();
    module->setupBuildEnvironmentScript.initialize(m_loaderState.topLevelProject()
            .scriptFunctionValue(item, StringConstants::setupBuildEnvironmentProperty()));
    module->setupRunEnvironmentScript.initialize(m_loaderState.topLevelProject()
            .scriptFunctionValue(item, StringConstants::setupRunEnvironmentProperty()));

    for (const Item::Module &m : item->modules()) {
        if (m.item->isPresentModule())
            module->moduleDependencies += m.name.toString();
    }

    m_product.product->modules.push_back(module);
    if (!parameters.empty())
        m_product.product->moduleParameters[module] = parameters;

    for (Item *child : item->children()) {
        switch (child->type()) {
        case ItemType::Rule:
            resolveRule(m_loaderState, child, nullptr, &m_product, &moduleContext);
            break;
        case ItemType::FileTagger:
            resolveFileTagger(m_loaderState, child, nullptr, &m_product);
            break;
        case ItemType::JobLimit:
            resolveJobLimit(m_loaderState, child, nullptr, nullptr, &moduleContext);
            break;
        case ItemType::Scanner:
            resolveScanner(child, moduleContext);
            break;
        default:
            break;
        }
    }
    for (int i = 0; i < moduleContext.jobLimits.count(); ++i) {
        const JobLimit &newJobLimit = moduleContext.jobLimits.jobLimitAt(i);
        const int oldLimit = jobLimits.getLimit(newJobLimit.pool());
        if (oldLimit == -1 || oldLimit > newJobLimit.limit())
            jobLimits.setJobLimit(newJobLimit);
    }
}

void ProductResolverStage2::applyFileTaggers()
{
    m_product.product->fileTaggers << m_product.project->fileTaggers;
    m_product.product->fileTaggers = sorted(m_product.product->fileTaggers,
              [] (const FileTaggerConstPtr &a, const FileTaggerConstPtr &b) {
        return a->priority() > b->priority();
    });
    for (const SourceArtifactPtr &artifact : m_product.product->allEnabledFiles()) {
        if (!artifact->overrideFileTags || artifact->fileTags.empty()) {
            const QString fileName = FileInfo::fileName(artifact->absoluteFilePath);
            const FileTags fileTags = m_product.product->fileTagsForFileName(fileName);
            artifact->fileTags.unite(fileTags);
            if (artifact->fileTags.empty())
                artifact->fileTags.insert(unknownFileTag());
            qCDebug(lcProjectResolver) << "adding file tags" << artifact->fileTags
                                       << "to" << fileName;
        }
    }
}

void ProductResolverStage2::finalizeArtifactProperties()
{
    for (const SourceArtifactPtr &artifact : m_product.product->allEnabledFiles()) {
        for (const auto &artifactProperties : m_product.product->artifactProperties) {
            if (!artifact->isTargetOfModule()
                    && artifact->fileTags.intersects(artifactProperties->fileTagsFilter())) {
                // FIXME: Should be merged, not overwritten.
                artifact->properties = artifactProperties->propertyMap();
            }
        }

        // Let a positive value of qbs.install imply the file tag "installable".
        if (artifact->properties->qbsPropertyValue(StringConstants::installProperty()).toBool())
            artifact->fileTags += "installable";
    }
}

void ProductResolverStage2::collectProductDependencies()
{
    const ResolvedProductPtr &product = m_product.product;
    if (!product)
        return;
    for (const Item::Module &module : m_product.item->modules()) {
        if (!module.product)
            continue;
        const ResolvedProductPtr &dep = module.product->product;
        QBS_CHECK(dep);
        QBS_CHECK(dep != product);
        product->dependencies << dep;
        product->dependencyParameters.insert(dep, module.parameters); // TODO: Streamline this with normal module dependencies?
    }

    // TODO: We might want to keep the topological sorting and get rid of "module module dependencies".
    std::sort(product->dependencies.begin(),product->dependencies.end(),
              [](const ResolvedProductPtr &p1, const ResolvedProductPtr &p2) {
        return p1->fullDisplayName() < p2->fullDisplayName();
    });
}

void ExportsResolver::start()
{
    resolveShadowProduct();
    collectExportedProductDependencies();
}

void ExportsResolver::resolveShadowProduct()
{
    if (!m_product.product->enabled)
        return;
    if (!m_product.shadowProduct)
        return;
    for (const auto &m : m_product.shadowProduct->item->modules()) {
        if (m.name.toString() != m_product.product->name)
            continue;
        collectPropertiesForExportItem(m.item);
        for (const auto &dep : m.item->modules())
            collectPropertiesForModuleInExportItem(dep);
        break;
    }
    try {
        adaptExportedPropertyValues();
    } catch (const ErrorInfo &) {}
}

class TempScopeSetter
{
public:
    TempScopeSetter(const ValuePtr &value, Item *newScope) : m_value(value), m_oldScope(value->scope())
    {
        value->setScope(newScope, {});
    }
    ~TempScopeSetter() { if (m_value) m_value->setScope(m_oldScope, {}); }

    TempScopeSetter(const TempScopeSetter &) = delete;
    TempScopeSetter &operator=(const TempScopeSetter &) = delete;
    TempScopeSetter &operator=(TempScopeSetter &&) = delete;

    TempScopeSetter(TempScopeSetter &&other) noexcept
        : m_value(std::move(other.m_value)), m_oldScope(other.m_oldScope)
    {
        other.m_value.reset();
        other.m_oldScope = nullptr;
    }

private:
    ValuePtr m_value;
    Item *m_oldScope;
};

void ExportsResolver::collectPropertiesForExportItem(
    const QualifiedId &moduleName, const ValuePtr &value, Item *moduleInstance,
    QVariantMap &moduleProps)
{
    QBS_CHECK(value->type() == Value::ItemValueType);
    Item * const itemValueItem = std::static_pointer_cast<ItemValue>(value)->item();
    if (itemValueItem->propertyDeclarations().isEmpty()) {
        for (const Item::Module &module : moduleInstance->modules()) {
            if (module.name == moduleName) {
                itemValueItem->setPropertyDeclarations(module.item->propertyDeclarations());
                break;
            }
        }
    }
    if (itemValueItem->type() == ItemType::ModuleInstancePlaceholder) {
        struct EvalPreparer {
            EvalPreparer(Item *valueItem, const QualifiedId &moduleName)
                : valueItem(valueItem),
                hadName(!!valueItem->variantProperty(StringConstants::nameProperty()))
            {
                if (!hadName) {
                    // Evaluator expects a name here.
                    valueItem->setProperty(StringConstants::nameProperty(),
                                           VariantValue::create(moduleName.toString()));
                }
            }
            ~EvalPreparer()
            {
                if (!hadName)
                    valueItem->removeProperty(StringConstants::nameProperty());
            }
            Item * const valueItem;
            const bool hadName;
        };
        EvalPreparer ep(itemValueItem, moduleName);
        std::vector<TempScopeSetter> tss;
        for (const ValuePtr &v : itemValueItem->properties())
            tss.emplace_back(v, moduleInstance);
        moduleProps.insert(moduleName.toString(), m_propertiesEvaluator.evaluateProperties(
                                                      itemValueItem, false, false));
        return;
    }
    QBS_CHECK(itemValueItem->type() == ItemType::ModulePrefix);
    const Item::PropertyMap &props = itemValueItem->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        QualifiedId fullModuleName = moduleName;
        fullModuleName << it.key();
        collectPropertiesForExportItem(fullModuleName, it.value(), moduleInstance, moduleProps);
    }
}

void ExportsResolver::collectPropertiesForExportItem(Item *productModuleInstance)
{
    if (!productModuleInstance->isPresentModule())
        return;
    Item * const exportItem = productModuleInstance->prototype();
    QBS_CHECK(exportItem);
    QBS_CHECK(exportItem->type() == ItemType::Export);
    const ItemDeclaration::Properties exportDecls = BuiltinDeclarations::instance()
                                                        .declarationsForType(ItemType::Export).properties();
    ExportedModule &exportedModule = m_product.product->exportedModule;
    const auto &props = exportItem->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        const auto match
            = [it](const PropertyDeclaration &decl) { return decl.name() == it.key(); };
        if (it.key() != StringConstants::prefixMappingProperty() &&
            std::find_if(exportDecls.begin(), exportDecls.end(), match) != exportDecls.end()) {
            continue;
        }
        if (it.value()->type() == Value::ItemValueType) {
            collectPropertiesForExportItem(it.key(), it.value(), productModuleInstance,
                                           exportedModule.modulePropertyValues);
        } else {
            TempScopeSetter tss(it.value(), productModuleInstance);
            m_propertiesEvaluator.evaluateProperty(
                exportItem, it.key(), it.value(), exportedModule.propertyValues, false);
        }
    }
}

// Collects module properties assigned to in other (higher-level) modules.
void ExportsResolver::collectPropertiesForModuleInExportItem(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    ExportedModule &exportedModule = m_product.product->exportedModule;
    if (module.product || module.name.first() == StringConstants::qbsModule())
        return;
    const auto checkName = [module](const ExportedModuleDependency &d) {
        return module.name.toString() == d.name;
    };
    if (any_of(exportedModule.moduleDependencies, checkName))
        return;

    Item *modulePrototype = module.item->prototype();
    while (modulePrototype && modulePrototype->type() != ItemType::Module)
        modulePrototype = modulePrototype->prototype();
    if (!modulePrototype) // Can happen for broken products in relaxed mode.
        return;
    ModuleItemLocker locker(*modulePrototype);
    const Item::PropertyMap &props = modulePrototype->properties();
    ExportedModuleDependency dep;
    dep.name = module.name.toString();
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (it.value()->type() == Value::ItemValueType)
            collectPropertiesForExportItem(it.key(), it.value(), module.item, dep.moduleProperties);
    }
    exportedModule.moduleDependencies.push_back(dep);

    for (const auto &dep : module.item->modules())
        collectPropertiesForModuleInExportItem(dep);
}

void ExportsResolver::adaptExportedPropertyValues()
{
    QBS_CHECK(m_product.shadowProduct);
    ExportedModule &m = m_product.product->exportedModule;
    const QVariantList prefixList = m.propertyValues.take(
                                                        StringConstants::prefixMappingProperty()).toList();
    const QString shadowProductName = m_loaderState.evaluator().stringValue(
        m_product.shadowProduct->item, StringConstants::nameProperty());
    const QString shadowProductBuildDir = m_loaderState.evaluator().stringValue(
        m_product.shadowProduct->item, StringConstants::buildDirectoryProperty());
    QVariantMap prefixMap;
    for (const QVariant &v : prefixList) {
        const QVariantMap o = v.toMap();
        prefixMap.insert(o.value(QStringLiteral("prefix")).toString(),
                         o.value(QStringLiteral("replacement")).toString());
    }
    const auto valueRefersToImportingProduct
        = [shadowProductName, shadowProductBuildDir](const QString &value) {
              return value.toLower().contains(shadowProductName.toLower())
                     || value.contains(shadowProductBuildDir);
          };
    static const auto stringMapper = [](const QVariantMap &mappings, const QString &value)
        -> QString {
        for (auto it = mappings.cbegin(); it != mappings.cend(); ++it) {
            if (value.startsWith(it.key()))
                return it.value().toString() + value.mid(it.key().size());
        }
        return value;
    };
    const auto stringListMapper = [&valueRefersToImportingProduct](
                                      const QVariantMap &mappings, const QStringList &value)  -> QStringList {
        QStringList result;
        result.reserve(value.size());
        for (const QString &s : value) {
            if (!valueRefersToImportingProduct(s))
                result.push_back(stringMapper(mappings, s));
        }
        return result;
    };
    const std::function<QVariant(const QVariantMap &, const QVariant &)> mapper
        = [&stringListMapper, &mapper](
              const QVariantMap &mappings, const QVariant &value) -> QVariant {
        switch (static_cast<QMetaType::Type>(value.userType())) {
        case QMetaType::QString:
            return stringMapper(mappings, value.toString());
        case QMetaType::QStringList:
            return stringListMapper(mappings, value.toStringList());
        case QMetaType::QVariantMap: {
            QVariantMap m = value.toMap();
            for (auto it = m.begin(); it != m.end(); ++it)
                it.value() = mapper(mappings, it.value());
            return m;
        }
        default:
            return value;
        }
    };
    for (auto it = m.propertyValues.begin(); it != m.propertyValues.end(); ++it)
        it.value() = mapper(prefixMap, it.value());
    for (auto it = m.modulePropertyValues.begin(); it != m.modulePropertyValues.end(); ++it)
        it.value() = mapper(prefixMap, it.value());
    for (ExportedModuleDependency &dep : m.moduleDependencies) {
        for (auto it = dep.moduleProperties.begin(); it != dep.moduleProperties.end(); ++it)
            it.value() = mapper(prefixMap, it.value());
    }
}

void ExportsResolver::collectExportedProductDependencies()
{
    if (!m_product.shadowProduct)
        return;
    const ResolvedProductPtr exportingProduct = m_product.product;
    if (!exportingProduct || !exportingProduct->enabled)
        return;
    Item * const importingProductItem = m_product.shadowProduct->item;

    std::vector<std::pair<ResolvedProductPtr, QVariantMap>> directDeps;
    for (const Item::Module &m : importingProductItem->modules()) {
        if (m.name.toString() != exportingProduct->name)
            continue;
        for (const Item::Module &dep : m.item->modules()) {
            if (dep.product)
                directDeps.emplace_back(dep.product->product, m.parameters);
        }
    }
    for (const auto &dep : directDeps) {
        if (!contains(exportingProduct->exportedModule.productDependencies,
                      dep.first->uniqueName())) {
            exportingProduct->exportedModule.productDependencies.push_back(
                dep.first->uniqueName());
        }
        if (!dep.second.isEmpty()) {
            exportingProduct->exportedModule.dependencyParameters.insert(dep.first,
                                                                         dep.second);
        }
    }
    auto &productDeps = exportingProduct->exportedModule.productDependencies;
    std::sort(productDeps.begin(), productDeps.end());
}

QVariantMap PropertiesEvaluator::evaluateProperties(
    const Item *item, const Item *propertiesContainer, const QVariantMap &tmplt,
    bool lookupPrototype, bool checkErrors)
{
    AccumulatingTimer propEvalTimer(m_loaderState.parameters().logElapsedTime()
                                        ? &m_product.timingData.propertyEvaluation
                                        : nullptr);
    QVariantMap result = tmplt;
    for (auto it = propertiesContainer->properties().begin();
         it != propertiesContainer->properties().end(); ++it) {
        evaluateProperty(item, it.key(), it.value(), result, checkErrors);
    }
    return lookupPrototype && propertiesContainer->prototype()
            && propertiesContainer->prototype()->type() != ItemType::Module
            ? evaluateProperties(item, propertiesContainer->prototype(), result, true, checkErrors)
            : result;
}

QVariantMap PropertiesEvaluator::evaluateProperties(Item *item, bool lookupPrototype,
                                                    bool checkErrors)
{
    const QVariantMap tmplt;
    return evaluateProperties(item, item, tmplt, lookupPrototype, checkErrors);
}

void PropertiesEvaluator::evaluateProperty(
    const Item *item, const QString &propName, const ValuePtr &propValue, QVariantMap &result,
    bool checkErrors)
{
    JSContext * const ctx = m_loaderState.evaluator().engine()->context();
    switch (propValue->type()) {
    case Value::ItemValueType:
    {
        // Ignore items. Those point to module instances
        // and are handled in evaluateModuleValues().
        break;
    }
    case Value::JSSourceValueType:
    {
        if (result.contains(propName))
            break;
        const PropertyDeclaration pd = item->propertyDeclaration(propName);
        if (pd.flags().testFlag(PropertyDeclaration::PropertyNotAvailableInConfig)) {
            break;
        }
        const ScopedJsValue scriptValue(ctx, m_loaderState.evaluator().property(item, propName));
        if (JsException ex = m_loaderState.evaluator().engine()->checkAndClearException(
                propValue->location())) {
            if (checkErrors)
                throw ex.toErrorInfo();
        }

        // NOTE: Loses type information if scriptValue.isUndefined == true,
        //       as such QScriptValues become invalid QVariants.
        QVariant v;
        if (JS_IsFunction(ctx, scriptValue)) {
            v = getJsString(ctx, scriptValue);
        } else {
            v = getJsVariant(ctx, scriptValue);
            QVariantMap m = v.toMap();
            if (m.contains(StringConstants::importScopeNamePropertyInternal())) {
                QVariantMap tmp = m;
                const ScopedJsValue proto(ctx, JS_GetPrototype(ctx, scriptValue));
                m = getJsVariant(ctx, proto).toMap();
                for (auto it = tmp.begin(); it != tmp.end(); ++it)
                    m.insert(it.key(), it.value());
                v = m;
            }
        }

        if (pd.type() == PropertyDeclaration::Path && v.isValid()) {
            v = v.toString();
        } else if (pd.type() == PropertyDeclaration::PathList
                   || pd.type() == PropertyDeclaration::StringList) {
            v = v.toStringList();
        } else if (pd.type() == PropertyDeclaration::VariantList) {
            v = v.toList();
        }
        pd.checkAllowedValues(v, propValue->location(), propName, m_loaderState);
        result[propName] = v;
        break;
    }
    case Value::VariantValueType:
    {
        if (result.contains(propName))
            break;
        VariantValuePtr vvp = std::static_pointer_cast<VariantValue>(propValue);
        QVariant v = vvp->value();

        const PropertyDeclaration pd = item->propertyDeclaration(propName);
        if (v.isNull() && !pd.isScalar()) // QTBUG-51237
            v = QStringList();

        pd.checkAllowedValues(v, propValue->location(), propName, m_loaderState);
        result[propName] = v;
        break;
    }
    }
}

} // namespace qbs::Internal
