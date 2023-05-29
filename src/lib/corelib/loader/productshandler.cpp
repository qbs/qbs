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

#include <language/evaluator.h>
#include <language/item.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/profiling.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

namespace qbs::Internal {

class ProductsHandler::Private
{
public:
    Private(LoaderState &loaderState) : loaderState(loaderState) {}

    void handleNextProduct();
    void handleProduct(ProductContext &product, Deferral deferral);
    void resolveProbes(ProductContext &product);
    void resolveProbes(ProductContext &product, Item *item);
    void handleModuleSetupError(ProductContext &product, const Item::Module &module,
                                const ErrorInfo &error);
    void runModuleProbes(ProductContext &product, const Item::Module &module);
    bool validateModule(ProductContext &product, const Item::Module &module);
    void updateModulePresentState(ProductContext &product, const Item::Module &module);
    void handleGroups(ProductContext &product);

    LoaderState &loaderState;
    GroupsHandler groupsHandler{loaderState};
    qint64 elapsedTime = 0;
};

ProductsHandler::ProductsHandler(LoaderState &loaderState) : d(makePimpl<Private>(loaderState)) {}

void ProductsHandler::run()
{
    AccumulatingTimer timer(d->loaderState.parameters().logElapsedTime()
                            ? &d->elapsedTime : nullptr);

    TopLevelProjectContext &topLevelProject = d->loaderState.topLevelProject();
    for (ProjectContext * const projectContext : topLevelProject.projects) {
        for (ProductContext &productContext : projectContext->products)
            topLevelProject.productsToHandle.emplace_back(&productContext, -1);
    }
    while (!topLevelProject.productsToHandle.empty())
        d->handleNextProduct();
}

void ProductsHandler::printProfilingInfo(int indent)
{
    if (!d->loaderState.parameters().logElapsedTime())
        return;
    const QByteArray prefix(indent, ' ');
    d->loaderState.logger().qbsLog(LoggerInfo, true)
            << prefix
            << Tr::tr("Handling products took %1.")
               .arg(elapsedTimeString(d->elapsedTime));
    d->groupsHandler.printProfilingInfo(indent + 2);
}

ProductsHandler::~ProductsHandler() = default;

void ProductsHandler::Private::handleNextProduct()
{
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();

    auto [product, queueSizeOnInsert] = topLevelProject.productsToHandle.front();
    topLevelProject.productsToHandle.pop_front();

    // If the queue of in-progress products has shrunk since the last time we tried handling
    // this product, there has been forward progress and we can allow a deferral.
    const Deferral deferral = queueSizeOnInsert == -1
            || queueSizeOnInsert > int(topLevelProject.productsToHandle.size())
            ? Deferral::Allowed : Deferral::NotAllowed;

    loaderState.itemReader().setExtraSearchPathsStack(product->project->searchPathsStack);
    try {
        handleProduct(*product, deferral);
        if (product->name.startsWith(StringConstants::shadowProductPrefix()))
            topLevelProject.probes << product->info.probes;
    } catch (const ErrorInfo &err) {
        product->handleError(err);
    }

    // The search paths stack can change during dependency resolution (due to module providers);
    // check that we've rolled back all the changes
    QBS_CHECK(loaderState.itemReader().extraSearchPathsStack() == product->project->searchPathsStack);

    // If we encountered a dependency to an in-progress product or to a bulk dependency,
    // we defer handling this product if it hasn't failed yet and there is still forward progress.
    if (!product->info.delayedError.hasError() && !product->dependenciesResolved) {
        topLevelProject.productsToHandle.emplace_back(
            product, int(topLevelProject.productsToHandle.size()));
    }
}

void ProductsHandler::Private::handleProduct(ProductContext &product, Deferral deferral)
{
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();
    topLevelProject.checkCancelation(loaderState.parameters());

    if (product.info.delayedError.hasError())
        return;

    product.dependenciesResolved = loaderState.dependenciesResolver()
            .resolveDependencies(product, deferral);
    if (!product.dependenciesResolved)
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
    loaderState.propertyMerger().doFinalMerge(product.item);

    const bool enabled = topLevelProject.checkItemCondition(product.item, evaluator);
    loaderState.dependenciesResolver().checkDependencyParameterDeclarations(
                product.item, product.name);

    handleGroups(product);

    // Collect the full list of fileTags, including the values contributed by modules.
    if (!product.info.delayedError.hasError() && enabled
        && !product.name.startsWith(StringConstants::shadowProductPrefix())) {
        for (const FileTag &tag : fileTags)
            topLevelProject.productsByType.insert({tag, &product});
        product.item->setProperty(StringConstants::typeProperty(),
                                  VariantValue::create(sorted(fileTags.toStringList())));
    }
    topLevelProject.productInfos[product.item] = product.info;
}

void ProductsHandler::Private::resolveProbes(ProductContext &product)
{
    for (const Item::Module &module : product.item->modules()) {
        runModuleProbes(product, module);
        if (product.info.delayedError.hasError())
            return;
    }
    resolveProbes(product, product.item);
}

void ProductsHandler::Private::resolveProbes(ProductContext &product, Item *item)
{
    product.info.probes << loaderState.probesResolver().resolveProbes(
                               {product.name, product.uniqueName()}, item);
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
    if (module.productInfo && loaderState.topLevelProject().disabledItems.contains(
                module.productInfo->item)) {
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
        if (product.info.delayedError.hasError())
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
            if (loaderState.topLevelProject().disabledItems.contains(
                        loadingItem->prototype()->parent())) {
                continue;
            }
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

void ProductsHandler::Private::handleGroups(ProductContext &product)
{
    groupsHandler.setupGroups(product.item, product.scope);
    product.info.modulePropertiesSetInGroups = groupsHandler.modulePropertiesSetInGroups();
    loaderState.topLevelProject().disabledItems.unite(groupsHandler.disabledGroups());
}

} // namespace qbs::Internal
