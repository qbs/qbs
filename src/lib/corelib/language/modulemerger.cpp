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
#include <tools/stringconstants.h>

namespace qbs {
namespace Internal {

ModuleMerger::ModuleMerger(Logger &logger, Item *productItem, const QString &productName,
                           const Item::Modules::iterator &modulesBegin,
                           const Item::Modules::iterator &modulesEnd)
    : m_logger(logger)
    , m_productItem(productItem)
    , m_mergedModule(*modulesBegin)
    , m_isBaseModule(m_mergedModule.name.first() == StringConstants::qbsModule())
    , m_isShadowProduct(productName.startsWith(StringConstants::shadowProductPrefix()))
    , m_modulesBegin(std::next(modulesBegin))
    , m_modulesEnd(modulesEnd)
{
    QBS_CHECK(modulesBegin->item->type() == ItemType::ModuleInstance);
}

void ModuleMerger::replaceItemInValues(QualifiedId moduleName, Item *containerItem, Item *toReplace)
{
    QBS_CHECK(!moduleName.empty());
    QBS_CHECK(containerItem != m_mergedModule.item);
    const QString moduleNamePrefix = moduleName.takeFirst();
    const Item::PropertyMap &properties = containerItem->properties();
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (it.key() != moduleNamePrefix)
            continue;
        Value * const val = it.value().get();
        QBS_CHECK(val);
        QBS_CHECK(val->type() == Value::ItemValueType);
        const auto itemVal = static_cast<ItemValue *>(val);
        if (moduleName.empty()) {
            QBS_CHECK(itemVal->item() == toReplace);
            itemVal->setItem(m_mergedModule.item);
        } else {
            replaceItemInValues(moduleName, itemVal->item(), toReplace);
        }
    }
}

void ModuleMerger::start()
{
    // Iterate over any module that our product depends on. These modules
    // may depend on m_mergedModule and contribute property assignments.
    Item::PropertyMap props;
    for (auto module = m_modulesBegin; module != m_modulesEnd; module++)
        mergeModule(&props, *module);

    // Module property assignments in the product have the highest priority
    // and are thus prepended.
    Item::Module m;
    m.item = m_productItem;
    mergeModule(&props, m);

    // The module's prototype is the essential unmodified module as loaded
    // from the cache.
    Item *moduleProto = m_mergedModule.item->prototype();
    while (moduleProto->prototype())
        moduleProto = moduleProto->prototype();

    // The prototype item might contain default values which get appended in
    // case of list properties. Scalar properties will only be set if not
    // already specified above.
    Item::PropertyMap mergedProps = m_mergedModule.item->properties();
    for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
        appendPrototypeValueToNextChain(moduleProto, it.key(), it.value());
        mergedProps[it.key()] = it.value();
    }

    m_mergedModule.item->setProperties(mergedProps);

    // Update all sibling instances of the to-be-merged module to behave identical
    // to the merged module.
    for (Item *moduleInstanceContainer : qAsConst(m_moduleInstanceContainers)) {
        Item::Modules modules;
        for (const Item::Module &dep : moduleInstanceContainer->modules()) {
            const bool isTheModule = dep.name == m_mergedModule.name;
            Item::Module m = dep;
            if (isTheModule && m.item != m_mergedModule.item) {
                QBS_CHECK(m.item->type() == ItemType::ModuleInstance);
                replaceItemInValues(m.name, moduleInstanceContainer, m.item);
                m.item = m_mergedModule.item;
                m.required = m_mergedModule.required;
                m.versionRange = m_mergedModule.versionRange;
            }
            modules << m;
        }
        moduleInstanceContainer->setModules(modules);
    }
}

void ModuleMerger::mergeModule(Item::PropertyMap *dstProps, const Item::Module &module)
{
    const Item::Module *dep = findModule(module.item, m_mergedModule.name);
    if (!dep)
        return;

    const bool mergingProductItem = (module.item == m_productItem);
    Item *srcItem = dep->item;
    Item *origSrcItem = srcItem;
    do {
        if (m_seenInstances.insert(srcItem).second) {
            for (auto it = srcItem->properties().constBegin();
                    it != srcItem->properties().constEnd(); ++it) {
                const ValuePtr &srcVal = it.value();
                if (srcVal->type() == Value::ItemValueType)
                    continue;
                if (it.key() == StringConstants::qbsSourceDirPropertyInternal())
                    continue;
                const PropertyDeclaration srcDecl = srcItem->propertyDeclaration(it.key());
                if (!srcDecl.isValid())
                    continue;

                // Scalar variant values could stem from product multiplexing, in which case
                // the merged qbs module instance needs to get that value.
                if (srcVal->type() == Value::VariantValueType
                        && (!srcDecl.isScalar() || !m_isBaseModule)) {
                    continue;
                }

                ValuePtr clonedSrcVal = srcVal->clone();
                clonedSrcVal->setDefiningItem(origSrcItem);

                ValuePtr &dstVal = (*dstProps)[it.key()];
                if (dstVal) {
                    if (srcDecl.isScalar()) {
                        // Scalar properties get replaced.
                        if ((dstVal->type() == Value::JSSourceValueType)
                                && (srcVal->type() == Value::JSSourceValueType)) {
                            // Warn only about conflicting source code values
                            const JSSourceValuePtr dstJsVal =
                                    std::static_pointer_cast<JSSourceValue>(dstVal);
                            const JSSourceValuePtr srcJsVal =
                                    std::static_pointer_cast<JSSourceValue>(srcVal);
                            const bool overriddenInProduct =
                                    m_mergedModule.item->properties().contains(it.key());

                            if (dstJsVal->sourceCode() != srcJsVal->sourceCode()
                                    && !mergingProductItem && !overriddenInProduct
                                    && !m_isShadowProduct) {
                                m_logger.qbsWarning()
                                        << Tr::tr("Conflicting scalar values at %1 and %2.").arg(
                                           dstJsVal->location().toString(),
                                           srcJsVal->location().toString());
                            }
                        }
                    } else {
                        // List properties get prepended
                        QBS_CHECK(!clonedSrcVal->next());
                        clonedSrcVal->setNext(dstVal);
                    }
                }
                dstVal = clonedSrcVal;
            }
        }
        srcItem = srcItem->prototype();
    } while (srcItem && srcItem->type() == ItemType::ModuleInstance);

    // Update dependency constraints
    if (dep->required)
        m_mergedModule.required = true;
    m_mergedModule.versionRange.narrowDown(dep->versionRange);

    // We need to touch the unmerged module instances later once more
    m_moduleInstanceContainers << module.item;
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
        moduleProto->copyProperty(StringConstants::nameProperty(), m_clonedModulePrototype);
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

const Item::Module *ModuleMerger::findModule(const Item *item, const QualifiedId &name)
{
    for (const auto &module : item->modules()) {
        if (module.name == name)
            return &module;
    }
    return nullptr;
}

void ModuleMerger::merge(Logger &logger, Item *product, const QString &productName,
                         Item::Modules *topSortedModules)
{
    for (auto it = topSortedModules->begin(); it != topSortedModules->end(); ++it)
        ModuleMerger(logger, product, productName, it, topSortedModules->end()).start();
}



} // namespace Internal
} // namespace qbs
