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

#include "modulemerger.h"

#include "value.h"

#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stlutils.h>

namespace qbs {
namespace Internal {

ModuleMerger::ModuleMerger(Logger &logger, Item *root, Item::Module &moduleToMerge)
    : m_logger(logger)
    , m_rootItem(root)
    , m_mergedModule(moduleToMerge)
    , m_required(moduleToMerge.required)
    , m_versionRange(moduleToMerge.versionRange)
{
    QBS_CHECK(moduleToMerge.item->type() == ItemType::ModuleInstance);
}

void ModuleMerger::replaceItemInValues(QualifiedId moduleName, Item *containerItem, Item *toReplace)
{
    QBS_CHECK(!moduleName.isEmpty());
    QBS_CHECK(containerItem != m_mergedModule.item);
    const QString moduleNamePrefix = moduleName.takeFirst();
    const Item::PropertyMap &properties = containerItem->properties();
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (it.key() != moduleNamePrefix)
            continue;
        Value * const val = it.value().get();
        QBS_CHECK(val);
        QBS_CHECK(val->type() == Value::ItemValueType);
        ItemValue * const itemVal = static_cast<ItemValue *>(val);
        if (moduleName.isEmpty()) {
            QBS_CHECK(itemVal->item() == toReplace);
            itemVal->setItem(m_mergedModule.item);
        } else {
            replaceItemInValues(moduleName, itemVal->item(), toReplace);
        }
    }
}

void ModuleMerger::replaceItemInScopes(Item *toReplace)
{
    // In insertProperties(), we potentially call setDefiningItem() with the "wrong"
    // (to-be-replaced) module instance as an argument. If such module instances
    // are dependencies of other modules, they have the depending module's instance
    // as their "instance scope", which is the scope of their scope. This function takes
    // care that the "wrong" definingItem of values in sub-modules still has the "right"
    // instance scope, namely our merged module instead of some other instance.
    for (const Item::Module &module : toReplace->modules()) {
        for (const ValuePtr &property : module.item->properties()) {
            ValuePtr v = property;
            do {
                if (v->definingItem() && v->definingItem()->scope()
                        && v->definingItem()->scope()->scope() == toReplace) {
                    v->definingItem()->scope()->setScope(m_mergedModule.item);
                }
                v = v->next();
            } while (v);
        }
    }
}

void ModuleMerger::start()
{
    Item::Module m;
    m.item = m_rootItem;
    const Item::PropertyMap props = dfs(m, Item::PropertyMap());
    if (m_required)
        m_mergedModule.required = true;
    m_mergedModule.versionRange.narrowDown(m_versionRange);
    Item::PropertyMap mergedProps = m_mergedModule.item->properties();

    Item *moduleProto = m_mergedModule.item->prototype();
    while (moduleProto->prototype())
        moduleProto = moduleProto->prototype();

    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        appendPrototypeValueToNextChain(moduleProto, it.key(), it.value());
        mergedProps[it.key()] = it.value();
    }
    m_mergedModule.item->setProperties(mergedProps);

    for (Item *moduleInstanceContainer : qAsConst(m_moduleInstanceContainers)) {
        Item::Modules modules;
        for (const Item::Module &dep : moduleInstanceContainer->modules()) {
            const bool isTheModule = dep.name == m_mergedModule.name;
            Item::Module m = dep;
            if (isTheModule && m.item != m_mergedModule.item) {
                QBS_CHECK(m.item->type() == ItemType::ModuleInstance);
                replaceItemInValues(m.name, moduleInstanceContainer, m.item);
                replaceItemInScopes(m.item);
                m.item = m_mergedModule.item;
                if (m_required)
                    m.required = true;
                m.versionRange.narrowDown(m_versionRange);
            }
            modules << m;
        }
        moduleInstanceContainer->setModules(modules);
    }
}

Item::PropertyMap ModuleMerger::dfs(const Item::Module &m, Item::PropertyMap props)
{
    Item *moduleInstance = 0;
    size_t numberOfOutprops = m.item->modules().size();
    for (const Item::Module &dep : m.item->modules()) {
        if (dep.name == m_mergedModule.name) {
            --numberOfOutprops;
            moduleInstance = dep.item;
            insertProperties(&props, moduleInstance, ScalarProperties);
            m_moduleInstanceContainers << m.item;
            if (dep.required)
                m_required = true;
            m_versionRange.narrowDown(dep.versionRange);
            break;
        }
    }

    std::vector<Item::PropertyMap> outprops;
    outprops.reserve(numberOfOutprops);
    for (const Item::Module &dep : m.item->modules()) {
        if (dep.item != moduleInstance)
            outprops.push_back(dfs(dep, props));
    }

    if (!outprops.empty()) {
        props = outprops.front();
        for (size_t i = 1; i < outprops.size(); ++i)
            mergeOutProps(&props, outprops.at(i));
    }

    if (moduleInstance)
        insertProperties(&props, moduleInstance, ListProperties);

    return props;
}

void ModuleMerger::mergeOutProps(Item::PropertyMap *dst, const Item::PropertyMap &src)
{
    for (auto it = src.constBegin(); it != src.constEnd(); ++it) {
        ValuePtr &v = (*dst)[it.key()];
        if (!v) {
            v = it.value();
            QBS_ASSERT(it.value(), continue);
            continue;
        }
        // possible conflict
        JSSourceValuePtr dstVal = std::dynamic_pointer_cast<JSSourceValue>(v);
        if (!dstVal)
            continue;
        JSSourceValuePtr srcVal = std::dynamic_pointer_cast<JSSourceValue>(it.value());
        if (!srcVal)
            continue;

        const PropertyDeclaration pd = m_decls.value(srcVal);
        QBS_CHECK(pd.isValid());

        if (pd.isScalar()) {
            if (dstVal->sourceCode() != srcVal->sourceCode()) {
                m_logger.qbsWarning() << Tr::tr("Conflicting scalar values at %1 and %2.").arg(
                                             dstVal->location().toString(),
                                             srcVal->location().toString());
                // TODO: yield error with a hint how to solve the conflict.
            }
            v = it.value();
        } else {
            lastInNextChain(dstVal)->setNext(srcVal);
        }
    }
}

void ModuleMerger::insertProperties(Item::PropertyMap *dst, Item *srcItem, PropertiesType type)
{
    Set<const Item *> &seenInstances = type == ScalarProperties
            ? m_seenInstancesTopDown : m_seenInstancesBottomUp;
    Item *origSrcItem = srcItem;
    do {
        if (seenInstances.insert(srcItem).second) {
            for (Item::PropertyMap::const_iterator it = srcItem->properties().constBegin();
                 it != srcItem->properties().constEnd(); ++it) {
                const ValuePtr &srcVal = it.value();
                if (srcVal->type() != Value::JSSourceValueType)
                    continue;
                const PropertyDeclaration srcDecl = srcItem->propertyDeclaration(it.key());
                if (!srcDecl.isValid() || srcDecl.isScalar() != (type == ScalarProperties))
                    continue;
                ValuePtr &v = (*dst)[it.key()];
                if (v && type == ScalarProperties)
                    continue;
                ValuePtr clonedVal = srcVal->clone();
                m_decls[clonedVal] = srcDecl;
                clonedVal->setDefiningItem(origSrcItem);
                if (v) {
                    QBS_CHECK(!clonedVal->next());
                    clonedVal->setNext(v);
                }
                v = clonedVal;
            }
        }
        srcItem = srcItem->prototype();
    } while (srcItem && srcItem->type() == ItemType::ModuleInstance);
}

void ModuleMerger::appendPrototypeValueToNextChain(Item *moduleProto, const QString &propertyName,
        const ValuePtr &sv)
{
    const PropertyDeclaration pd = m_mergedModule.item->propertyDeclaration(propertyName);
    if (pd.isScalar())
        return;
    if (!m_clonedModulePrototype) {
        m_clonedModulePrototype = Item::create(moduleProto->pool(), ItemType::Module);
        m_clonedModulePrototype->setScope(m_mergedModule.item);
        m_clonedModulePrototype->setLocation(moduleProto->location());
        moduleProto->copyProperty(QStringLiteral("name"), m_clonedModulePrototype);
    }
    const ValuePtr &protoValue = moduleProto->property(propertyName);
    QBS_CHECK(protoValue);
    const ValuePtr clonedValue = protoValue->clone();
    lastInNextChain(sv)->setNext(clonedValue);
    clonedValue->setDefiningItem(m_clonedModulePrototype);
    m_clonedModulePrototype->setPropertyDeclaration(propertyName, pd);
    m_clonedModulePrototype->setProperty(propertyName, clonedValue);
}

ValuePtr ModuleMerger::lastInNextChain(const ValuePtr &v)
{
    ValuePtr n = v;
    while (n->next())
        n = n->next();
    return n;
}

} // namespace Internal
} // namespace qbs
