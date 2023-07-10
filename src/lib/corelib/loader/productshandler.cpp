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

#include "productshandler.h"

#include "dependenciesresolver.h"
#include "groupshandler.h"
#include "itemreader.h"
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
#include <language/itemdeclaration.h>
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

#include <queue>

namespace qbs::Internal {

class ProductsHandler::Private
{
public:
    Private(LoaderState &loaderState) : loaderState(loaderState) {}

    void handleProduct(ProductContext &product, Deferral deferral);
    void setupProductForResolving(ProductContext &product, Deferral deferral);
    void resolveProbes(ProductContext &product);
    void resolveProbes(ProductContext &product, Item *item);
    void handleModuleSetupError(ProductContext &product, const Item::Module &module,
                                const ErrorInfo &error);
    void runModuleProbes(ProductContext &product, const Item::Module &module);
    bool validateModule(ProductContext &product, const Item::Module &module);
    void updateModulePresentState(ProductContext &product, const Item::Module &module);
    void checkPropertyDeclarations(ProductContext &product);

    void resolveProduct(ProductContext &productContext);
    void resolveProductFully(ProductContext &productContext);
    void buildProductDependencyList(ProductContext &productContext);
    void collectExportedProductDependencies(ProductContext &productContext);
    void createProductConfig(ProductContext &productContext);
    QVariantMap evaluateModuleValues(ProductContext &productContext, Item *item,
                                     bool lookupPrototype = true);
    QVariantMap evaluateProperties(ProductContext &productContext, Item *item, bool lookupPrototype,
                                   bool checkErrors);
    QVariantMap evaluateProperties(
            ProductContext &productContext, const Item *item, const Item *propertiesContainer,
            const QVariantMap &tmplt, bool lookupPrototype, bool checkErrors);
    void evaluateProperty(const Item *item, const QString &propName, const ValuePtr &propValue,
                          QVariantMap &result, bool checkErrors);
    void checkAllowedValues(const QVariant &value, const CodeLocation &loc,
                            const PropertyDeclaration &decl, const QString &key) const;
    void applyFileTaggers(const ProductContext &productContext);
    void finalizeArtifactProperties(const ResolvedProduct &product);

    // TODO: Move to GroupsHandler?
    void resolveGroup(Item *item, ProductContext &productContext);
    void resolveGroupFully(Item *item, ProductContext &productContext, bool isEnabled);
    QVariantMap resolveAdditionalModuleProperties(ProductContext &productContext, const Item *group,
                                                  const QVariantMap &currentValues);

    static SourceArtifactPtr createSourceArtifact(
        const ResolvedProductPtr &rproduct, const QString &fileName, const GroupPtr &group,
        bool wildcard, const CodeLocation &filesLocation = CodeLocation(),
        FileLocations *fileLocations = nullptr, ErrorInfo *errorInfo = nullptr);
    void resolveShadowProduct(ProductContext &productContext);
    void collectPropertiesForExportItem(ProductContext &productContext,
                                        Item *productModuleInstance);
    void collectPropertiesForExportItem(
            ProductContext &productContext, const QualifiedId &moduleName, const ValuePtr &value,
            Item *moduleInstance, QVariantMap &moduleProps);
    void collectPropertiesForModuleInExportItem(ProductContext &productContext,
                                                const Item::Module &module);
    void adaptExportedPropertyValues(ProductContext &productContext);
    void resolveExport(Item *exportItem, ProductContext &productContext);
    void setupExportedProperties(const Item *item, const QString &namePrefix,
                                 std::vector<ExportedProperty> &properties);
    std::unique_ptr<ExportedItem> resolveExportChild(const Item *item,
                                                     const ExportedModule &module);
    void resolveModules(const Item *item, ProductContext &productContext);
    void resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                       const QVariantMap &parameters, JobLimits &jobLimits,
                       ProductContext &productContext);
    void resolveScanner(Item *item, ProductContext &productContext, ModuleContext &moduleContext);

    LoaderState &loaderState;
};

ProductsHandler::ProductsHandler(LoaderState &loaderState) : d(makePimpl<Private>(loaderState)) {}

void ProductsHandler::run()
{
    TopLevelProjectContext &topLevelProject = d->loaderState.topLevelProject();
    std::queue<std::pair<ProductContext *, int>> productsToHandle;
    for (ProjectContext * const projectContext : topLevelProject.projects()) {
        for (ProductContext &productContext : projectContext->products) {
            topLevelProject.addProductToHandle(productContext);
            productsToHandle.emplace(&productContext, -1);
        }
    }
    while (!productsToHandle.empty()) {
        const auto [product, queueSizeOnInsert] = productsToHandle.front();
        productsToHandle.pop();

        // If the queue of in-progress products has shrunk since the last time we tried handling
        // this product, there has been forward progress and we can allow a deferral.
        const Deferral deferral = queueSizeOnInsert == -1
                || queueSizeOnInsert > int(productsToHandle.size())
                ? Deferral::Allowed : Deferral::NotAllowed;
        d->loaderState.itemReader().setExtraSearchPathsStack(product->project->searchPathsStack);
        d->handleProduct(*product, deferral);
        if (topLevelProject.isCanceled())
            throw CancelException();

        // The search paths stack can change during dependency resolution (due to module providers);
        // check that we've rolled back all the changes
        QBS_CHECK(d->loaderState.itemReader().extraSearchPathsStack()
                  == product->project->searchPathsStack);

        // If we encountered a dependency to an in-progress product or to a bulk dependency,
        // we defer handling this product if it hasn't failed yet and there is still
        // forward progress.
        if (product->dependenciesResolvingPending())
            productsToHandle.emplace(product, int(productsToHandle.size()));
        else
            topLevelProject.removeProductToHandle(*product);

        topLevelProject.timingData() += product->timingData;
    }
}

void ProductsHandler::Private::resolveProduct(ProductContext &productContext)
{
    loaderState.evaluator().clearPropertyDependencies();

    ResolvedProductPtr product = ResolvedProduct::create();
    product->enabled = productContext.project->project->enabled;
    product->moduleProperties = PropertyMapInternal::create();
    product->project = productContext.project->project;
    productContext.product = product;
    product->location = productContext.item->location();
    const auto errorFromDelayedError = [&] {
        if (productContext.delayedError.hasError()) {
            ErrorInfo errorInfo;

            // First item is "main error", gets prepended again in the catch clause.
            const QList<ErrorItem> &items = productContext.delayedError.items();
            for (int i = 1; i < items.size(); ++i)
                errorInfo.append(items.at(i));

            productContext.delayedError.clear();
            return errorInfo;
        }
        return ErrorInfo();
    };

    // Even if we previously encountered an error, try to continue for as long as possible
    // to provide IDEs with useful data (e.g. the list of files).
    // If we encounter a follow-up error, suppress it and report the original one instead.
    try {
        resolveProductFully(productContext);
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
        if (loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
            throw fullError;
        loaderState.logger().printWarning(fullError);
        loaderState.logger().printWarning(
                    ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                              .arg(product->name), product->location));
        product->enabled = false;
    }
}

ProductsHandler::~ProductsHandler() = default;

void ProductsHandler::Private::handleProduct(ProductContext &product, Deferral deferral)
{
    try {
        setupProductForResolving(product, deferral);
    } catch (const ErrorInfo &err) {
        if (err.isCancelException()) {
            loaderState.topLevelProject().setCanceled();
            return;
        }
        product.handleError(err);
    }

    if (product.dependenciesResolvingPending())
        return;

    // TODO: The weird double-forwarded error handling can hopefully be simplified now.
    try {
        resolveProduct(product);
    } catch (const ErrorInfo &err) {
        if (err.isCancelException()) {
            loaderState.topLevelProject().setCanceled();
            return;
        }
        loaderState.topLevelProject().addQueuedError(err);
    }
}

void ProductsHandler::Private::setupProductForResolving(ProductContext &product, Deferral deferral)
{
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();
    topLevelProject.checkCancelation();

    if (product.delayedError.hasError())
        return;

    loaderState.dependenciesResolver().resolveDependencies(product, deferral);
    QBS_CHECK(product.dependenciesContext);
    if (!product.dependenciesContext->dependenciesResolved)
        return;

    // Run probes for modules and product.
    resolveProbes(product);

    // After the probes have run, we can switch on the evaluator cache.
    Evaluator &evaluator = loaderState.evaluator();
    FileTags fileTags = evaluator.fileTagsValue(product.item, StringConstants::typeProperty());
    EvalCacheEnabler cacheEnabler(&evaluator, evaluator.stringValue(
                                                  product.item,
                                                  StringConstants::sourceDirectoryProperty()));

    // Run module validation scripts.
    for (const Item::Module &module : product.item->modules()) {
        if (!validateModule(product, module))
            return;
        fileTags += evaluator.fileTagsValue(
            module.item, StringConstants::additionalProductTypesProperty());
    }

    // Disable modules that have been pulled in only by now-disabled modules.
    // Note that this has to happen in the reverse order compared to the other loops,
    // with the leaves checked last.
    for (auto it = product.item->modules().rbegin(); it != product.item->modules().rend(); ++it)
        updateModulePresentState(product, *it);

    // Now do the canonical module property values merge. Note that this will remove
    // previously attached values from modules that failed validation.
    // Evaluator cache entries that could potentially change due to this will be purged.
    loaderState.propertyMerger().doFinalMerge(product);

    const bool enabled = topLevelProject.checkItemCondition(product.item, evaluator);
    loaderState.dependenciesResolver().checkDependencyParameterDeclarations(
                product.item, product.name);

    setupGroups(product, loaderState);

    // Collect the full list of fileTags, including the values contributed by modules.
    if (!product.delayedError.hasError() && enabled
        && !product.name.startsWith(StringConstants::shadowProductPrefix())) {
        topLevelProject.addProductByType(product, fileTags);
        product.item->setProperty(StringConstants::typeProperty(),
                                  VariantValue::create(sorted(fileTags.toStringList())));
    }

    checkPropertyDeclarations(product);

    if (!product.shadowProduct)
        return;
    cacheEnabler.reset();
    try {
        setupProductForResolving(*product.shadowProduct, Deferral::NotAllowed);
        topLevelProject.addProbes(product.shadowProduct->probes);
    } catch (const ErrorInfo &e) {
        if (e.isCancelException())
            throw CancelException();
        product.shadowProduct->handleError(e);
    }
}

void ProductsHandler::Private::resolveProbes(ProductContext &product)
{
    for (const Item::Module &module : product.item->modules()) {
        runModuleProbes(product, module);
        if (product.delayedError.hasError())
            return;
    }
    resolveProbes(product, product.item);
}

void ProductsHandler::Private::resolveProbes(ProductContext &product, Item *item)
{
    loaderState.probesResolver().resolveProbes(product, item);
}

void ProductsHandler::Private::handleModuleSetupError(
        ProductContext &product, const Item::Module &module, const ErrorInfo &error)
{
    if (module.required) {
        product.handleError(error);
    } else {
        qCDebug(lcModuleLoader()) << "non-required module" << module.name.toString()
                                  << "found, but not usable in product" << product.name
                                  << error.toString();
        createNonPresentModule(*module.item->pool(), module.name.toString(),
                               QStringLiteral("failed validation"), module.item);
    }
}

void ProductsHandler::Private::runModuleProbes(ProductContext &product, const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    if (module.product && loaderState.topLevelProject().isDisabledItem(module.product->item)) {
        createNonPresentModule(*module.item->pool(), module.name.toString(),
                               QLatin1String("module's exporting product is disabled"),
                               module.item);
        return;
    }
    try {
        resolveProbes(product, module.item);
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
                        loaderState.evaluator().stringValue(module.item,
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
        handleModuleSetupError(product, module, error);
    }
}

bool ProductsHandler::Private::validateModule(ProductContext &product, const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return true;
    try {
        loaderState.evaluator().boolValue(module.item, StringConstants::validateProperty());
        for (const auto &dep : module.item->modules()) {
            if (dep.required && !dep.item->isPresentModule()) {
                throw ErrorInfo(Tr::tr("Module '%1' depends on module '%2', which was not "
                                       "loaded successfully")
                                    .arg(module.name.toString(), dep.name.toString()));
            }
        }
    } catch (const ErrorInfo &error) {
        handleModuleSetupError(product, module, error);
        if (product.delayedError.hasError())
            return false;
    }
    return true;
}

void ProductsHandler::Private::updateModulePresentState(ProductContext &product,
                                                        const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    bool hasPresentLoadingItem = false;
    for (const Item * const loadingItem : module.loadingItems) {
        if (loadingItem == product.item) {
            hasPresentLoadingItem = true;
            break;
        }
        if (!loadingItem->isPresentModule())
            continue;
        if (loadingItem->prototype() && loadingItem->prototype()->type() == ItemType::Export) {
            QBS_CHECK(loadingItem->prototype()->parent()->type() == ItemType::Product);
            if (loaderState.topLevelProject().isDisabledItem(loadingItem->prototype()->parent()))
                continue;
        }
        hasPresentLoadingItem = true;
        break;
    }
    if (!hasPresentLoadingItem) {
        createNonPresentModule(*module.item->pool(), module.name.toString(),
                               QLatin1String("imported only by disabled module(s)"),
                               module.item);
    }
}

void ProductsHandler::Private::checkPropertyDeclarations(ProductContext &product)
{
    AccumulatingTimer timer(loaderState.parameters().logElapsedTime()
                            ? &product.timingData.propertyChecking : nullptr);
    qbs::Internal::checkPropertyDeclarations(
                product.item, loaderState.topLevelProject().disabledItems(),
                loaderState.parameters(), loaderState.logger());
}

void ProductsHandler::Private::resolveProductFully(ProductContext &productContext)
{
    Item * const item = productContext.item;
    const ResolvedProductPtr product = productContext.product;
    product->project->products.push_back(product);
    Evaluator &evaluator = loaderState.evaluator();
    product->name = evaluator.stringValue(item, StringConstants::nameProperty());

    // product->buildDirectory() isn't valid yet, because the productProperties map is not ready.
    productContext.buildDirectory = evaluator.stringValue(
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
        product->destinationDirectory = productContext.buildDirectory;
    } else {
        product->destinationDirectory = FileInfo::resolvePath(
                    product->topLevelProject()->buildDirectory,
                    product->destinationDirectory);
    }
    product->probes = productContext.probes;
    createProductConfig(productContext);
    product->productProperties.insert(StringConstants::destinationDirProperty(),
                                      product->destinationDirectory);
    ModuleProperties::init(evaluator.engine(), evaluator.scriptValue(item), product.get());

    QList<Item *> subItems = item->children();
    const ValuePtr filesProperty = item->property(StringConstants::filesProperty());
    if (filesProperty) {
        Item *fakeGroup = Item::create(item->pool(), ItemType::Group);
        fakeGroup->setFile(item->file());
        fakeGroup->setLocation(item->location());
        fakeGroup->setScope(item);
        fakeGroup->setProperty(StringConstants::nameProperty(), VariantValue::create(product->name));
        fakeGroup->setProperty(StringConstants::filesProperty(), filesProperty);
        fakeGroup->setProperty(StringConstants::excludeFilesProperty(),
                               item->property(StringConstants::excludeFilesProperty()));
        fakeGroup->setProperty(StringConstants::overrideTagsProperty(),
                               VariantValue::falseValue());
        fakeGroup->setupForBuiltinType(loaderState.parameters().deprecationWarningMode(),
                                       loaderState.logger());
        subItems.prepend(fakeGroup);
    }

    for (Item * const child : std::as_const(subItems)) {
        switch (child->type()) {
        case ItemType::Rule:
            resolveRule(loaderState, child, productContext.project, &productContext, nullptr);
            break;
        case ItemType::FileTagger:
            resolveFileTagger(loaderState, child, nullptr, &productContext);
            break;
        case ItemType::JobLimit:
            resolveJobLimit(loaderState, child, nullptr, &productContext, nullptr);
            break;
        case ItemType::Group:
            resolveGroup(child, productContext);
            break;
        case ItemType::Export:
            resolveExport(child, productContext);
            break;
        default:
            break;
        }
    }

    for (const ProjectContext *p = productContext.project; p; p = p->parent) {
        JobLimits tempLimits = p->jobLimits;
        product->jobLimits = tempLimits.update(product->jobLimits);
    }

    resolveShadowProduct(productContext);
    resolveModules(item, productContext);
    applyFileTaggers(productContext);
    finalizeArtifactProperties(*product);

    for (const RulePtr &rule : productContext.project->rules) {
        RulePtr clonedRule = rule->clone();
        clonedRule->product = product.get();
        product->rules.push_back(clonedRule);
    }

    buildProductDependencyList(productContext);
    collectExportedProductDependencies(productContext);
}

void ProductsHandler::Private::buildProductDependencyList(ProductContext &productContext)
{
    const ResolvedProductPtr &product = productContext.product;
    if (!product)
        return;
    for (const Item::Module &module : productContext.item->modules()) {
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

void ProductsHandler::Private::collectExportedProductDependencies(ProductContext &productContext)
{
    if (!productContext.shadowProduct)
        return;
    const ResolvedProductPtr exportingProduct = productContext.product;
    if (!exportingProduct || !exportingProduct->enabled)
        return;
    Item * const importingProductItem = productContext.shadowProduct->item;

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

void ProductsHandler::Private::createProductConfig(ProductContext &productContext)
{
    EvalCacheEnabler cachingEnabler(&loaderState.evaluator(),
                                    productContext.product->sourceDirectory);
    productContext.product->moduleProperties->setValue(
                evaluateModuleValues(productContext, productContext.item));
    productContext.product->productProperties = evaluateProperties(productContext,
                productContext.item, productContext.item, QVariantMap(), true, true);
}

QVariantMap ProductsHandler::Private::evaluateModuleValues(
        ProductContext &productContext, Item *item, bool lookupPrototype)
{
    QVariantMap moduleValues;
    for (const Item::Module &module : item->modules()) {
        if (!module.item->isPresentModule())
            continue;
        const QString fullName = module.name.toString();
        moduleValues[fullName] = evaluateProperties(productContext, module.item, lookupPrototype,
                                                    true);
    }
    return moduleValues;
}

QVariantMap ProductsHandler::Private::evaluateProperties(
        ProductContext &productContext, Item *item, bool lookupPrototype, bool checkErrors)
{
    const QVariantMap tmplt;
    return evaluateProperties(productContext, item, item, tmplt, lookupPrototype, checkErrors);
}

QVariantMap ProductsHandler::Private::evaluateProperties(
        ProductContext &productContext, const Item *item, const Item *propertiesContainer,
        const QVariantMap &tmplt, bool lookupPrototype, bool checkErrors)
{
    AccumulatingTimer propEvalTimer(loaderState.parameters().logElapsedTime()
                                    ? &productContext.timingData.propertyEvaluation
                                    : nullptr);
    QVariantMap result = tmplt;
    for (auto it = propertiesContainer->properties().begin();
         it != propertiesContainer->properties().end(); ++it) {
        evaluateProperty(item, it.key(), it.value(), result, checkErrors);
    }
    return lookupPrototype && propertiesContainer->prototype()
            ? evaluateProperties(productContext, item, propertiesContainer->prototype(), result,
                                 true, checkErrors)
            : result;
}

void ProductsHandler::Private::evaluateProperty(
        const Item *item, const QString &propName, const ValuePtr &propValue, QVariantMap &result,
        bool checkErrors)
{
    JSContext * const ctx = loaderState.evaluator().engine()->context();
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
        const ScopedJsValue scriptValue(ctx, loaderState.evaluator().property(item, propName));
        if (JsException ex = loaderState.evaluator().engine()->checkAndClearException(
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
        checkAllowedValues(v, propValue->location(), pd, propName);
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

        checkAllowedValues(v, propValue->location(), pd, propName);
        result[propName] = v;
        break;
    }
    }
}

void ProductsHandler::Private::checkAllowedValues(
        const QVariant &value, const CodeLocation &loc, const PropertyDeclaration &decl,
        const QString &key) const
{
    const auto type = decl.type();
    if (type != PropertyDeclaration::String && type != PropertyDeclaration::StringList)
        return;

    if (value.isNull())
        return;

    const auto &allowedValues = decl.allowedValues();
    if (allowedValues.isEmpty())
        return;

    const auto checkValue = [this, &loc, &allowedValues, &key](const QString &value)
    {
        if (!allowedValues.contains(value)) {
            const auto message = Tr::tr("Value '%1' is not allowed for property '%2'.")
                    .arg(value, key);
            ErrorInfo error(message, loc);
            handlePropertyError(error, loaderState.parameters(), loaderState.logger());
        }
    };

    if (type == PropertyDeclaration::StringList) {
        const auto strings = value.toStringList();
        for (const auto &string: strings) {
            checkValue(string);
        }
    } else if (type == PropertyDeclaration::String) {
        checkValue(value.toString());
    }
}

void ProductsHandler::Private::applyFileTaggers(const ProductContext &productContext)
{
    productContext.product->fileTaggers << productContext.project->fileTaggers;
    productContext.product->fileTaggers = sorted(productContext.product->fileTaggers,
              [] (const FileTaggerConstPtr &a, const FileTaggerConstPtr &b) {
        return a->priority() > b->priority();
    });
    for (const SourceArtifactPtr &artifact : productContext.product->allEnabledFiles()) {
        if (!artifact->overrideFileTags || artifact->fileTags.empty()) {
            const QString fileName = FileInfo::fileName(artifact->absoluteFilePath);
            const FileTags fileTags = productContext.product->fileTagsForFileName(fileName);
            artifact->fileTags.unite(fileTags);
            if (artifact->fileTags.empty())
                artifact->fileTags.insert(unknownFileTag());
            qCDebug(lcProjectResolver) << "adding file tags" << artifact->fileTags
                                       << "to" << fileName;
        }
    }
}

void ProductsHandler::Private::finalizeArtifactProperties(const ResolvedProduct &product)
{
    for (const SourceArtifactPtr &artifact : product.allEnabledFiles()) {
        for (const auto &artifactProperties : product.artifactProperties) {
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

void ProductsHandler::Private::resolveGroup(Item *item, ProductContext &productContext)
{
    const bool parentEnabled = productContext.currentGroup
            ? productContext.currentGroup->enabled
            : productContext.product->enabled;
    const bool isEnabled = parentEnabled
            && loaderState.evaluator().boolValue(item, StringConstants::conditionProperty());
    try {
        resolveGroupFully(item, productContext, isEnabled);
    } catch (const ErrorInfo &error) {
        if (!isEnabled) {
            qCDebug(lcProjectResolver) << "error resolving group at" << item->location()
                                       << error.toString();
            return;
        }
        if (loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
            throw;
        loaderState.logger().printWarning(error);
    }
}

void ProductsHandler::Private::resolveGroupFully(Item *item, ProductContext &productContext,
                                                 bool isEnabled)
{
    AccumulatingTimer groupTimer(loaderState.parameters().logElapsedTime()
                                 ? &productContext.timingData.groupsResolving
                                 : nullptr);

    const auto getGroupPropertyMap = [&](const ArtifactProperties *existingProps) {
        PropertyMapPtr moduleProperties;
        bool newPropertyMapRequired = false;
        if (existingProps)
            moduleProperties = existingProps->propertyMap();
        if (!moduleProperties) {
            newPropertyMapRequired = true;
            moduleProperties = productContext.currentGroup
                    ? productContext.currentGroup->properties
                    : productContext.product->moduleProperties;
        }
        const QVariantMap newModuleProperties = resolveAdditionalModuleProperties(
                    productContext, item, moduleProperties->value());
        if (!newModuleProperties.empty()) {
            if (newPropertyMapRequired)
                moduleProperties = PropertyMapInternal::create();
            moduleProperties->setValue(newModuleProperties);
        }
        return moduleProperties;
    };

    Evaluator &evaluator = loaderState.evaluator();
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

        ProductContext::ArtifactPropertiesInfo &apinfo
                = productContext.artifactPropertiesPerFilter[fileTagsFilter];
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
            productContext.product->artifactProperties.push_back(apinfo.first);
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
    if (!prefixWasSet && productContext.currentGroup)
        group->prefix = productContext.currentGroup->prefix;
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
    } else if (productContext.currentGroup) {
        group->fileTags.unite(productContext.currentGroup->fileTags);
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
                productContext.project->project->topLevelProject()->buildDirectory);
        for (const QString &fileName : files)
            createSourceArtifact(productContext.product, fileName, group, true, filesLocation,
                                 &productContext.sourceArtifactLocations, &fileError);
    }

    for (const QString &fileName : std::as_const(files)) {
        createSourceArtifact(productContext.product, fileName, group, false, filesLocation,
                             &productContext.sourceArtifactLocations, &fileError);
    }
    if (fileError.hasError()) {
        if (group->enabled) {
            if (loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
                throw ErrorInfo(fileError);
            loaderState.logger().printWarning(fileError);
        } else {
            qCDebug(lcProjectResolver) << "error for disabled group:" << fileError.toString();
        }
    }
    group->name = evaluator.stringValue(item, StringConstants::nameProperty());
    if (group->name.isEmpty())
        group->name = Tr::tr("Group %1").arg(productContext.product->groups.size());
    productContext.product->groups.push_back(group);

    class GroupContextSwitcher {
    public:
        GroupContextSwitcher(ProductContext &context, const GroupConstPtr &newGroup)
            : m_context(context), m_oldGroup(context.currentGroup) {
            m_context.currentGroup = newGroup;
        }
        ~GroupContextSwitcher() { m_context.currentGroup = m_oldGroup; }
    private:
        ProductContext &m_context;
        const GroupConstPtr m_oldGroup;
    };
    GroupContextSwitcher groupSwitcher(productContext, group);
    for (Item * const childItem : item->children())
        resolveGroup(childItem, productContext);
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

QVariantMap ProductsHandler::Private::resolveAdditionalModuleProperties(
        ProductContext &productContext, const Item *group, const QVariantMap &currentValues)
{
    // Step 1: Retrieve the properties directly set in the group
    const ModulePropertiesPerGroup &mp = productContext.modulePropertiesSetInGroups;
    const auto it = mp.find(group);
    if (it == mp.end())
        return {};
    const QualifiedIdSet &propsSetInGroup = it->second;

    // Step 2: Gather all properties that depend on these properties.
    const QualifiedIdSet &propsToEval = propertiesToEvaluate(
        rangeTo<std::deque<QualifiedId>>(propsSetInGroup),
                loaderState.evaluator().propertyDependencies());

    // Step 3: Evaluate all these properties and replace their values in the map
    QVariantMap modulesMap = currentValues;
    QHash<QString, QStringList> propsPerModule;
    for (auto fullPropName : propsToEval) {
        const QString moduleName
                = QualifiedId(fullPropName.mid(0, fullPropName.size() - 1)).toString();
        propsPerModule[moduleName] << fullPropName.last();
    }
    EvalCacheEnabler cachingEnabler(&loaderState.evaluator(),
                                    productContext.product->sourceDirectory);
    for (const Item::Module &module : group->modules()) {
        const QString &fullModName = module.name.toString();
        const QStringList propsForModule = propsPerModule.take(fullModName);
        if (propsForModule.empty())
            continue;
        QVariantMap reusableValues = modulesMap.value(fullModName).toMap();
        for (const QString &prop : std::as_const(propsForModule))
            reusableValues.remove(prop);
        modulesMap.insert(fullModName,
                          evaluateProperties(productContext, module.item, module.item,
                                             reusableValues, true, true));
    }
    return modulesMap;
}

SourceArtifactPtr ProductsHandler::Private::createSourceArtifact(
        const ResolvedProductPtr &rproduct, const QString &fileName, const GroupPtr &group,
        bool wildcard, const CodeLocation &filesLocation, FileLocations *fileLocations,
        ErrorInfo *errorInfo)
{
    const QString &baseDir = FileInfo::path(group->location.filePath());
    const QString absFilePath = QDir::cleanPath(FileInfo::resolvePath(baseDir, fileName));
    if (!wildcard && !FileInfo(absFilePath).exists()) {
        if (errorInfo)
            errorInfo->append(Tr::tr("File '%1' does not exist.").arg(absFilePath), filesLocation);
        rproduct->missingSourceFiles << absFilePath;
        return {};
    }
    if (group->enabled && fileLocations) {
        CodeLocation &loc = (*fileLocations)[std::make_pair(group->targetOfModule, absFilePath)];
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

void ProductsHandler::Private::resolveShadowProduct(ProductContext &productContext)
{
    if (!productContext.product->enabled)
        return;
    if (!productContext.shadowProduct)
        return;
    for (const auto &m : productContext.shadowProduct->item->modules()) {
        if (m.name.toString() != productContext.product->name)
            continue;
        collectPropertiesForExportItem(productContext, m.item);
        for (const auto &dep : m.item->modules())
            collectPropertiesForModuleInExportItem(productContext, dep);
        break;
    }
    try {
        adaptExportedPropertyValues(productContext);
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

void ProductsHandler::Private::collectPropertiesForExportItem(ProductContext &productContext,
                                                              Item *productModuleInstance)
{
    if (!productModuleInstance->isPresentModule())
        return;
    Item * const exportItem = productModuleInstance->prototype();
    QBS_CHECK(exportItem);
    QBS_CHECK(exportItem->type() == ItemType::Export);
    const ItemDeclaration::Properties exportDecls = BuiltinDeclarations::instance()
            .declarationsForType(ItemType::Export).properties();
    ExportedModule &exportedModule = productContext.product->exportedModule;
    const auto &props = exportItem->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        const auto match
                = [it](const PropertyDeclaration &decl) { return decl.name() == it.key(); };
        if (it.key() != StringConstants::prefixMappingProperty() &&
                std::find_if(exportDecls.begin(), exportDecls.end(), match) != exportDecls.end()) {
            continue;
        }
        if (it.value()->type() == Value::ItemValueType) {
            collectPropertiesForExportItem(productContext, it.key(), it.value(),
                                           productModuleInstance,
                                           exportedModule.modulePropertyValues);
        } else {
            TempScopeSetter tss(it.value(), productModuleInstance);
            evaluateProperty(exportItem, it.key(), it.value(), exportedModule.propertyValues,
                             false);
        }
    }
}

void ProductsHandler::Private::collectPropertiesForExportItem(
        ProductContext &productContext, const QualifiedId &moduleName, const ValuePtr &value,
        Item *moduleInstance, QVariantMap &moduleProps)
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
        moduleProps.insert(moduleName.toString(), evaluateProperties(productContext, itemValueItem, false, false));
        return;
    }
    QBS_CHECK(itemValueItem->type() == ItemType::ModulePrefix);
    const Item::PropertyMap &props = itemValueItem->properties();
    for (auto it = props.begin(); it != props.end(); ++it) {
        QualifiedId fullModuleName = moduleName;
        fullModuleName << it.key();
        collectPropertiesForExportItem(productContext, fullModuleName, it.value(), moduleInstance,
                                       moduleProps);
    }
}

// Collects module properties assigned to in other (higher-level) modules.
void ProductsHandler::Private::collectPropertiesForModuleInExportItem(
        ProductContext &productContext, const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    ExportedModule &exportedModule = productContext.product->exportedModule;
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
    const Item::PropertyMap &props = modulePrototype->properties();
    ExportedModuleDependency dep;
    dep.name = module.name.toString();
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (it.value()->type() == Value::ItemValueType) {
            collectPropertiesForExportItem(productContext, it.key(), it.value(), module.item,
                                           dep.moduleProperties);
        }
    }
    exportedModule.moduleDependencies.push_back(dep);

    for (const auto &dep : module.item->modules())
        collectPropertiesForModuleInExportItem(productContext, dep);
}

void ProductsHandler::Private::adaptExportedPropertyValues(ProductContext &productContext)
{
    QBS_CHECK(productContext.shadowProduct);
    ExportedModule &m = productContext.product->exportedModule;
    const QVariantList prefixList = m.propertyValues.take(
                StringConstants::prefixMappingProperty()).toList();
    const QString shadowProductName = loaderState.evaluator().stringValue(
                productContext.shadowProduct->item, StringConstants::nameProperty());
    const QString shadowProductBuildDir = loaderState.evaluator().stringValue(
                productContext.shadowProduct->item, StringConstants::buildDirectoryProperty());
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

void ProductsHandler::Private::resolveExport(Item *exportItem, ProductContext &productContext)
{
    ExportedModule &exportedModule = productContext.product->exportedModule;
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

void ProductsHandler::Private::setupExportedProperties(
        const Item *item, const QString &namePrefix, std::vector<ExportedProperty> &properties)
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
                || loaderState.evaluator().isNonDefaultValue(item, it.key())) {
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

// TODO: This probably wouldn't be necessary if we had item serialization.
std::unique_ptr<ExportedItem> ProductsHandler::Private::resolveExportChild(
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

void ProductsHandler::Private::resolveModules(const Item *item, ProductContext &productContext)
{
    JobLimits jobLimits;
    for (const Item::Module &m : item->modules())
        resolveModule(m.name, m.item, m.product, m.parameters, jobLimits, productContext);
    for (int i = 0; i < jobLimits.count(); ++i) {
        const JobLimit &moduleJobLimit = jobLimits.jobLimitAt(i);
        if (productContext.product->jobLimits.getLimit(moduleJobLimit.pool()) == -1)
            productContext.product->jobLimits.setJobLimit(moduleJobLimit);
    }
}

void ProductsHandler::Private::resolveModule(
        const QualifiedId &moduleName, Item *item, bool isProduct, const QVariantMap &parameters,
        JobLimits &jobLimits, ProductContext &productContext)
{
    if (!item->isPresentModule())
        return;

    ModuleContext moduleContext;
    moduleContext.module = ResolvedModule::create();

    const ResolvedModulePtr &module = moduleContext.module;
    module->name = moduleName.toString();
    module->isProduct = isProduct;
    module->product = productContext.product.get();
    module->setupBuildEnvironmentScript.initialize(loaderState.topLevelProject()
            .scriptFunctionValue(item, StringConstants::setupBuildEnvironmentProperty()));
    module->setupRunEnvironmentScript.initialize(loaderState.topLevelProject()
            .scriptFunctionValue(item, StringConstants::setupRunEnvironmentProperty()));

    for (const Item::Module &m : item->modules()) {
        if (m.item->isPresentModule())
            module->moduleDependencies += m.name.toString();
    }

    productContext.product->modules.push_back(module);
    if (!parameters.empty())
        productContext.product->moduleParameters[module] = parameters;

    for (Item *child : item->children()) {
        switch (child->type()) {
        case ItemType::Rule:
            resolveRule(loaderState, child, nullptr, &productContext, &moduleContext);
            break;
        case ItemType::FileTagger:
            resolveFileTagger(loaderState, child, nullptr, &productContext);
            break;
        case ItemType::JobLimit:
            resolveJobLimit(loaderState, child, nullptr, nullptr, &moduleContext);
            break;
        case ItemType::Scanner:
            resolveScanner(child, productContext, moduleContext);
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

void ProductsHandler::Private::resolveScanner(Item *item, ProductContext &productContext,
                                              ModuleContext &moduleContext)
{
    Evaluator &evaluator = loaderState.evaluator();
    if (!evaluator.boolValue(item, StringConstants::conditionProperty())) {
        qCDebug(lcProjectResolver) << "scanner condition is false";
        return;
    }

    ResolvedScannerPtr scanner = ResolvedScanner::create();
    scanner->module = moduleContext.module;
    scanner->inputs = evaluator.fileTagsValue(item, StringConstants::inputsProperty());
    scanner->recursive = evaluator.boolValue(item, StringConstants::recursiveProperty());
    scanner->searchPathsScript.initialize(loaderState.topLevelProject().scriptFunctionValue(
                                              item, StringConstants::searchPathsProperty()));
    scanner->scanScript.initialize(loaderState.topLevelProject().scriptFunctionValue(
                                       item, StringConstants::scanProperty()));
    productContext.product->scanners.push_back(scanner);
}

} // namespace qbs::Internal
