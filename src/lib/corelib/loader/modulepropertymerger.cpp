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

namespace qbs::Internal {
class ModulePropertyMerger::Private
{
public:
    Private(LoaderState &loaderState) : loaderState(loaderState) {}

    int compareValuePriorities(const Item *productItem, const ValueConstPtr &v1,
                               const ValueConstPtr &v2);
    ValuePtr mergeListValues(const Item *productItem, const ValuePtr &currentHead, const ValuePtr &newElem);
    void mergePropertyFromLocalInstance(const Item *productItem, Item *loadingItem,
                                const QString &loadingName, Item *globalInstance,
                                const QString &name, const ValuePtr &value);
    bool doFinalMerge(const Item *productItem, Item *moduleItem);
    bool doFinalMerge(const Item *productItem, const PropertyDeclaration &propertyDecl,
                      ValuePtr &propertyValue);

    LoaderState &loaderState;
    qint64 elapsedTime = 0;
};

void ModulePropertyMerger::mergeFromLocalInstance(
    const Item *productItem, Item *loadingItem, const QString &loadingName,
    const Item *localInstance, Item *globalInstance)
{
    AccumulatingTimer t(d->loaderState.parameters().logElapsedTime() ? &d->elapsedTime : nullptr);

    for (auto it = localInstance->properties().constBegin();
         it != localInstance->properties().constEnd(); ++it) {
        d->mergePropertyFromLocalInstance(productItem, loadingItem, loadingName,
                                  globalInstance, it.key(), it.value());
    }
}

void ModulePropertyMerger::doFinalMerge(const Item *productItem)
{
    AccumulatingTimer t(d->loaderState.parameters().logElapsedTime() ? &d->elapsedTime : nullptr);

    Set<const Item *> itemsToInvalidate;
    for (const Item::Module &module : productItem->modules()) {
        if (d->doFinalMerge(productItem, module.item))
            itemsToInvalidate << module.item;
    }
    const auto collectDependentItems = [&itemsToInvalidate](const Item *item,
                                                            const auto &collect) -> bool {
        const bool alreadyInSet = itemsToInvalidate.contains(item);
        bool addItem = false;
        for (const Item::Module &m : item->modules()) {
            if (collect(m.item, collect))
                addItem = true;
        }
        if (addItem && !alreadyInSet)
            itemsToInvalidate << item;
        return addItem || alreadyInSet;
    };
    collectDependentItems(productItem, collectDependentItems);
    for (const Item * const item : itemsToInvalidate)
        d->loaderState.evaluator().clearCache(item);
}

void ModulePropertyMerger::printProfilingInfo(int indent)
{
    if (!d->loaderState.parameters().logElapsedTime())
        return;
    d->loaderState.logger().qbsLog(LoggerInfo, true)
            << QByteArray(indent, ' ')
            << Tr::tr("Merging module property values took %1.")
               .arg(elapsedTimeString(d->elapsedTime));
}

ModulePropertyMerger::ModulePropertyMerger(LoaderState &loaderState)
    : d(makePimpl<Private>(loaderState)) { }
ModulePropertyMerger::~ModulePropertyMerger() = default;

int ModulePropertyMerger::Private::compareValuePriorities(
    const Item *productItem, const ValueConstPtr &v1, const ValueConstPtr &v2)
{
    QBS_CHECK(v1);
    QBS_CHECK(v2);
    QBS_CHECK(v1->scope() != v2->scope());
    QBS_CHECK(v1->type() == Value::JSSourceValueType || v2->type() == Value::JSSourceValueType);

    const int prio1 = v1->priority(productItem);
    const int prio2 = v2->priority(productItem);
    if (prio1 != prio2)
        return prio1 - prio2;
    const int prioDiff = v1->scopeName().compare(v2->scopeName()); // Sic! See 8ff1dd0044
    QBS_CHECK(prioDiff != 0);
    return prioDiff;
}

ValuePtr ModulePropertyMerger::Private::mergeListValues(
    const Item *productItem, const ValuePtr &currentHead, const ValuePtr &newElem)
{
    QBS_CHECK(newElem);
    QBS_CHECK(!newElem->next());

    if (!currentHead)
        return !newElem->expired(productItem) ? newElem : newElem->next();

    QBS_CHECK(!currentHead->expired(productItem));

    if (newElem->expired(productItem))
        return currentHead;

    if (compareValuePriorities(productItem, currentHead, newElem) < 0) {
        newElem->setNext(currentHead);
        return newElem;
    }
    currentHead->setNext(mergeListValues(productItem, currentHead->next(), newElem));
    return currentHead;
}

void ModulePropertyMerger::Private::mergePropertyFromLocalInstance(
    const Item *productItem, Item *loadingItem, const QString &loadingName, Item *globalInstance,
    const QString &name, const ValuePtr &value)
{
    const PropertyDeclaration decl = globalInstance->propertyDeclaration(name);
    if (!decl.isValid()) {
        if (value->type() == Value::ItemValueType || value->createdByPropertiesBlock())
            return;
        throw ErrorInfo(Tr::tr("Property '%1' is not declared.")
                            .arg(name), value->location());
    }
    if (const ErrorInfo error = decl.checkForDeprecation(
                loaderState.parameters().deprecationWarningMode(), value->location(),
                loaderState.logger()); error.hasError()) {
        handlePropertyError(error, loaderState.parameters(), loaderState.logger());
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
        QBS_CHECK(!globalVal->expired(productItem));
        QBS_CHECK(!value->expired(productItem));
        if (compareValuePriorities(productItem, globalVal, value) < 0) {
            value->setCandidates(globalVal->candidates());
            globalVal->setCandidates({});
            value->addCandidate(globalVal);
            globalInstance->setProperty(decl.name(), value);
        } else {
            globalVal->addCandidate(value);
        }
    } else {
        if (const ValuePtr &newChainStart = mergeListValues(productItem, globalVal, value);
            newChainStart != globalVal) {
            globalInstance->setProperty(decl.name(), newChainStart);
        }
    }
}

bool ModulePropertyMerger::Private::doFinalMerge(const Item *productItem, Item *moduleItem)
{
    if (!moduleItem->isPresentModule())
        return false;
    bool mustInvalidateCache = false;
    for (auto it = moduleItem->properties().begin(); it != moduleItem->properties().end(); ++it) {
        if (doFinalMerge(productItem, moduleItem->propertyDeclaration(it.key()), it.value()))
            mustInvalidateCache = true;
    }
    return mustInvalidateCache;
}

bool ModulePropertyMerger::Private::doFinalMerge(
    const Item *productItem, const PropertyDeclaration &propertyDecl, ValuePtr &propertyValue)
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
        candidatesWithHighestPrio.first = propertyValue->priority(productItem);
        candidatesWithHighestPrio.second.push_back(propertyValue);
        for (const ValuePtr &v : propertyValue->candidates()) {
            const int prio = v->priority(productItem);
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
                loaderState.logger().printWarning(error);
        }

        if (propertyValue == chosenValue)
            return false;
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
        newValue = mergeListValues(productItem, newValue, v);
    std::vector<ValuePtr> singleValuesAfter;
    for (ValuePtr current = propertyValue; current; current = current->next())
        singleValuesAfter.push_back(current);
    propertyValue = newValue;
    return singleValuesBefore != singleValuesAfter;
}

} // namespace qbs::Internal
