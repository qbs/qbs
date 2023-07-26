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

#include "groupshandler.h"

#include "loaderutils.h"
#include "moduleinstantiator.h"

#include <language/evaluator.h>
#include <language/item.h>
#include <language/value.h>
#include <logging/translator.h>
#include <tools/profiling.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

namespace qbs::Internal {
class GroupsHandler
{
public:
    GroupsHandler(ProductContext &product, LoaderState &loaderState)
        : m_product(product), m_loaderState(loaderState) {}

    void run();

private:
    void gatherAssignedProperties(ItemValue *iv, const QualifiedId &prefix,
                                  QualifiedIdSet &properties);
    void markModuleTargetGroups(Item *group, const Item::Module &module);
    void moveGroupsFromModuleToProduct(const Item::Module &module);
    void moveGroupsFromModulesToProduct();
    void propagateModulesFromParent(Item *group);
    void handleGroup(Item *group);
    void adjustScopesInGroupModuleInstances(Item *groupItem, const Item::Module &module);
    QualifiedIdSet gatherModulePropertiesSetInGroup(const Item *group);

    ProductContext &m_product;
    LoaderState &m_loaderState;
};

void setupGroups(ProductContext &product, LoaderState &loaderState)
{
    GroupsHandler(product, loaderState).run();
}

void GroupsHandler::run()
{
    AccumulatingTimer timer(m_loaderState.parameters().logElapsedTime()
                            ? &m_product.timingData.groupsSetup : nullptr);

    moveGroupsFromModulesToProduct();
    for (Item * const child : m_product.item->children()) {
        if (child->type() == ItemType::Group)
            handleGroup(child);
    }
}

void GroupsHandler::gatherAssignedProperties(ItemValue *iv, const QualifiedId &prefix,
                                                      QualifiedIdSet &properties)
{
    const Item::PropertyMap &props = iv->item()->properties();
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        switch (it.value()->type()) {
        case Value::JSSourceValueType:
            properties << (QualifiedId(prefix) << it.key());
            break;
        case Value::ItemValueType:
            if (iv->item()->type() == ItemType::ModulePrefix) {
                gatherAssignedProperties(std::static_pointer_cast<ItemValue>(it.value()).get(),
                                         QualifiedId(prefix) << it.key(), properties);
            }
            break;
        default:
            break;
        }
    }
}

void GroupsHandler::markModuleTargetGroups(Item *group, const Item::Module &module)
{
    QBS_CHECK(group->type() == ItemType::Group);
    if (m_loaderState.evaluator().boolValue(group, StringConstants::filesAreTargetsProperty())) {
        group->setProperty(StringConstants::modulePropertyInternal(),
                           VariantValue::create(module.name.toString()));
    }
    for (Item * const child : group->children())
        markModuleTargetGroups(child, module);
}

void GroupsHandler::moveGroupsFromModuleToProduct(const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;
    for (auto it = module.item->children().begin(); it != module.item->children().end();) {
        Item * const child = *it;
        if (child->type() != ItemType::Group) {
            ++it;
            continue;
        }
        child->setScope(m_product.scope);
        setScopeForDescendants(child, m_product.scope);
        Item::addChild(m_product.item, child);
        markModuleTargetGroups(child, module);
        it = module.item->children().erase(it);
    }
}

void GroupsHandler::moveGroupsFromModulesToProduct()
{
    for (const Item::Module &module : m_product.item->modules())
        moveGroupsFromModuleToProduct(module);
}

// TODO: I don't completely understand this function, and I suspect we do both too much
//       and too little here. In particular, I'm not sure why we should even have to do anything
//       with groups that don't attach properties.
//       Set aside a day or two at some point to fully grasp all the details and rewrite accordingly.
void GroupsHandler::propagateModulesFromParent(Item *group)
{
    QBS_CHECK(group->type() == ItemType::Group);
    QHash<QualifiedId, Item *> moduleInstancesForGroup;

    // Step 1: "Instantiate" the product's modules for the group.
    for (Item::Module m : group->parent()->modules()) {
        Item * const targetItem = retrieveModuleInstanceItem(group, m.name, m_loaderState);
        QBS_CHECK(targetItem->type() == ItemType::ModuleInstancePlaceholder);
        targetItem->setPrototype(m.item);

        Item * const moduleScope = Item::create(&m_loaderState.itemPool(), ItemType::Scope);
        moduleScope->setFile(group->file());
        moduleScope->setProperties(m.item->scope()->properties()); // "project", "product", ids
        moduleScope->setScope(group);
        targetItem->setScope(moduleScope);

        targetItem->setFile(m.item->file());

        // "parent" should point to the group/artifact parent
        targetItem->setParent(group->parent());

        targetItem->setOuterItem(m.item);

        m.item = targetItem;
        group->addModule(m);
        moduleInstancesForGroup.insert(m.name, targetItem);
    }

    // Step 2: Make the inter-module references point to the instances created in step 1.
    for (const Item::Module &module : group->modules()) {
        Item::Modules adaptedModules;
        const Item::Modules &oldModules = module.item->prototype()->modules();
        for (Item::Module depMod : oldModules) {
            depMod.item = moduleInstancesForGroup.value(depMod.name);
            adaptedModules << depMod;
            if (depMod.name.front() == module.name.front())
                continue;
            const ItemValuePtr &modulePrefix = group->itemProperty(depMod.name.front(),
                                                                   m_loaderState.itemPool());
            QBS_CHECK(modulePrefix);
            module.item->setProperty(depMod.name.front(), modulePrefix);
        }
        module.item->setModules(adaptedModules);
    }

    const QualifiedIdSet &propsSetInGroup = gatherModulePropertiesSetInGroup(group);
    if (propsSetInGroup.empty())
        return;
    m_product.modulePropertiesSetInGroups.insert(std::make_pair(group, propsSetInGroup));

    // Step 3: Adapt scopes in values. This is potentially necessary if module properties
    //         get assigned on the group level.
    for (const Item::Module &module : group->modules())
        adjustScopesInGroupModuleInstances(group, module);
}

void GroupsHandler::handleGroup(Item *group)
{
    propagateModulesFromParent(group);
    m_loaderState.topLevelProject().checkItemCondition(group, m_loaderState.evaluator());
    for (Item * const child : group->children()) {
        if (child->type() == ItemType::Group)
            handleGroup(child);
    }
}

void GroupsHandler::adjustScopesInGroupModuleInstances(Item *groupItem,
                                                                const Item::Module &module)
{
    if (!module.item->isPresentModule())
        return;

    Item *modulePrototype = module.item->rootPrototype();
    QBS_CHECK(modulePrototype->type() == ItemType::Module
              || modulePrototype->type() == ItemType::Export);

    const Item::PropertyDeclarationMap &propDecls = modulePrototype->propertyDeclarations();
    for (const auto &decl : propDecls) {
        const QString &propName = decl.name();

        // Nothing gets inherited for module properties assigned directly in the group.
        // In particular, setting a list property overwrites the value from the product's
        // (or parent group's) instance completely, rather than appending to it
        // (concatenation happens via outer.concat()).
        ValuePtr propValue = module.item->ownProperty(propName);
        if (propValue) {
            propValue->setScope(module.item->scope(), {});
            continue;
        }

        // Find the nearest prototype instance that has the value assigned.
        // The result is either an instance of a parent group (or the parent group's
        // parent group and so on) or the instance of the product or the module prototype.
        // In the latter case, we don't have to do anything.
        const Item *instanceWithProperty = module.item;
        do {
            instanceWithProperty = instanceWithProperty->prototype();
            QBS_CHECK(instanceWithProperty);
            propValue = instanceWithProperty->ownProperty(propName);
        } while (!propValue);
        QBS_CHECK(propValue);

        if (propValue->type() != Value::JSSourceValueType)
            continue;

        if (propValue->scope())
            module.item->setProperty(propName, propValue->clone(m_loaderState.itemPool()));
    }

    for (const ValuePtr &prop : module.item->properties()) {
        if (prop->type() != Value::JSSourceValueType) {
            QBS_CHECK(!prop->next());
            continue;
        }
        for (ValuePtr v = prop; v; v = v->next()) {
            if (!v->scope())
                continue;
            for (const Item::Module &module : groupItem->modules()) {
                if (v->scope() == module.item->prototype()) {
                    v->setScope(module.item, {});
                    break;
                }
            }
        }
    }
}

QualifiedIdSet GroupsHandler::gatherModulePropertiesSetInGroup(const Item *group)
{
    QualifiedIdSet propsSetInGroup;
    const Item::PropertyMap &props = group->properties();
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        if (it.value()->type() == Value::ItemValueType) {
            gatherAssignedProperties(std::static_pointer_cast<ItemValue>(it.value()).get(),
                                     QualifiedId(it.key()), propsSetInGroup);
        }
    }
    return propsSetInGroup;
}

} // namespace qbs::Internal
