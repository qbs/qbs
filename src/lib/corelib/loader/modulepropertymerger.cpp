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

#include "modulepropertymerger.h"

#include "loaderutils.h"

#include <language/evaluator.h>
#include <language/item.h>
#include <language/value.h>
#include <logging/translator.h>
#include <tools/profiling.h>
#include <tools/set.h>
#include <tools/setupprojectparameters.h>

#include <unordered_set>

namespace qbs::Internal {
class ModulePropertyMerger
{
public:
    ModulePropertyMerger(ProductContext &product, LoaderState &loaderState)
        : m_product(product), m_loaderState(loaderState) {}

    void mergeFromLocalInstance(Item *loadingItem, const QString &loadingName,
                                const Item *localInstance, Item *globalInstance);
    void doFinalMerge();

private:
    int compareValuePriorities(const ValueConstPtr &v1, const ValueConstPtr &v2);
    ValuePtr mergeListValues(const ValuePtr &currentHead, const ValuePtr &newElem);
    void mergePropertyFromLocalInstance(Item *loadingItem, const QString &loadingName,
                                        Item *globalInstance, const QString &name,
                                        const ValuePtr &value);
    bool doFinalMerge(Item *moduleItem);
    bool doFinalMerge(const PropertyDeclaration &propertyDecl, ValuePtr &propertyValue);

    ProductContext & m_product;
    LoaderState &m_loaderState;
};

void mergeFromLocalInstance(ProductContext &product, Item *loadingItem, const QString &loadingName,
                            const Item *localInstance, Item *globalInstance,
                            LoaderState &loaderState)
{
    ModulePropertyMerger(product, loaderState).mergeFromLocalInstance(
        loadingItem, loadingName, localInstance, globalInstance);
}

void doFinalMerge(ProductContext &product, LoaderState &loaderState)
{
    ModulePropertyMerger(product, loaderState).doFinalMerge();
}

void ModulePropertyMerger::mergeFromLocalInstance(Item *loadingItem, const QString &loadingName,
                                                  const Item *localInstance, Item *globalInstance)
{
    AccumulatingTimer t(m_loaderState.parameters().logElapsedTime()
                            ? &m_product.timingData.propertyMerging : nullptr);

    for (auto it = localInstance->properties().constBegin();
         it != localInstance->properties().constEnd(); ++it) {
        mergePropertyFromLocalInstance(loadingItem, loadingName, globalInstance, it.key(),
                                       it.value());
    }
}

void ModulePropertyMerger::doFinalMerge()
{
    AccumulatingTimer t(m_loaderState.parameters().logElapsedTime()
                            ? &m_product.timingData.propertyMerging : nullptr);

    std::unordered_set<const Item *> itemsToInvalidate;
    for (const Item::Module &module : m_product.item->modules()) {
        if (doFinalMerge(module.item))
            itemsToInvalidate.insert(module.item);
    }

    // For each module item, if it requires invalidation, we also add modules that depend on
    // that module item, all the way up to the Product item.
    std::unordered_set<const Item *> visitedItems;
    const auto collectDependentItems =
        [&itemsToInvalidate, &visitedItems](const Item *item, const auto &collect) -> bool
    {
        const bool alreadyInSet = itemsToInvalidate.count(item);
        if (!visitedItems.insert(item).second) // item handled already
            return alreadyInSet;
        bool addItem = false;
        for (const Item::Module &m : item->modules()) {
            if (collect(m.item, collect))
                addItem = true;
        }
        if (addItem && !alreadyInSet)
            itemsToInvalidate.insert(item);
        return addItem || alreadyInSet;
    };
    collectDependentItems(m_product.item, collectDependentItems);
    for (const Item * const item : itemsToInvalidate)
        m_loaderState.evaluator().clearCache(item);
}

int ModulePropertyMerger::compareValuePriorities(const ValueConstPtr &v1, const ValueConstPtr &v2)
{
    QBS_CHECK(v1);
    QBS_CHECK(v2);
    QBS_CHECK(v1->scope() != v2->scope());
    QBS_CHECK(v1->type() == Value::JSSourceValueType || v2->type() == Value::JSSourceValueType);

    const int prio1 = v1->priority(m_product.item);
    const int prio2 = v2->priority(m_product.item);
    if (prio1 != prio2)
        return prio1 - prio2;
    const int prioDiff = v1->scopeName().compare(v2->scopeName()); // Sic! See 8ff1dd0044
    QBS_CHECK(prioDiff != 0);
    return prioDiff;
}

ValuePtr ModulePropertyMerger::mergeListValues(const ValuePtr &currentHead, const ValuePtr &newElem)
{
    QBS_CHECK(newElem);
    QBS_CHECK(!newElem->next());

    if (!currentHead)
        return !newElem->expired(m_product.item) ? newElem : newElem->next();

    QBS_CHECK(!currentHead->expired(m_product.item));

    if (newElem->expired(m_product.item))
        return currentHead;

    if (compareValuePriorities(currentHead, newElem) < 0) {
        newElem->setNext(currentHead);
        return newElem;
    }
    currentHead->setNext(mergeListValues(currentHead->next(), newElem));
    return currentHead;
}

void ModulePropertyMerger::mergePropertyFromLocalInstance(
    Item *loadingItem, const QString &loadingName, Item *globalInstance,
    const QString &name, const ValuePtr &value)
{
    if (loadingItem->type() == ItemType::Project) {
        throw ErrorInfo(Tr::tr("Module properties cannot be set in Project items."),
                        value->location());
    }

    const PropertyDeclaration decl = globalInstance->propertyDeclaration(name);
    if (!decl.isValid()) {
        if (value->type() == Value::ItemValueType || value->createdByPropertiesBlock())
            return;
        throw ErrorInfo(Tr::tr("Property '%1' is not declared.")
                            .arg(name), value->location());
    }
    if (const ErrorInfo error = decl.checkForDeprecation(
                m_loaderState.parameters().deprecationWarningMode(), value->location(),
                m_loaderState.logger()); error.hasError()) {
        handlePropertyError(error, m_loaderState.parameters(), m_loaderState.logger());
        return;
    }
    if (value->setInternally()) { // E.g. qbs.architecture after multiplexing.
        globalInstance->setProperty(decl.name(), value);
        return;
    }
    QBS_CHECK(value->type() != Value::ItemValueType);
    const ValuePtr globalVal = globalInstance->ownProperty(decl.name());
    value->setScope(loadingItem, loadingName);
    QBS_CHECK(globalVal);

    // Values set internally cannot be overridden by JS values.
    // The same goes for values set on the command line.
    // Note that in both cases, there is no merging for list properties: The override is absolute.
    if (globalVal->setInternally() || globalVal->setByCommandLine())
        return;

    QBS_CHECK(value->type() == Value::JSSourceValueType);

    if (decl.isScalar()) {
        QBS_CHECK(!globalVal->expired(m_product.item));
        QBS_CHECK(!value->expired(m_product.item));
        if (compareValuePriorities(globalVal, value) < 0) {
            value->setCandidates(globalVal->candidates());
            globalVal->setCandidates({});
            value->addCandidate(globalVal);
            globalInstance->setProperty(decl.name(), value);
        } else {
            globalVal->addCandidate(value);
        }
    } else {
        if (const ValuePtr &newChainStart = mergeListValues(globalVal, value);
            newChainStart != globalVal) {
            globalInstance->setProperty(decl.name(), newChainStart);
        }
    }
}

bool ModulePropertyMerger::doFinalMerge(Item *moduleItem)
{
    if (!moduleItem->isPresentModule())
        return false;
    bool mustInvalidateCache = false;
    for (auto it = moduleItem->properties().begin(); it != moduleItem->properties().end(); ++it) {
        if (doFinalMerge(moduleItem->propertyDeclaration(it.key()), it.value()))
            mustInvalidateCache = true;
    }
    return mustInvalidateCache;
}

bool ModulePropertyMerger::doFinalMerge(const PropertyDeclaration &propertyDecl,
                                        ValuePtr &propertyValue)
{
    if (propertyValue->type() == Value::VariantValueType) {
        QBS_CHECK(!propertyValue->next());
        return false;
    }
    if (!propertyDecl.isValid())
        return false; // Caught later by dedicated checker.
    propertyValue->resetPriority();
    if (propertyDecl.isScalar()) {
        if (propertyValue->candidates().empty())
            return false;
        std::pair<int, std::vector<ValuePtr>> candidatesWithHighestPrio;
        candidatesWithHighestPrio.first = propertyValue->priority(m_product.item);
        candidatesWithHighestPrio.second.push_back(propertyValue);
        for (const ValuePtr &v : propertyValue->candidates()) {
            const int prio = v->priority(m_product.item);
            if (prio < candidatesWithHighestPrio.first)
                continue;
            if (prio > candidatesWithHighestPrio.first) {
                candidatesWithHighestPrio.first = prio;
                candidatesWithHighestPrio.second = {v};
                continue;
            }
            candidatesWithHighestPrio.second.push_back(v);
        }
        ValuePtr chosenValue = candidatesWithHighestPrio.second.front();
        if (int(candidatesWithHighestPrio.second.size()) > 1) {
            ErrorInfo error(Tr::tr("Conflicting scalar values for property '%1'.")
                                .arg(propertyDecl.name()));
            error.append({}, chosenValue->location());
            QBS_CHECK(chosenValue->type() == Value::JSSourceValueType);
            QStringView sourcCode = static_cast<JSSourceValue *>(
                                        chosenValue.get())->sourceCode();
            for (int i = 1; i < int(candidatesWithHighestPrio.second.size()); ++i) {
                const ValuePtr &v = candidatesWithHighestPrio.second.at(i);
                QBS_CHECK(v->type() == Value::JSSourceValueType);

                // Note that this is a bit silly: The source code could still evaluate to
                // different values in the end.
                if (static_cast<JSSourceValue *>(v.get())->sourceCode() != sourcCode)
                    error.append({}, v->location());
            }
            if (error.items().size() > 2)
                m_loaderState.logger().printWarning(error);
        }

        if (propertyValue == chosenValue)
            return false;
        std::vector<ValuePtr> candidates = propertyValue->candidates();
        candidates.erase(std::find(candidates.begin(), candidates.end(), chosenValue));
        chosenValue->setCandidates(candidates);
        chosenValue->addCandidate(propertyValue);
        propertyValue->setCandidates({});
        propertyValue = chosenValue;
        return true;
    }
    if (!propertyValue->next())
        return false;
    std::vector<ValuePtr> singleValuesBefore;
    for (ValuePtr current = propertyValue; current;) {
        singleValuesBefore.push_back(current);
        const ValuePtr next = current->next();
        if (next)
            current->setNext({});
        current = next;
    }
    ValuePtr newValue;
    for (const ValuePtr &v : singleValuesBefore)
        newValue = mergeListValues(newValue, v);
    std::vector<ValuePtr> singleValuesAfter;
    for (ValuePtr current = propertyValue; current; current = current->next())
        singleValuesAfter.push_back(current);
    propertyValue = newValue;
    return singleValuesBefore != singleValuesAfter;
}

} // namespace qbs::Internal
