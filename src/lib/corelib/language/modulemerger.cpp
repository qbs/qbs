/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "modulemerger.h"

#include "value.h"

#include <logging/translator.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ModuleMerger::ModuleMerger(const Logger &logger, Item *root, Item::Module &moduleToMerge)
    : m_logger(logger)
    , m_rootItem(root)
    , m_mergedModule(moduleToMerge)
    , m_required(moduleToMerge.required)
{
    QBS_CHECK(moduleToMerge.item->type() == ItemType::ModuleInstance);
}

void ModuleMerger::replaceItemInValues(QualifiedId moduleName, Item *containerItem, Item *toReplace)
{
    QBS_CHECK(!moduleName.isEmpty());
    QBS_CHECK(containerItem != m_mergedModule.item);
    const QString moduleNamePrefix = moduleName.takeFirst();
    Item::PropertyMap properties = containerItem->properties();
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (it.key() != moduleNamePrefix)
            continue;
        Value * const val = it.value().data();
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

void ModuleMerger::start()
{
    Item::Module m;
    m.item = m_rootItem;
    const Item::PropertyMap props = dfs(m, Item::PropertyMap());
    if (m_required)
        m_mergedModule.required = true;
    Item::PropertyMap mergedProps = m_mergedModule.item->properties();

    Item *moduleProto = m_mergedModule.item->prototype();
    while (moduleProto->prototype())
        moduleProto = moduleProto->prototype();

    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        appendPrototypeValueToNextChain(moduleProto, it.key(), it.value());
        mergedProps[it.key()] = it.value();
    }
    m_mergedModule.item->setProperties(mergedProps);

    foreach (Item *moduleInstanceContainer, m_moduleInstanceContainers) {
        QList<Item::Module> modules;
        foreach (const Item::Module &dep, moduleInstanceContainer->modules()) {
            const bool isTheModule = dep.name == m_mergedModule.name;
            Item::Module m = dep;
            if (isTheModule && m.item != m_mergedModule.item) {
                QBS_CHECK(m.item->type() == ItemType::ModuleInstance);
                replaceItemInValues(m.name, moduleInstanceContainer, m.item);
                m.item = m_mergedModule.item;
                if (m_required)
                    m.required = true;
            }
            modules << m;
        }
        moduleInstanceContainer->setModules(modules);
    }
}

Item::PropertyMap ModuleMerger::dfs(const Item::Module &m, Item::PropertyMap props)
{
    Item *moduleInstance = 0;
    int numberOfOutprops = m.item->modules().count();
    foreach (const Item::Module &dep, m.item->modules()) {
        if (dep.name == m_mergedModule.name) {
            --numberOfOutprops;
            moduleInstance = dep.item;
            insertProperties(&props, moduleInstance, ScalarProperties);
            m_moduleInstanceContainers << m.item;
            if (dep.required)
                m_required = true;
            break;
        }
    }

    QVector<Item::PropertyMap> outprops;
    outprops.reserve(numberOfOutprops);
    foreach (const Item::Module &dep, m.item->modules()) {
        if (dep.item != moduleInstance)
            outprops << dfs(dep, props);
    }

    if (!outprops.isEmpty()) {
        props = outprops.first();
        for (int i = 1; i < outprops.count(); ++i)
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
        JSSourceValuePtr dstVal = v.dynamicCast<JSSourceValue>();
        if (!dstVal)
            continue;
        JSSourceValuePtr srcVal = it.value().dynamicCast<JSSourceValue>();
        if (!srcVal)
            continue;

        const PropertyDeclaration pd = m_decls.value(srcVal);
        QBS_CHECK(pd.isValid());

        if (pd.isScalar()) {
            if (dstVal->sourceCode() != srcVal->sourceCode()
                    && dstVal->isInExportItem() == srcVal->isInExportItem()) {
                m_logger.qbsWarning() << Tr::tr("Conflicting scalar values at %1 and %2.").arg(
                                             dstVal->location().toString(),
                                             srcVal->location().toString());
                // TODO: yield error with a hint how to solve the conflict.
            }
            if (!dstVal->isInExportItem())
                v = it.value();
        } else {
            lastInNextChain(dstVal)->setNext(srcVal);
        }
    }
}

void ModuleMerger::insertProperties(Item::PropertyMap *dst, Item *srcItem, PropertiesType type)
{
    QSet<const Item *> &seenInstances
            = type == ScalarProperties ? m_seenInstancesTopDown : m_seenInstancesBottomUp;
    Item *origSrcItem = srcItem;
    do {
        if (!seenInstances.contains(srcItem)) {
            seenInstances.insert(srcItem);
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
    ValuePtr protoValue = moduleProto->property(propertyName);
    QBS_CHECK(protoValue);
    if (!m_clonedModulePrototype) {
        m_clonedModulePrototype = moduleProto->clone();
        m_clonedModulePrototype->setScope(m_mergedModule.item->scope());
    }
    const ValuePtr clonedValue = protoValue->clone();
    clonedValue->setDefiningItem(m_clonedModulePrototype);
    lastInNextChain(sv)->setNext(clonedValue);
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
