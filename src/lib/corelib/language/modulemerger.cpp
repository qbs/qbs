/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

ModuleMerger::ModuleMerger(const Logger &logger, Item *root, Item *moduleToMerge,
        const QualifiedId &moduleName)
    : m_logger(logger)
    , m_rootItem(root)
    , m_mergedModuleItem(moduleToMerge)
    , m_moduleName(moduleName)
{
}

void ModuleMerger::start()
{
    if (!m_mergedModuleItem->isPresentModule())
        return;
    Item::Module m;
    m.item = m_rootItem;
    const Item::PropertyMap props = dfs(m, Item::PropertyMap());
    Item::PropertyMap mergedProps = m_mergedModuleItem->properties();

    Item *moduleProto = m_mergedModuleItem->prototype();
    while (moduleProto->prototype())
        moduleProto = moduleProto->prototype();

    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        appendPrototypeValueToNextChain(moduleProto, it.key(), it.value());
        mergedProps[it.key()] = it.value();
    }
    m_mergedModuleItem->setProperties(mergedProps);
}

Item::PropertyMap ModuleMerger::dfs(const Item::Module &m, Item::PropertyMap props)
{
    Item *moduleInstance = 0;
    int numberOfOutprops = m.item->modules().count();
    foreach (const Item::Module &dep, m.item->modules()) {
        if (dep.name == m_moduleName) {
            --numberOfOutprops;
            moduleInstance = dep.item;
            pushScalarProperties(&props, moduleInstance);
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
        pullListProperties(&props, moduleInstance);

    return props;
}

void ModuleMerger::pushScalarProperties(Item::PropertyMap *dst, Item *srcItem)
{
    Item *origSrcItem = srcItem;
    do {
        if (!m_seenInstancesTopDown.contains(srcItem)) {
            m_seenInstancesTopDown.insert(srcItem);
            for (auto it = srcItem->properties().constBegin();
                 it != srcItem->properties().constEnd(); ++it) {
                const ValuePtr &srcVal = it.value();
                if (srcVal->type() != Value::JSSourceValueType)
                    continue;
                const PropertyDeclaration srcDecl = srcItem->propertyDeclaration(it.key());
                if (!srcDecl.isValid() || !srcDecl.isScalar())
                    continue;
                ValuePtr &v = (*dst)[it.key()];
                if (v)
                    continue;
                ValuePtr clonedVal = srcVal->clone();
                m_decls[clonedVal] = srcDecl;
                clonedVal->setDefiningItem(origSrcItem);
                v = clonedVal;
            }
        }
        srcItem = srcItem->prototype();
    } while (srcItem && srcItem->isModuleInstance());
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
        if (!pd.isValid())
            continue;

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

void ModuleMerger::pullListProperties(Item::PropertyMap *dst, Item *instance)
{
    Item *origInstance = instance;
    do {
        if (!m_seenInstancesBottomUp.contains(instance)) {
            m_seenInstancesBottomUp.insert(instance);
            for (Item::PropertyMap::const_iterator it = instance->properties().constBegin();
                 it != instance->properties().constEnd(); ++it) {
                const ValuePtr &srcVal = it.value();
                if (srcVal->type() != Value::JSSourceValueType)
                    continue;
                const PropertyDeclaration srcDecl = instance->propertyDeclaration(it.key());
                if (!srcDecl.isValid() || srcDecl.isScalar())
                    continue;
                ValuePtr clonedVal = srcVal->clone();
                m_decls[clonedVal] = srcDecl;
                clonedVal->setDefiningItem(origInstance);
                ValuePtr &v = (*dst)[it.key()];
                if (v) {
                    QBS_CHECK(!clonedVal->next());
                    clonedVal->setNext(v);
                }
                v = clonedVal;
            }
        }
        instance = instance->prototype();
    } while (instance && instance->isModuleInstance());
}

void ModuleMerger::appendPrototypeValueToNextChain(Item *moduleProto, const QString &propertyName,
        const ValuePtr &sv)
{
    const PropertyDeclaration pd = m_mergedModuleItem->propertyDeclaration(propertyName);
    if (pd.isScalar())
        return;
    ValuePtr protoValue = moduleProto->property(propertyName);
    if (!protoValue)
        return;
    if (!m_clonedModulePrototype) {
        m_clonedModulePrototype = moduleProto->clone();
        Item * const scope = Item::create(m_clonedModulePrototype->pool());
        scope->setFile(m_clonedModulePrototype->file());
        m_mergedModuleItem->scope()->copyProperty(QLatin1String("project"), scope);
        m_mergedModuleItem->scope()->copyProperty(QLatin1String("product"), scope);
        m_clonedModulePrototype->setScope(scope);
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
