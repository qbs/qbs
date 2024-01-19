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

#include "moduleinstantiator.h"

#include "loaderutils.h"
#include "modulepropertymerger.h"

#include <language/item.h>
#include <language/itempool.h>
#include <language/qualifiedid.h>
#include <language/value.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/profiling.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <utility>

namespace qbs::Internal {

static std::pair<const Item *, Item *>
getOrSetModuleInstanceItem(Item *container, Item *moduleItem, const QualifiedId &moduleName,
                           const QString &id, bool replace, LoaderState &loaderState);

class ModuleInstantiator
{
public:
    ModuleInstantiator(const InstantiationContext &context, LoaderState &loaderState)
        : context(context), loaderState(loaderState) {}

    void instantiate();

private:
    void overrideProperties();
    void setupScope();
    void exchangePlaceholderItem(Item *loadingItem, Item *moduleItemForItemValues);

    const InstantiationContext &context;
    LoaderState &loaderState;
};

void ModuleInstantiator::instantiate()
{
    AccumulatingTimer timer(loaderState.parameters().logElapsedTime()
                            ? &context.product.timingData.moduleInstantiation : nullptr);

    // This part needs to be done only once per module and product, and only if the module
    // was successfully loaded.
    if (context.module && !context.alreadyLoaded) {
        context.module->setType(ItemType::ModuleInstance);
        overrideProperties();
        setupScope();
    }

    // This strange-looking code deals with the fact that our syntax cannot properly handle
    // dependencies on several multiplexed variants of the same product.
    // See getOrSetModuleInstanceItem() below for details.
    Item * const moduleItemForItemValues
        = context.moduleWithSameName ? context.moduleWithSameName
                                     : context.module;

    // Now attach the module instance as an item value to the loading item, potentially
    // evicting a previously attached placeholder item and merging its values into the instance.
    // Note that we potentially do this twice, once for the actual loading item and once
    // for the product item, if the two are different. The reason is this:
    // For convenience, in the product item we allow attaching to properties from indirectly
    // loaded modules. For instance:
    // Product {
    //    Depends { name: "Qt.core" }
    //    cpp.cxxLanguageVersion: "c++20" // Works even though there is no Depends item for cpp
    //  }
    // It's debatable whether that's a good feature, but it has been working (accidentally?)
    // for a long time, and removing it now would break a lot of existing projects.
    exchangePlaceholderItem(context.loadingItem, moduleItemForItemValues);

    if (!context.alreadyLoaded && context.product.item
            && context.product.item != context.loadingItem) {
        exchangePlaceholderItem(context.product.item, moduleItemForItemValues);
    }
}

void ModuleInstantiator::exchangePlaceholderItem(Item *loadingItem, Item *moduleItemForItemValues)
{
    // If we have a module item, set an item value pointing to it as a property on the loading item.
    // Evict a possibly existing placeholder item, and return it to us, so we can merge its values
    // into the instance.
    const auto &[oldItem, newItem] = getOrSetModuleInstanceItem(
        loadingItem, moduleItemForItemValues, context.moduleName, context.id, true, loaderState);

    // The new item always exists, even if we don't have a module item. In that case, the
    // function created a placeholder item for us, which we then have to turn into a
    // non-present module.
    QBS_CHECK(newItem);
    if (!moduleItemForItemValues) {
        createNonPresentModule(loaderState.itemPool(), context.moduleName.toString(),
                               QLatin1String("not found"), newItem);
        return;
    }

    if (!moduleItemForItemValues->isPresentModule())
        return;

    // This will yield false negatives for the case where there is an invalid property attached
    // for a module that is actually found by pkg-config via the fallback provider.
    // However, this is extremely rare compared to the case where the presence of the fallback
    // module simply indicates "not present".
    if (moduleItemForItemValues->isFallbackModule())
        return;

    // If the old and the new items are the same, it means the existing item value already
    // pointed to a module instance (rather than a placeholder).
    // This can happen in two cases:
    //   a) Multiple identical Depends items on the same level (easily possible with inheritance).
    //   b) Dependencies to multiplexed variants of the same product
    //      (see getOrSetModuleInstanceItem() below for details).
    if (oldItem == newItem) {
        QBS_CHECK(oldItem->type() == ItemType::ModuleInstance);
        QBS_CHECK(context.alreadyLoaded || context.exportingProduct);
        return;
    }

    // In all other cases, our request to set the module instance item must have been honored.
    QBS_CHECK(newItem == moduleItemForItemValues);

    // If there was no placeholder item, we don't have to merge any values and are done.
    if (!oldItem)
        return;

    // If an item was replaced, then it must have been a placeholder.
    QBS_CHECK(oldItem->type() == ItemType::ModuleInstancePlaceholder);

    // Prevent setting read-only properties.
    for (auto it = oldItem->properties().cbegin(); it != oldItem->properties().cend(); ++it) {
        const PropertyDeclaration &pd = moduleItemForItemValues->propertyDeclaration(it.key());
        if (pd.flags().testFlag(PropertyDeclaration::ReadOnlyFlag)) {
            throw ErrorInfo(Tr::tr("Cannot set read-only property '%1'.").arg(pd.name()),
                            it.value()->location());
        }
    }

    // Now merge the locally attached values into the actual module instance.
    mergeFromLocalInstance(context.product, loadingItem, context.loadingName, oldItem,
                           moduleItemForItemValues, loaderState);

    // TODO: We'd like to delete the placeholder item here, because it's not
    //       being referenced anymore and there's a lot of them. However, this
    //       is not supported by ItemPool. Investigate the use of std::pmr.
}

Item *retrieveModuleInstanceItem(Item *containerItem, const QualifiedId &name,
                                 LoaderState &loaderState)
{
    return getOrSetModuleInstanceItem(containerItem, nullptr, name, {}, false, loaderState).second;
}

Item *retrieveQbsItem(Item *containerItem, LoaderState &loaderState)
{
    return retrieveModuleInstanceItem(containerItem, StringConstants::qbsModule(), loaderState);
}

void ModuleInstantiator::overrideProperties()
{
    // Users can override module properties on the command line with the
    // modules.<module-name>.<property-name>:<value> syntax.
    // For simplicity and backwards compatibility, qbs properties can also be given without
    // the "modules." prefix, i.e. just qbs.<property-name>:<value>.
    // In addition, users can override module properties just for certain products
    // using the products.<product-name>.<module-name>.<property-name>:<value> syntax.
    // Such product-specific overrides have higher precedence.
    const QString fullName = context.moduleName.toString();
    const QString generalOverrideKey = QStringLiteral("modules.") + fullName;
    const QString perProductOverrideKey = StringConstants::productsOverridePrefix()
                                          + context.product.name + QLatin1Char('.') + fullName;
    const SetupProjectParameters &parameters = loaderState.parameters();
    Logger &logger = loaderState.logger();
    context.module->overrideProperties(parameters.overriddenValuesTree(), generalOverrideKey,
                                       parameters, logger);
    if (fullName == StringConstants::qbsModule()) {
        context.module->overrideProperties(parameters.overriddenValuesTree(), fullName, parameters,
                                           logger);
    }
    context.module->overrideProperties(parameters.overriddenValuesTree(), perProductOverrideKey,
                                       parameters, logger);
}

void ModuleInstantiator::setupScope()
{
    Item * const scope = Item::create(&loaderState.itemPool(), ItemType::Scope);
    QBS_CHECK(context.module->file());
    scope->setFile(context.module->file());
    QBS_CHECK(context.product.project->scope);
    context.product.project->scope->copyProperty(StringConstants::projectVar(), scope);
    if (context.product.scope)
        context.product.scope->copyProperty(StringConstants::productVar(), scope);
    else
        QBS_CHECK(context.moduleName.toString() == StringConstants::qbsModule()); // Dummy product.

    if (!context.module->id().isEmpty())
        scope->setProperty(context.module->id(), ItemValue::create(context.module));
    for (Item * const child : context.module->children()) {
        child->setScope(scope);
        if (!child->id().isEmpty())
            scope->setProperty(child->id(), ItemValue::create(child));
    }
    context.module->setScope(scope);

    if (context.exportingProduct) {
        QBS_CHECK(context.exportingProduct->type() == ItemType::Product);

        const auto exportingProductItemValue = ItemValue::create(context.exportingProduct);
        scope->setProperty(QStringLiteral("exportingProduct"), exportingProductItemValue);

        const auto importingProductItemValue = ItemValue::create(context.product.item);
        scope->setProperty(QStringLiteral("importingProduct"), importingProductItemValue);

        // FIXME: This looks wrong. Introduce exportingProject variable?
        scope->setProperty(StringConstants::projectVar(),
                           ItemValue::create(context.exportingProduct->parent()));

        PropertyDeclaration pd(StringConstants::qbsSourceDirPropertyInternal(),
                               PropertyDeclaration::String, QString(),
                               PropertyDeclaration::PropertyNotAvailableInConfig);
        context.module->setPropertyDeclaration(pd.name(), pd);
        context.module->setProperty(pd.name(), context.exportingProduct->property(
                                                   StringConstants::sourceDirectoryProperty()));
    }
}

void instantiateModule(const InstantiationContext &context, LoaderState &loaderState)
{
    ModuleInstantiator(context, loaderState).instantiate();
}

// This important function deals with retrieving and setting (pseudo-)module instances from/on
// items.
// Use case 1: Suppose we resolve the dependency cpp in module Qt.core, which also contains
//             property bindings for cpp such as "cpp.defines: [...]".
//             The "cpp" part of this binding is represented by an ItemValue whose
//             item is of type ModuleInstancePlaceholder, originally set up by ItemReaderASTVisitor.
//             This function will be called with the actual cpp module item and will
//             replace the placeholder item in the item value. It will also return
//             the placeholder item for subsequent merging of its properties with the
//             ones of the actual module (in ModulePropertyMerger::mergeFromLocalInstance()).
//             If there were no cpp property bindings defined in Qt.core, then we'd still
//             have to replace the placeholder item, because references to "cpp" on the
//             right-hand-side of other properties must refer to the module item.
//             This is the common use of this function as employed by resolveProdsucDependencies().
//             Note that if a product has dependencies on more than one variant of a multiplexed
//             product, these dependencies are competing for the item value property name,
//             i.e. this case is not properly supported by the syntax. You must therefore not
//             export properties from multiplexed products that will be different between the
//             variants. In this function, the condition manifests itself by a module instance
//             being encountered instead of a module instance placeholder, in which case
//             nothing is done at all.
// Use case 2: We inject a fake qbs module into a project item, so qbs properties set in profiles
//             can be accessed in the project level. Doing this is discouraged, and the
//             functionality is kept mostly for backwards compatibility. The moduleItem
//             parameter is null in this case, and the item will be created by the function itself.
// Use case 3: A temporary qbs module is attached to a product during low-level setup, namely
//             in product multiplexing and setting qbs.profile.
// Use case 4: Module propagation to the the Group level.
// In all cases, the first returned item is the existing one, and the second returned item
// is the new one. Depending on the use case, they might be null and might also be the same item.
std::pair<const Item *, Item *> getOrSetModuleInstanceItem(
    Item *container, Item *moduleItem, const QualifiedId &moduleName, const QString &id,
    bool replace, LoaderState &loaderState)
{
    Item *instance = container;
    const QualifiedId itemValueName
        = !id.isEmpty() ? QualifiedId::fromString(id) : moduleName;
    for (int i = 0; i < itemValueName.size(); ++i) {
        const QString &moduleNameSegment = itemValueName.at(i);
        const ValuePtr v = instance->ownProperty(itemValueName.at(i));
        if (v && v->type() == Value::ItemValueType) {
            ItemValue * const itemValue = std::static_pointer_cast<ItemValue>(v).get();
            instance = itemValue->item();
            if (i == itemValueName.size() - 1) {
                if (replace && instance != moduleItem
                    && instance->type() == ItemType::ModuleInstancePlaceholder) {
                    if (!moduleItem) {
                        moduleItem = Item::create(&loaderState.itemPool(),
                                                  ItemType::ModuleInstancePlaceholder);
                    }
                    itemValue->setItem(moduleItem);
                }
                return {instance, itemValue->item()};
            }
        } else {
            Item *newItem = i < itemValueName.size() - 1
                                ? Item::create(&loaderState.itemPool(), ItemType::ModulePrefix)
                                : moduleItem
                                      ? moduleItem
                                      : Item::create(&loaderState.itemPool(),
                                                     ItemType::ModuleInstancePlaceholder);
            instance->setProperty(moduleNameSegment, ItemValue::create(newItem));
            instance = newItem;
        }
    }
    return {nullptr, instance};
}

} // namespace qbs::Internal
