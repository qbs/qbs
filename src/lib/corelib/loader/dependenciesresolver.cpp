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

#include "dependenciesresolver.h"

#include "itemreader.h"
#include "loaderutils.h"
#include "moduleinstantiator.h"
#include "moduleloader.h"

#include <language/scriptengine.h>
#include <language/evaluator.h>
#include <language/item.h>
#include <language/itempool.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/preferences.h>
#include <tools/profiling.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <optional>
#include <queue>

namespace qbs::Internal {
namespace {
enum class HandleDependency { Use, Ignore, Defer };

class LoadModuleResult
{
public:
    Item *moduleItem = nullptr;
    ProductContext *product = nullptr;
    HandleDependency handleDependency = HandleDependency::Use;
};

// Corresponds completely to a Depends item.
// May result in more than one module, due to "multiplexing" properties such as subModules etc.
// May also result in no module at all, e.g. if productTypes does not match anything.
class EvaluatedDependsItem
{
public:
    Item *item = nullptr;
    QualifiedId name;
    QStringList subModules;
    FileTags productTypes;
    QStringList multiplexIds;
    std::optional<QStringList> profiles;
    VersionRange versionRange;
    QVariantMap parameters;
    bool limitToSubProject = false;
    bool requiredLocally = true;
    bool requiredGlobally = true;
};

// As opposed to EvaluatedDependsItem, one of these corresponds exactly to one module
// to be loaded. Such an attempt might still fail, though, which may or may not result
// in an error, depending on the value of Depends.required and other circumstances.
class FullyResolvedDependsItem
{
public:
    FullyResolvedDependsItem(ProductContext *product, const EvaluatedDependsItem &dependency);
    FullyResolvedDependsItem(const EvaluatedDependsItem &dependency, QualifiedId name,
                             QString profile, QString multiplexId);
    FullyResolvedDependsItem() = default;
    static FullyResolvedDependsItem makeBaseDependency();

    QString id() const;
    CodeLocation location() const;
    QString displayName() const;

    // If product is non-null, we already know which product the dependency targets.
    // This happens either if Depends.productTypes was set, or if we tried to load the
    // dependency before and already identified the product, but could not complete the
    // procedure because said product had itself not been handled yet.
    ProductContext *product = nullptr;

    Item *item = nullptr;
    QualifiedId name;
    QString profile;
    QString multiplexId;
    VersionRange versionRange;
    QVariantMap parameters;
    bool limitToSubProject = false;
    bool requiredLocally = true;
    bool requiredGlobally = true;
    bool checkProduct = true;
};

class DependenciesResolvingState
{
public:
    Item *loadingItem = nullptr;
    FullyResolvedDependsItem loadingItemOrigin;
    std::queue<Item *> pendingDependsItems;
    std::optional<EvaluatedDependsItem> currentDependsItem;
    std::queue<FullyResolvedDependsItem> pendingResolvedDependencies;
    bool requiredByLoadingItem = true;
};

class DependenciesContextImpl : public DependenciesContext
{
public:
    DependenciesContextImpl(ProductContext &product, LoaderState &loaderState);

    std::list<DependenciesResolvingState> stateStack;

private:
    std::pair<ProductDependency, ProductContext *> pendingDependency() const override;

    void setSearchPathsForProduct(LoaderState &loaderState);

    ProductContext &m_product;
};

class DependenciesResolver
{
public:
    DependenciesResolver(LoaderState &loaderState, ProductContext &product, Deferral deferral)
        : m_loaderState(loaderState), m_product(product), m_deferral(deferral) {}

    void resolve();
    LoadModuleResult loadModule(Item *loadingItem, const FullyResolvedDependsItem &dependency);

private:
    void evaluateNextDependsItem();
    HandleDependency handleResolvedDependencies();
    std::pair<Item::Module *, Item *> findExistingModule(const FullyResolvedDependsItem &dependency,
                                                         Item *item);
    void updateModule(Item::Module &module, const FullyResolvedDependsItem &dependency);
    int dependsChainLength();
    ProductContext *findMatchingProduct(const FullyResolvedDependsItem &dependency);
    Item *findMatchingModule(const FullyResolvedDependsItem &dependency);
    std::pair<bool, HandleDependency> checkProductDependency(
        const FullyResolvedDependsItem &depSpec, const ProductContext &dep);
    void checkModule(const FullyResolvedDependsItem &dependency, Item *moduleItem,
                     ProductContext *productDep);
    void checkForModuleNamePrefixCollision(const FullyResolvedDependsItem &dependency);
    Item::Module createModule(const FullyResolvedDependsItem &dependency, Item *item,
                              ProductContext *productDep);
    void adjustDependsItemForMultiplexing(Item *dependsItem);
    std::optional<EvaluatedDependsItem> evaluateDependsItem(Item *item);
    std::queue<FullyResolvedDependsItem> multiplexDependency(
        const EvaluatedDependsItem &dependency);
    QVariantMap extractParameters(Item *dependsItem) const;
    void forwardParameterDeclarations(const Item *dependsItem, const Item::Modules &modules);
    void forwardParameterDeclarations(const QualifiedId &moduleName, Item *item,
                                      const Item::Modules &modules);
    std::list<DependenciesResolvingState> &stateStack();

    LoaderState &m_loaderState;
    ProductContext &m_product;
    Deferral m_deferral;
};

static bool haveSameSubProject(const ProductContext &p1, const ProductContext &p2);
static QVariantMap safeToVariant(JSContext *ctx, const JSValue &v);
static void collectDependsItems(Item *parent, std::queue<Item *> &dependsItems);

} // namespace

void resolveDependencies(ProductContext &product, Deferral deferral, LoaderState &loaderState)
{
    DependenciesResolver(loaderState, product, deferral).resolve();
}

Item *loadBaseModule(ProductContext &product, Item *item, LoaderState &loaderState)
{
    const auto baseDependency = FullyResolvedDependsItem::makeBaseDependency();
    Item * const moduleItem = DependenciesResolver(loaderState, product, Deferral::NotAllowed)
            .loadModule(item, baseDependency).moduleItem;
    if (Q_UNLIKELY(!moduleItem))
        throw ErrorInfo(Tr::tr("Cannot load base qbs module."));
    return moduleItem;
}

namespace {

void DependenciesResolver::resolve()
{
    AccumulatingTimer timer(m_loaderState.parameters().logElapsedTime()
                            ? &m_product.timingData.dependenciesResolving : nullptr);

    if (!m_product.dependenciesContext) {
        m_product.dependenciesContext = std::make_unique<DependenciesContextImpl>(
                    m_product, m_loaderState);
    } else {
        QBS_CHECK(!m_product.dependenciesContext->dependenciesResolved);
    }
    SearchPathsManager searchPathsMgr(m_loaderState.itemReader(), m_product.searchPaths);

    while (!stateStack().empty()) {
        auto &state = stateStack().front();

        // If we have pending FullyResolvedDependsItems, then these are handled first.
        if (handleResolvedDependencies() == HandleDependency::Defer)
            return;

        // The above procedure might have pushed another state to the stack due to recursive
        // dependencies (i.e. Depends items in the newly loaded module), in which case we
        // continue with that one.
        if (&state != &stateStack().front())
            continue;

        // If we have a pending EvaluatedDependsItem, we multiplex it and then handle
        // the resulting FullyResolvedDependsItems, if there were any.
        if (state.currentDependsItem) {
            QBS_CHECK(state.pendingResolvedDependencies.empty());

            // We postpone handling Depends.productTypes for as long as possible, because
            // the full type of a product becomes available only after its modules have been loaded.
            if (!state.currentDependsItem->productTypes.empty() && m_deferral == Deferral::Allowed)
                return;

            state.pendingResolvedDependencies = multiplexDependency(*state.currentDependsItem);
            state.currentDependsItem.reset();
            m_deferral = Deferral::Allowed; // We made progress.

            continue;
        }

        // Here we have no resolved/evaluated Depends items of any kind, so we evaluate the next
        // pending Depends item.
        evaluateNextDependsItem();
        if (state.currentDependsItem)
            continue;

        // No resolved or unresolved Depends items are left, so we're done with the current state.
        QBS_CHECK(!state.currentDependsItem);
        QBS_CHECK(state.pendingResolvedDependencies.empty());
        QBS_CHECK(state.pendingDependsItems.empty());

        // This ensures our invariant: A sorted module list in the product
        // (dependers after dependencies).
        if (stateStack().size() > 1) {
            QBS_CHECK(state.loadingItem->type() == ItemType::ModuleInstance);
            Item::Modules &modules = m_product.item->modules();
            const auto loadingItemModule = std::find_if(modules.begin(), modules.end(),
                                                        [&](const Item::Module &m) {
                                                            return m.item == state.loadingItem;
                                                        });
            QBS_CHECK(loadingItemModule != modules.end());
            const Item::Module tempModule = *loadingItemModule;
            modules.erase(loadingItemModule);
            modules.push_back(tempModule);
        }
        stateStack().pop_front();
    }
    m_product.dependenciesContext->dependenciesResolved = true;
}

void DependenciesResolver::evaluateNextDependsItem()
{
    auto &state = stateStack().front();
    while (!state.pendingDependsItems.empty()) {
        QBS_CHECK(!state.currentDependsItem);
        QBS_CHECK(state.pendingResolvedDependencies.empty());
        Item * const dependsItem = state.pendingDependsItems.front();
        state.pendingDependsItems.pop();
        adjustDependsItemForMultiplexing(dependsItem);
        if (auto evaluated = evaluateDependsItem(dependsItem)) {
            evaluated->requiredGlobally = evaluated->requiredLocally
                                          && state.loadingItemOrigin.requiredGlobally;
            state.currentDependsItem = evaluated;
            return;
        }
    }
}

HandleDependency DependenciesResolver::handleResolvedDependencies()
{
    DependenciesResolvingState &state = stateStack().front();
    while (!state.pendingResolvedDependencies.empty()) {
        QBS_CHECK(!state.currentDependsItem);
        const FullyResolvedDependsItem dependency = state.pendingResolvedDependencies.front();
        try {
            const LoadModuleResult res = loadModule(state.loadingItem, dependency);
            switch (res.handleDependency) {
            case HandleDependency::Defer:
                QBS_CHECK(m_deferral == Deferral::Allowed);

                // Optimization: We already looked up the product, so let's not do that again
                // next time.
                if (res.product)
                    state.pendingResolvedDependencies.front().product = res.product;

                return HandleDependency::Defer;
            case HandleDependency::Ignore:
                // This happens if the dependency was not required or the module was already
                // loaded via another path.
                state.pendingResolvedDependencies.pop();
                continue;
            case HandleDependency::Use:
                if (dependency.name.toString() == StringConstants::qbsModule()) {
                    // Shortcut: No need to look for recursive dependencies in the base module.
                    state.pendingResolvedDependencies.pop();
                    continue;
                }
                m_deferral = Deferral::Allowed; // We made progress.
                break;
            }

            QBS_CHECK(res.moduleItem);

            // Now continue with the dependencies of the just-loaded module.
            std::queue<Item *> moduleDependsItems;
            collectDependsItems(res.moduleItem, moduleDependsItems);
            state.pendingResolvedDependencies.pop();
            stateStack().push_front(
                {res.moduleItem, dependency, moduleDependsItems, {}, {},
                 dependency.requiredGlobally || state.requiredByLoadingItem});
            stateStack().front().pendingResolvedDependencies.push(
                FullyResolvedDependsItem::makeBaseDependency());
            break;
        } catch (const ErrorInfo &e) {
            if (dependency.name.toString() == StringConstants::qbsModule())
                throw e;

            // See QBS-1338 for why we do not abort handling the product.
            state.pendingResolvedDependencies.pop();
            Item::Modules &modules = m_product.item->modules();

            // Unwind.
            bool unwound = false;
            while (stateStack().size() > 1) {
                const auto loadingItemModule = std::find_if(
                    modules.begin(), modules.end(), [&](const Item::Module &m) {
                        return m.item == stateStack().front().loadingItem;
                    });
                for (auto it = loadingItemModule; it != modules.end(); ++it) {
                    createNonPresentModule(m_loaderState.itemPool(), it->name.toString(),
                                           QLatin1String("error in Depends chain"), it->item);
                }
                modules.erase(loadingItemModule, modules.end());
                stateStack().pop_front();
                unwound = true;
            }

            m_product.handleError(e);
            if (unwound)
                return HandleDependency::Ignore;
            continue;
        }
    }
    return HandleDependency::Ignore;
}

// Produces an item of type ModuleInstance corresponding to the specified dependency.
// The instance's prototype item is either of type Export (if the dependency is a product)
// or of type Module (for an actual module).
// The loadingItem parameter is either the depending product or another module. The newly
// created module is added to the module list of the product item and additionally to the
// loading item's one, if it is not the product. Its name is also injected into the respective
// scopes.
LoadModuleResult DependenciesResolver::loadModule(
    Item *loadingItem, const FullyResolvedDependsItem &dependency)
{
    qCDebug(lcModuleLoader) << "loadModule name:" << dependency.name.toString()
                            << "id:" << dependency.id();

    QBS_CHECK(loadingItem);

    ProductContext *productDep = nullptr;
    Item *moduleItem = nullptr;

    const auto addLoadContext = [&](Item::Module &module) {
        module.loadContexts.emplace_back(dependency.item,
                                         std::make_pair(dependency.parameters,
                                                        INT_MAX - dependsChainLength()));
    };

    // The module might already have been loaded for this product (directly or indirectly).
    const auto &[existingModule, moduleWithSameName]
            = findExistingModule(dependency, m_product.item);
    if (existingModule) {
        // Merge version range and required property. These will be checked again
        // after probes resolving.
        if (existingModule->name.toString() != StringConstants::qbsModule())
            updateModule(*existingModule, dependency);

        QBS_CHECK(existingModule->item);
        moduleItem = existingModule->item;
        const auto matcher = [loadingItem](const Item::Module::LoadContext &context) {
            return context.loadingItem() == loadingItem;
        };
        const auto it = std::find_if(existingModule->loadContexts.begin(),
                                     existingModule->loadContexts.end(), matcher);
        if (it == existingModule->loadContexts.end())
            addLoadContext(*existingModule);
        else
            it->parameters.first = mergeDependencyParameters(it->parameters.first,
                                                             dependency.parameters);
    } else if (dependency.product) {
        productDep = dependency.product; // We have already done the look-up.
    } else if (productDep = findMatchingProduct(dependency); !productDep) {
        moduleItem = findMatchingModule(dependency);
    }

    if (productDep) {
        const auto checkResult = checkProductDependency(dependency, *productDep);

        // We found a product dependency, but that product has not been handled yet,
        // so stop dependency resolving for the current product and resume it later, when the
        // dependency is ready.
        if (checkResult.second == HandleDependency::Defer)
            return {nullptr, productDep, HandleDependency::Defer};

        if (checkResult.first) {
            QBS_CHECK(productDep->mergedExportItem);
            moduleItem = productDep->mergedExportItem->clone(m_loaderState.itemPool());
            moduleItem->setParent(nullptr);

            // Needed for isolated Export item evaluation.
            moduleItem->setPrototype(productDep->mergedExportItem);
        } else {
            // The product dependency is faulty, but Depends.reqired was false.
            productDep = nullptr;
        }
    }

    if (moduleItem)
        checkModule(dependency, moduleItem, productDep);

    // Can only happen with multiplexing.
    if (moduleWithSameName && moduleWithSameName != moduleItem)
        QBS_CHECK(productDep);

    // The loading name is only used to ensure consistent sorting in case of equal
    // value priorities; see ModulePropertyMerger.
    QString loadingName;
    if (loadingItem == m_product.item) {
        loadingName = m_product.name;
    } else if (m_product.dependenciesContext && !stateStack().empty()) {
        const auto &loadingItemOrigin = stateStack().front().loadingItemOrigin;
        loadingName = loadingItemOrigin.name.toString() + loadingItemOrigin.multiplexId
                      + loadingItemOrigin.profile;
    }
    instantiateModule({m_product, loadingItem, loadingName, moduleItem, moduleWithSameName,
        productDep ? productDep->item : nullptr, dependency.name, dependency.id(),
        bool(existingModule)}, m_loaderState);

    // At this point, a null module item is only possible for a non-required dependency.
    // Note that we still needed to to the instantiation above, as that injects the module
    // name into the surrounding item for the ".present" check.
    if (!moduleItem) {
        QBS_CHECK(!dependency.requiredGlobally);
        return {nullptr, nullptr, HandleDependency::Ignore};
    }

    const auto addLocalModule = [&] {
        if (loadingItem->type() == ItemType::ModuleInstance
            && !findExistingModule(dependency, loadingItem).first) {
            loadingItem->addModule(createModule(dependency, moduleItem, productDep));
        }
    };

    // The module has already been loaded, so we don't need to add it to the product's list of
    // modules, nor do we need to handle its dependencies. The only thing we might need to
    // do is to add it to the "local" list of modules of the loading item, if it isn't in there yet.
    if (existingModule) {
        addLocalModule();
        return {nullptr, nullptr, HandleDependency::Ignore};
    }

    qCDebug(lcModuleLoader) << "module loaded:" << dependency.name.toString();
    if (m_product.item) {
        Item::Module module = createModule(dependency, moduleItem, productDep);
        module.required = dependency.requiredGlobally;
        addLoadContext(module);
        module.maxDependsChainLength = dependsChainLength();
        m_product.item->addModule(module);
        addLocalModule();
    }
    return {moduleItem, nullptr, HandleDependency::Use};
}

std::pair<Item::Module *, Item *> DependenciesResolver::findExistingModule(
    const FullyResolvedDependsItem &dependency, Item *item)
{
    if (!item) // Happens if and only if called via loadBaseModule().
        return {};
    Item *moduleWithSameName = nullptr;
    for (Item::Module &m : item->modules()) {
        if (m.name != dependency.name)
            continue;
        if (!m.product) {
            QBS_CHECK(!dependency.product);
            return {&m, m.item};
        }
        if ((dependency.profile.isEmpty() || (m.product->profileName == dependency.profile))
            && m.product->multiplexConfigurationId == dependency.multiplexId) {
            return {&m, m.item};
        }

        // This can happen if there are dependencies to several variants of a multiplexed
        // product.
        moduleWithSameName = m.item;
    }
    return {nullptr, moduleWithSameName};
}

void DependenciesResolver::updateModule(
    Item::Module &module, const FullyResolvedDependsItem &dependency)
{
    forwardParameterDeclarations(dependency.item, m_product.item->modules());
    module.versionRange.narrowDown(dependency.versionRange);
    module.required |= dependency.requiredGlobally;
    if (dependsChainLength() > module.maxDependsChainLength)
        module.maxDependsChainLength = dependsChainLength();
}

int DependenciesResolver::dependsChainLength()
{
    return m_product.dependenciesContext ? stateStack().size() : 1;
}

ProductContext *DependenciesResolver::findMatchingProduct(
    const FullyResolvedDependsItem &dependency)
{
    const auto constraint = [this, &dependency](ProductContext &product) {
        if (product.multiplexConfigurationId != dependency.multiplexId)
            return false;
        if (!dependency.profile.isEmpty() && dependency.profile != product.profileName)
            return false;
        if (dependency.limitToSubProject && !haveSameSubProject(m_product, product))
            return false;
        return true;

    };
    return m_product.project->topLevelProject->productWithNameAndConstraint(
                dependency.name.toString(), constraint);
}

Item *DependenciesResolver::findMatchingModule(
    const FullyResolvedDependsItem &dependency)
{
    // If we can tell that this is supposed to be a product dependency, we can skip
    // the module look-up.
    if (!dependency.profile.isEmpty() || !dependency.multiplexId.isEmpty()) {
        if (dependency.requiredGlobally) {
            if (!dependency.profile.isEmpty()) {
                throw ErrorInfo(Tr::tr("Product '%1' depends on '%2', which does not exist "
                                       "for the requested profile '%3'.")
                                .arg(m_product.displayName(), dependency.displayName(),
                                         dependency.profile),
                                m_product.item->location());
            }
            throw ErrorInfo(Tr::tr("Product '%1' depends on '%2', which does not exist.")
                            .arg(m_product.displayName(), dependency.displayName()),
                            m_product.item->location());
        }
        return nullptr;
    }

    if (Item *moduleItem = searchAndLoadModuleFile(
            m_loaderState,
            m_product,
            dependency.location(),
            dependency.name,
            dependency.versionRange)) {
        QBS_CHECK(moduleItem->type() == ItemType::Module);
        Item * const proto = moduleItem;
        ModuleItemLocker locker(*moduleItem);
        moduleItem = moduleItem->clone(m_loaderState.itemPool());
        moduleItem->setPrototype(proto); // For parameter declarations.
        return moduleItem;
    }

    if (!dependency.requiredGlobally) {
        return createNonPresentModule(m_loaderState.itemPool(), dependency.name.toString(),
                                      QStringLiteral("not found"), nullptr);
    }

    throw ErrorInfo(Tr::tr("Dependency '%1' not found for product '%2'.")
                        .arg(dependency.name.toString(), m_product.displayName()),
                    dependency.location());
}

std::pair<bool, HandleDependency> DependenciesResolver::checkProductDependency(
    const FullyResolvedDependsItem &depSpec, const ProductContext &dep)
{
    // Optimization: If we already checked the product earlier and then deferred, we don't
    //               need to check it again.
    if (!depSpec.checkProduct)
        return {true, HandleDependency::Use};

    if (&dep == &m_product) {
        throw ErrorInfo(Tr::tr("Dependency '%1' refers to itself.").arg(depSpec.name.toString()),
                        depSpec.location());
    }

    if (m_product.project->topLevelProject->isProductQueuedForHandling(dep)) {
        if (m_deferral == Deferral::Allowed)
            return {false, HandleDependency::Defer};
        ErrorInfo e;
        e.append(Tr::tr("Cyclic dependencies detected:"));
        e.append(Tr::tr("First product is '%1'.")
                 .arg(m_product.displayName()), m_product.item->location());
        e.append(Tr::tr("Second product is '%1'.")
                     .arg(dep.displayName()), dep.item->location());
        e.append(Tr::tr("Requested here."), depSpec.location());
        throw e;
    }

    // This covers both the case of user-disabled products and products with errors.
    // The latter are force-disabled in ProductContext::handleError().
    if (m_product.project->topLevelProject->isDisabledItem(dep.item)) {
        if (depSpec.requiredGlobally) {
            ErrorInfo e;
            e.append(Tr::tr("Product '%1' depends on '%2',")
                     .arg(m_product.displayName(), dep.displayName()),
                     m_product.item->location());
            e.append(Tr::tr("but product '%1' is disabled.").arg(dep.displayName()),
                     dep.item->location());
            throw e;
        }
        return {false, HandleDependency::Ignore};
    }
    return {true, HandleDependency::Use};
}

void DependenciesResolver::checkModule(
    const FullyResolvedDependsItem &dependency, Item *moduleItem, ProductContext *productDep)
{
    // When loading a pseudo or temporary qbs module in early setup via loadBaseModule(),
    // there is no proper state yet.
    if (!m_product.dependenciesContext)
        return;

    for (auto it = stateStack().begin(); it != stateStack().end(); ++it) {
        Item *itemToCheck = moduleItem;
        if (it->loadingItem != itemToCheck) {
            if (!productDep)
                continue;
            itemToCheck = productDep->item;
        }
        if (it->loadingItem != itemToCheck)
            continue;
        ErrorInfo e;
        e.append(Tr::tr("Cyclic dependencies detected:"));
        while (true) {
            e.append(it->loadingItemOrigin.name.toString(),
                     it->loadingItemOrigin.location());
            if (it->loadingItem->type() == ItemType::ModuleInstance) {
                createNonPresentModule(m_loaderState.itemPool(),
                                       it->loadingItemOrigin.name.toString(),
                                       QLatin1String("cyclic dependency"), it->loadingItem);
            }
            if (it == stateStack().begin())
                break;
            --it;
        }
        e.append(dependency.name.toString(), dependency.location());
        throw e;
    }
    checkForModuleNamePrefixCollision(dependency);
}

void DependenciesResolver::adjustDependsItemForMultiplexing(Item *dependsItem)
{
    if (m_product.name.startsWith(StringConstants::shadowProductPrefix()))
        return;

    Evaluator &evaluator = m_loaderState.evaluator();
    const QString name = evaluator.stringValue(dependsItem, StringConstants::nameProperty());
    const bool productIsMultiplexed = !m_product.multiplexConfigurationId.isEmpty();
    if (name == m_product.name) {
        QBS_CHECK(!productIsMultiplexed); // This product must be an aggregator.
        return;
    }

    bool hasNonMultiplexedDependency = false;
    const std::vector<ProductContext *> multiplexedDependencies = m_product.project
            ->topLevelProject->productsWithNameAndConstraint(name, [&hasNonMultiplexedDependency]
                                                             (const ProductContext &product) {
        if (product.multiplexConfigurationId.isEmpty()) {
            hasNonMultiplexedDependency = true;
            return false;
        }
        return true;
    });
    const bool hasMultiplexedDependencies = !multiplexedDependencies.empty();

    // These are the allowed cases:
    // (1) Normal dependency with no multiplexing whatsoever.
    // (2) Both product and dependency are multiplexed.
    //     (2a) The profiles property is not set, we want to depend on the best
    //          matching variant.
    //     (2b) The profiles property is set, we want to depend on all variants
    //          with a matching profile.
    // (3) The product is not multiplexed, but the dependency is.
    //     (3a) The profiles property is not set, the dependency has an aggregator.
    //          We want to depend on the aggregator.
    //     (3b) The profiles property is not set, the dependency does not have an
    //          aggregator. We want to depend on all the multiplexed variants.
    //     (3c) The profiles property is set, we want to depend on all variants
    //          with a matching profile regardless of whether an aggregator exists or not.
    // (4) The product is multiplexed, but the dependency is not. We don't have to adapt
    //     any Depends items.
    // (1) and (4)
    if (!hasMultiplexedDependencies)
        return;

    bool profilesPropertyIsSet;
    const QStringList profiles = evaluator.stringListValue(
        dependsItem, StringConstants::profilesProperty(), &profilesPropertyIsSet);

    // (3a)
    if (!productIsMultiplexed && hasNonMultiplexedDependency && !profilesPropertyIsSet)
        return;

    static const auto multiplexConfigurationIntersects = [](const QVariantMap &lhs,
                                                            const QVariantMap &rhs) {
        QBS_CHECK(!lhs.isEmpty() && !rhs.isEmpty());
        for (auto lhsProperty = lhs.constBegin(); lhsProperty != lhs.constEnd(); lhsProperty++) {
            const auto rhsProperty = rhs.find(lhsProperty.key());
            const bool isCommonProperty = rhsProperty != rhs.constEnd();
            if (isCommonProperty && !qVariantsEqual(lhsProperty.value(), rhsProperty.value()))
                return false;
        }
        return true;
    };

    QStringList multiplexIds;
    const auto productMultiplexConfig = m_loaderState.topLevelProject().multiplexConfiguration(
                m_product.multiplexConfigurationId);

    for (const ProductContext *dependency : multiplexedDependencies) {
        if (productIsMultiplexed && !profilesPropertyIsSet) { // 2a
            if (dependency->multiplexConfigurationId == m_product.multiplexConfigurationId) {
                const ValuePtr &multiplexId = m_product.item->property(
                    StringConstants::multiplexConfigurationIdProperty());
                dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                                         multiplexId);
                return;

            }
            // Otherwise collect partial matches and decide later
            const auto dependencyMultiplexConfig = m_loaderState.topLevelProject()
                    .multiplexConfiguration(dependency->multiplexConfigurationId);

            if (multiplexConfigurationIntersects(dependencyMultiplexConfig, productMultiplexConfig))
                multiplexIds << dependency->multiplexConfigurationId;
        } else {
            // (2b), (3b) or (3c)
            const bool profileMatch = !profilesPropertyIsSet || profiles.empty()
                                      || profiles.contains(dependency->profileName);
            if (profileMatch)
                multiplexIds << dependency->multiplexConfigurationId;
        }
    }
    if (multiplexIds.empty()) {
        const QString productName = m_product.displayName();
        throw ErrorInfo(Tr::tr("Dependency from product '%1' to product '%2' not fulfilled. "
                               "There are no eligible multiplex candidates.").arg(productName,
                                 name),
                        dependsItem->location());
    }

    // In case of (2a), at most 1 match is allowed
    if (productIsMultiplexed && !profilesPropertyIsSet && multiplexIds.size() > 1) {
        QStringList candidateNames;
        for (const auto &id : std::as_const(multiplexIds))
            candidateNames << fullProductDisplayName(name, id);
        throw ErrorInfo(
            Tr::tr("Dependency from product '%1' to product '%2' is ambiguous. "
                   "Eligible multiplex candidates: %3.").arg(
                        m_product.displayName(), name, candidateNames.join(QLatin1String(", "))),
            dependsItem->location());
    }

    dependsItem->setProperty(StringConstants::multiplexConfigurationIdsProperty(),
                             VariantValue::create(multiplexIds));

}

std::optional<EvaluatedDependsItem> DependenciesResolver::evaluateDependsItem(Item *item)
{
    Evaluator &evaluator = m_loaderState.evaluator();
    for (Item *current = item;
         current->type() == ItemType::Depends || current->type() == ItemType::Group;
         current = current->parent()) {
        if (!m_loaderState.topLevelProject().checkItemCondition(
                current, m_loaderState.evaluator())) {
            qCDebug(lcModuleLoader) << "Depends item disabled, ignoring.";
            return {};
        }
    }

    const QString name = evaluator.stringValue(item, StringConstants::nameProperty());
    if (name == StringConstants::qbsModule()) // Redundant
        return {};

    bool submodulesPropertySet;
    const QStringList submodules = evaluator.stringListValue(
        item, StringConstants::submodulesProperty(), &submodulesPropertySet);
    if (submodules.empty() && submodulesPropertySet) {
        qCDebug(lcModuleLoader) << "Ignoring Depends item with empty submodules list.";
        return {};
    }
    if (Q_UNLIKELY(submodules.size() > 1 && !item->id().isEmpty())) {
        QString msg = Tr::tr("A Depends item with more than one module cannot have an id.");
        throw ErrorInfo(msg, item->location());
    }
    bool productTypesWasSet;
    const QStringList productTypes = evaluator.stringListValue(
        item, StringConstants::productTypesProperty(), &productTypesWasSet);
    if (!name.isEmpty() && !productTypes.isEmpty()) {
        throw ErrorInfo(Tr::tr("The name and productTypes are mutually exclusive "
                               "in a Depends item."), item->location());
    }
    if (productTypes.isEmpty() && productTypesWasSet) {
        qCDebug(lcModuleLoader) << "Ignoring Depends item with empty productTypes list.";
        return {};
    }
    if (name.isEmpty() && !productTypesWasSet) {
        throw ErrorInfo(Tr::tr("Either name or productTypes must be set in a Depends item"),
                        item->location());
    }

    bool profilesPropertyWasSet = false;
    std::optional<QStringList> profiles;
    bool required = true;
    if (productTypes.isEmpty()) {
        const QStringList profileList = evaluator.stringListValue(
            item, StringConstants::profilesProperty(), &profilesPropertyWasSet);
        if (profilesPropertyWasSet)
            profiles.emplace(profileList);
        required = evaluator.boolValue(item, StringConstants::requiredProperty());
    }
    const Version minVersion = Version::fromString(
        evaluator.stringValue(item, StringConstants::versionAtLeastProperty()));
    const Version maxVersion = Version::fromString(
        evaluator.stringValue(item, StringConstants::versionBelowProperty()));
    const bool limitToSubProject = evaluator.boolValue(
        item, StringConstants::limitToSubProjectProperty());
    const QStringList multiplexIds = evaluator.stringListValue(
        item, StringConstants::multiplexConfigurationIdsProperty());
    adjustParametersScopes(item, item);
    forwardParameterDeclarations(item, m_product.item->modules());
    const QVariantMap parameters = extractParameters(item);

    const FileTags productTypeTags = FileTags::fromStringList(productTypes);
    if (!productTypeTags.empty())
        m_product.bulkDependencies.emplace_back(productTypeTags, item->location());
    return EvaluatedDependsItem{
        item,
        QualifiedId::fromString(name),
        submodules,
        productTypeTags,
        multiplexIds,
        profiles,
        {minVersion, maxVersion},
        parameters,
        limitToSubProject,
        required};
}

// Potentially multiplexes a dependency along Depends.productTypes, Depends.subModules and
// Depends.profiles, as well as internally set up multiplexing axes.
// Each entry in the resulting queue corresponds to exactly one product or module to pull in.
std::queue<FullyResolvedDependsItem>
DependenciesResolver::multiplexDependency(const EvaluatedDependsItem &dependency)
{
    std::queue<FullyResolvedDependsItem> dependencies;
    if (!dependency.productTypes.empty()) {
        const auto constraint = [&](const ProductContext &product) {
            return &product != &m_product && product.name != m_product.name
                    && (!dependency.limitToSubProject || haveSameSubProject(m_product, product));
        };
        const std::vector<ProductContext *> matchingProducts = m_product.project->topLevelProject
                ->productsWithTypeAndConstraint(dependency.productTypes, constraint);
        if (matchingProducts.empty()) {
            qCDebug(lcModuleLoader) << "Depends.productTypes does not match anything."
                                    << dependency.item->location();
            return {};
        }
        for (ProductContext * const match : matchingProducts)
            dependencies.emplace(match, dependency);
        return dependencies;
    }

    const QStringList profiles = dependency.profiles && !dependency.profiles->isEmpty()
                                     ? *dependency.profiles : QStringList(QString());
    const QStringList multiplexIds = !dependency.multiplexIds.isEmpty()
                                         ? dependency.multiplexIds : QStringList(QString());
    const QStringList subModules = !dependency.subModules.isEmpty()
                                       ? dependency.subModules : QStringList(QString());
    for (const QString &profile : profiles) {
        for (const QString &multiplexId : multiplexIds) {
            for (const QString &subModule : subModules) {
                QualifiedId name = dependency.name;
                if (!subModule.isEmpty())
                    name << subModule.split(QLatin1Char('.'));
                dependencies.emplace(dependency, name, profile, multiplexId);
            }
        }
    }
    return dependencies;
}

QVariantMap DependenciesResolver::extractParameters(Item *dependsItem) const
{
    try {
        QVariantMap result;
        const auto &properties = dependsItem->properties();
        Evaluator &evaluator = m_loaderState.evaluator();
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            const JSValue sv = evaluator.scriptValue(
                        std::static_pointer_cast<ItemValue>(it.value())->item());
            result.insert(it.key(), safeToVariant(evaluator.engine()->context(), sv));
        }
        return result;
    } catch (const ErrorInfo &exception) {
        auto ei = exception;
        ei.prepend(Tr::tr("Error in dependency parameter."), dependsItem->location());
        throw ei;
    }
}

void DependenciesResolver::forwardParameterDeclarations(const Item *dependsItem,
                                                            const Item::Modules &modules)
{
    for (auto it = dependsItem->properties().begin(); it != dependsItem->properties().end(); ++it) {
        if (it.value()->type() != Value::ItemValueType)
            continue;
        forwardParameterDeclarations(it.key(),
                                     std::static_pointer_cast<ItemValue>(it.value())->item(),
                                     modules);
    }
}

void DependenciesResolver::forwardParameterDeclarations(
        const QualifiedId &moduleName, Item *item, const Item::Modules &modules)
{
    auto it = std::find_if(modules.begin(), modules.end(), [&moduleName] (const Item::Module &m) {
        return m.name == moduleName;
    });
    if (it != modules.end()) {
        item->setPropertyDeclarations(m_loaderState.topLevelProject().parameterDeclarations(
                                          it->item->rootPrototype()));
    } else {
        for (auto it = item->properties().begin(); it != item->properties().end(); ++it) {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            forwardParameterDeclarations(QualifiedId(moduleName) << it.key(),
                                         std::static_pointer_cast<ItemValue>(it.value())->item(),
                                         modules);
        }
    }
}

std::list<DependenciesResolvingState> &DependenciesResolver::stateStack()
{
    QBS_CHECK(m_product.dependenciesContext);
    return static_cast<DependenciesContextImpl *>(m_product.dependenciesContext.get())->stateStack;
}

void DependenciesResolver::checkForModuleNamePrefixCollision(
    const FullyResolvedDependsItem &dependency)
{
    if (!m_product.item)
        return;

    for (const Item::Module &m : m_product.item->modules()) {
        if (m.name.length() == dependency.name.length())
            continue;

        QualifiedId shortName;
        QualifiedId longName;
        if (m.name.length() < dependency.name.length()) {
            shortName = m.name;
            longName = dependency.name;
        } else {
            shortName = dependency.name;
            longName = m.name;
        }
        const auto isPrefix = [&] {
            for (int i = 0; i < shortName.length(); ++i) {
                if (shortName.at(i) != longName.at(i))
                    return false;
            }
            return true;
        };
        if (!isPrefix())
            continue;

        throw ErrorInfo(Tr::tr("The name of module '%1' is a prefix of the name of module '%2', "
                               "which is not allowed")
                            .arg(shortName.toString(), longName.toString()), dependency.location());
    }
}

Item::Module DependenciesResolver::createModule(
    const FullyResolvedDependsItem &dependency, Item *item, ProductContext *productDep)
{
    Item::Module m;
    m.item = item;
    m.product = productDep;
    m.name = dependency.name;
    m.required = dependency.requiredLocally;
    m.versionRange = dependency.versionRange;
    return m;
}

FullyResolvedDependsItem::FullyResolvedDependsItem(
    ProductContext *product, const EvaluatedDependsItem &dependency)
    : product(product)
    , item(dependency.item)
    , name(product->name)
    , versionRange(dependency.versionRange)
    , parameters(dependency.parameters)
    , checkProduct(false)
{}

FullyResolvedDependsItem FullyResolvedDependsItem::makeBaseDependency()
{
    FullyResolvedDependsItem item;
    item.name = StringConstants::qbsModule();
    return item;
}

FullyResolvedDependsItem::FullyResolvedDependsItem(
    const EvaluatedDependsItem &dependency, QualifiedId name, QString profile, QString multiplexId)
    : item(dependency.item)
    , name(std::move(name))
    , profile(std::move(profile))
    , multiplexId(std::move(multiplexId))
    , versionRange(dependency.versionRange)
    , parameters(dependency.parameters)
    , limitToSubProject(dependency.limitToSubProject)
    , requiredLocally(dependency.requiredLocally)
    , requiredGlobally(dependency.requiredGlobally)
{}

QString FullyResolvedDependsItem::id() const
{
    if (!item) {
        QBS_CHECK(name.toString() == StringConstants::qbsModule());
        return {};
    }
    return item->id();
}

CodeLocation FullyResolvedDependsItem::location() const
{
    if (!item) {
        QBS_CHECK(name.toString() == StringConstants::qbsModule());
        return {};
    }
    return item->location();
}

QString FullyResolvedDependsItem::displayName() const
{
    return fullProductDisplayName(name.toString(), multiplexId);
}

bool haveSameSubProject(const ProductContext &p1, const ProductContext &p2)
{
    for (const Item *otherParent = p2.item->parent(); otherParent;
         otherParent = otherParent->parent()) {
        if (otherParent == p1.item->parent())
            return true;
    }
    return false;
}

QVariantMap safeToVariant(JSContext *ctx, const JSValue &v)
{
    QVariantMap result;
    handleJsProperties(ctx, v, [&](const JSAtom &prop, const JSPropertyDescriptor &desc) {
        const JSValue u = desc.value;
        if (JS_IsError(ctx, u))
            throw ErrorInfo(getJsString(ctx, u));
        const QString name = getJsString(ctx, prop);
        result[name] = (JS_IsObject(u) && !JS_IsArray(ctx, u) && !JS_IsRegExp(u))
                           ? safeToVariant(ctx, u)
                           : getJsVariant(ctx, u);
    });
    return result;
}

void collectDependsItems(Item *parent, std::queue<Item *> &dependsItems)
{
    for (Item * const child : parent->children()) {
        if (child->type() == ItemType::Depends)
            dependsItems.push(child);
        else if (child->type() == ItemType::Group)
            collectDependsItems(child, dependsItems);
    }
}

DependenciesContextImpl::DependenciesContextImpl(ProductContext &product, LoaderState &loaderState)
    : m_product(product)
{
    setSearchPathsForProduct(loaderState);

    // Initialize the state with the direct Depends items of the product item.
    DependenciesResolvingState newState{product.item,};
    collectDependsItems(product.item, newState.pendingDependsItems);
    stateStack.push_front(std::move(newState));
    stateStack.front().pendingResolvedDependencies.push(
        FullyResolvedDependsItem::makeBaseDependency());
}

std::pair<ProductDependency, ProductContext *> DependenciesContextImpl::pendingDependency() const
{
    QBS_CHECK(!stateStack.empty());
    if (const auto &currentDependsItem = stateStack.front().currentDependsItem;
        currentDependsItem && !currentDependsItem->productTypes.empty()) {
        qCDebug(lcLoaderScheduling) << "product" << m_product.displayName()
                                    << "to be delayed because of bulk dependency";
        return {ProductDependency::Bulk, nullptr};
    }
    if (!stateStack.front().pendingResolvedDependencies.empty()) {
        if (ProductContext * const dep = stateStack.front().pendingResolvedDependencies
                .front().product) {
            if (m_product.project->topLevelProject->isProductQueuedForHandling(*dep)) {
                qCDebug(lcLoaderScheduling) << "product" << m_product.displayName()
                                            << "to be delayed because of dependency "
                                               "to unfinished product" << dep->displayName();
                return {ProductDependency::Single, dep};
            } else {
                qCDebug(lcLoaderScheduling) << "product" << m_product.displayName()
                                            << "to be re-scheduled, as dependency "
                                            << dep->displayName()
                                            << "appears to have finished in the meantime";
                return {ProductDependency::None, dep};
            }
        }
    }
    return {ProductDependency::None, nullptr};
}

void DependenciesContextImpl::setSearchPathsForProduct(LoaderState &loaderState)
{
    QBS_CHECK(m_product.searchPaths.isEmpty());

    m_product.searchPaths = loaderState.itemReader().readExtraSearchPaths(m_product.item);
    Settings settings(loaderState.parameters().settingsDirectory());
    const QStringList prefsSearchPaths = Preferences(&settings, m_product.profileModuleProperties)
            .searchPaths();
    const QStringList &currentSearchPaths = loaderState.itemReader().allSearchPaths();
    for (const QString &p : prefsSearchPaths) {
        if (!currentSearchPaths.contains(p) && FileInfo(p).exists())
            m_product.searchPaths << p;
    }
}

} // namespace
} // namespace qbs::Internal
