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

#include "item.h"

#include "builtindeclarations.h"
#include "filecontext.h"
#include "itemobserver.h"
#include "itempool.h"
#include "value.h"

#include <api/languageinfo.h>
#include <loader/loaderutils.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <algorithm>

namespace qbs {
namespace Internal {

Item *Item::create(ItemPool *pool, ItemType type)
{
    return pool->allocateItem(type);
}

Item *Item::clone(ItemPool &pool) const
{
    assertModuleLocked();

    Item *dup = create(&pool, type());
    dup->m_id = m_id;
    dup->m_location = m_location;
    dup->m_prototype = m_prototype;
    dup->m_scope = m_scope;
    dup->m_outerItem = m_outerItem;
    dup->m_parent = m_parent;
    dup->m_file = m_file;
    dup->m_propertyDeclarations = m_propertyDeclarations;
    dup->m_modules = m_modules;

    dup->m_children.reserve(m_children.size());
    for (const Item * const child : std::as_const(m_children)) {
        Item *clonedChild = child->clone(pool);
        clonedChild->m_parent = dup;
        dup->m_children.push_back(clonedChild);
    }

    for (PropertyMap::const_iterator it = m_properties.constBegin(); it != m_properties.constEnd();
         ++it) {
        dup->m_properties.insert(it.key(), it.value()->clone(pool));
    }

    return dup;
}

Item *Item::rootPrototype()
{
    Item *proto = this;
    while (proto->prototype())
        proto = proto->prototype();
    return proto;
}

QString Item::typeName() const
{
    switch (type()) {
    case ItemType::IdScope: return QStringLiteral("[IdScope]");
    case ItemType::ModuleInstance: return QStringLiteral("[ModuleInstance]");
    case ItemType::ModuleInstancePlaceholder: return QStringLiteral("[ModuleInstancePlaceholder]");
    case ItemType::ModuleParameters: return QStringLiteral("[ModuleParametersInstance]");
    case ItemType::ModulePrefix: return QStringLiteral("[ModulePrefix]");
    case ItemType::Outer: return QStringLiteral("[Outer]");
    case ItemType::Scope: return QStringLiteral("[Scope]");
    default: return BuiltinDeclarations::instance().nameForType(type());
    }
}

bool Item::hasProperty(const QString &name) const
{
    assertModuleLocked();
    const Item *item = this;
    do {
        if (item->m_properties.contains(name))
            return true;
        item = item->m_prototype;
    } while (item);
    return false;
}

bool Item::hasOwnProperty(const QString &name) const
{
    assertModuleLocked();
    return m_properties.contains(name);
}

ValuePtr Item::property(const QString &name) const
{
    assertModuleLocked();
    ValuePtr value;
    const Item *item = this;
    do {
        if ((value = item->m_properties.value(name)))
            break;
        item = item->m_prototype;
    } while (item);
    return value;
}

ValuePtr Item::ownProperty(const QString &name) const
{
    assertModuleLocked();
    return m_properties.value(name);
}

ItemValuePtr Item::itemProperty(const QString &name, ItemPool &pool, const Item *itemTemplate)
{
    return itemProperty(name, itemTemplate, ItemValueConstPtr(), pool);
}

ItemValuePtr Item::itemProperty(const QString &name, const ItemValueConstPtr &value, ItemPool &pool)
{
    return itemProperty(name, value->item(), value, pool);
}

ItemValuePtr Item::itemProperty(const QString &name, const Item *itemTemplate,
                                const ItemValueConstPtr &itemValue, ItemPool &pool)
{
    const ValuePtr v = property(name);
    if (v && v->type() == Value::ItemValueType)
        return std::static_pointer_cast<ItemValue>(v);
    if (!itemTemplate)
        return ItemValuePtr();
    const bool createdByPropertiesBlock = itemValue && itemValue->createdByPropertiesBlock();
    ItemValuePtr result = ItemValue::create(Item::create(&pool, itemTemplate->type()),
                                            createdByPropertiesBlock);
    setProperty(name, result);
    return result;
}

JSSourceValuePtr Item::sourceProperty(const QString &name) const
{
    ValuePtr v = property(name);
    if (!v || v->type() != Value::JSSourceValueType)
        return JSSourceValuePtr();
    return std::static_pointer_cast<JSSourceValue>(v);
}

VariantValuePtr Item::variantProperty(const QString &name) const
{
    ValuePtr v = property(name);
    if (!v || v->type() != Value::VariantValueType)
        return VariantValuePtr();
    return std::static_pointer_cast<VariantValue>(v);
}

bool Item::isOfTypeOrhasParentOfType(ItemType type) const
{
    const Item *item = this;
    do {
        if (item->type() == type)
            return true;
        item = item->parent();
    } while (item);
    return false;
}

void Item::addObserver(ItemObserver *observer) const
{
    // Cached Module properties never change.
    if (m_type == ItemType::Module)
        return;

    std::lock_guard lock(m_observersMutex);
    if (!qEnvironmentVariableIsEmpty("QBS_SANITY_CHECKS"))
        QBS_CHECK(!contains(m_observers, observer));
    m_observers << observer;
}

void Item::removeObserver(ItemObserver *observer) const
{
    if (m_type == ItemType::Module)
        return;
    std::lock_guard lock(m_observersMutex);
    const auto it = std::find(m_observers.begin(), m_observers.end(), observer);
    QBS_CHECK(it != m_observers.end());
    m_observers.erase(it);
}

PropertyDeclaration Item::propertyDeclaration(const QString &name, bool allowExpired) const
{
    auto it = m_propertyDeclarations.find(name);
    if (it != m_propertyDeclarations.end())
        return it.value();
    if (allowExpired) {
        it = m_expiredPropertyDeclarations.find(name);
        if (it != m_expiredPropertyDeclarations.end())
            return it.value();
    }
    return m_prototype ? m_prototype->propertyDeclaration(name) : PropertyDeclaration();
}

void Item::addModule(const Item::Module &module)
{
    if (!qEnvironmentVariableIsEmpty("QBS_SANITY_CHECKS")) {
        QBS_CHECK(none_of(m_modules, [&](const Module &m) {
            if (m.name != module.name)
                return false;
            if (!!module.product != !!m.product)
                return true;
            if (!module.product)
                return true;
            if (module.product->multiplexConfigurationId == m.product->multiplexConfigurationId
                    && module.product->profileName == m.product->profileName) {
                return true;
            }
            return false;
        }));
    }

    m_modules.push_back(module);
}

void Item::setProperty(const QString &name, const ValuePtr &value)
{
    assertModuleLocked();
    m_properties.insert(name, value);
    std::lock_guard lock(m_observersMutex);
    for (ItemObserver * const observer : m_observers)
        observer->onItemPropertyChanged(this);
}

void Item::dump() const
{
    dump(0);
}

bool Item::isPresentModule() const
{
    // Initial value is "true" as JS source, overwritten one is always QVariant(false).
    const ValueConstPtr v = property(StringConstants::presentProperty());
    return v && v->type() == Value::JSSourceValueType;
}

bool Item::isFallbackModule() const
{
    return hasProperty(QLatin1String("__fallback"));
}

void Item::setupForBuiltinType(DeprecationWarningMode deprecationMode, Logger &logger)
{
    assertModuleLocked();
    const BuiltinDeclarations &builtins = BuiltinDeclarations::instance();
    const auto properties = builtins.declarationsForType(type()).properties();
    for (const PropertyDeclaration &pd : properties) {
        m_propertyDeclarations.insert(pd.name(), pd);
        const ValuePtr value = m_properties.value(pd.name());
        if (!value) {
            if (pd.isDeprecated())
                continue;
            JSSourceValuePtr sourceValue = JSSourceValue::create();
            sourceValue->setIsBuiltinDefaultValue();
            sourceValue->setFile(file());
            sourceValue->setSourceCode(pd.initialValueSource().isEmpty()
                                       ? StringConstants::undefinedValue()
                                       : pd.initialValueSource());
            m_properties.insert(pd.name(), sourceValue);
        } else if (ErrorInfo error = pd.checkForDeprecation(deprecationMode, value->location(),
                                                            logger); error.hasError()) {
            throw error;
        }
    }
}

void Item::copyProperty(const QString &propertyName, Item *target) const
{
    target->setProperty(propertyName, property(propertyName));
}

void Item::overrideProperties(const QVariantMap &config, const QString &key,
                              const SetupProjectParameters &parameters, Logger &logger)
{
    const QVariant configMap = config.value(key);
    if (configMap.isNull())
        return;
    overrideProperties(configMap.toMap(), QualifiedId(key), parameters, logger);
}

static const char *valueType(const Value *v)
{
    switch (v->type()) {
    case Value::JSSourceValueType: return "JS source";
    case Value::ItemValueType: return "Item";
    case Value::VariantValueType: return "Variant";
    }
    return ""; // For dumb compilers.
}

void Item::dump(int indentation) const
{
    const QByteArray indent(indentation, ' ');
    qDebug("%stype: %s, pointer value: %p", indent.constData(), qPrintable(typeName()), this);
    if (!m_properties.empty())
        qDebug("%sproperties:", indent.constData());
    for (auto it = m_properties.constBegin(); it != m_properties.constEnd(); ++it) {
        const QByteArray nextIndent(indentation + 4, ' ');
        qDebug("%skey: %s, value type: %s", nextIndent.constData(), qPrintable(it.key()),
               valueType(it.value().get()));
        switch (it.value()->type()) {
        case Value::JSSourceValueType:
            qDebug("%svalue: %s", nextIndent.constData(),
                   qPrintable(std::static_pointer_cast<JSSourceValue>(it.value())->sourceCodeForEvaluation()));
            break;
        case Value::ItemValueType:
            qDebug("%svalue:", nextIndent.constData());
            std::static_pointer_cast<ItemValue>(it.value())->item()->dump(indentation + 8);
            break;
        case Value::VariantValueType:
            qDebug("%svalue: %s", nextIndent.constData(),
                   qPrintable(std::static_pointer_cast<VariantValue>(it.value())->value().toString()));
            break;
        }
    }
    if (!m_children.empty())
        qDebug("%schildren:", indent.constData());
    for (const Item * const child : std::as_const(m_children))
        child->dump(indentation + 4);
    if (prototype()) {
        qDebug("%sprototype:", indent.constData());
        prototype()->dump(indentation + 4);
    }
}

void Item::lockModule() const
{
    QBS_CHECK(m_type == ItemType::Module);
    m_moduleMutex.lock();
#ifndef NDEBUG
    QBS_CHECK(!m_moduleLocked);
    m_moduleLocked = true;
#endif
}

void Item::unlockModule() const
{
    QBS_CHECK(m_type == ItemType::Module);
#ifndef NDEBUG
    QBS_CHECK(m_moduleLocked);
    m_moduleLocked = false;
#endif
    m_moduleMutex.unlock();
}

// This safeguard verifies that all contexts which access Module properties have really
// acquired the lock via ModuleItemLocker, as they must.
void Item::assertModuleLocked() const
{
#ifndef NDEBUG
    if (m_type == ItemType::Module)
        QBS_CHECK(m_moduleLocked);
#endif
}

void Item::removeProperty(const QString &name)
{
    assertModuleLocked();
    m_properties.remove(name);
}

Item *Item::child(ItemType type, bool checkForMultiple) const
{
    Item *child = nullptr;
    for (Item * const currentChild : children()) {
        if (currentChild->type() == type) {
            if (!checkForMultiple)
                return currentChild;
            if (child) {
                ErrorInfo error(Tr::tr("Multiple instances of item '%1' found where at most one "
                                       "is allowed.")
                                .arg(BuiltinDeclarations::instance().nameForType(type)));
                error.append(Tr::tr("First item"), child->location());
                error.append(Tr::tr("Second item"), currentChild->location());
                throw error;
            }
            child = currentChild;
        }
    }
    return child;
}

void Item::addChild(Item *parent, Item *child)
{
    parent->m_children.push_back(child);
    child->setParent(parent);
}

void Item::removeChild(Item *parent, Item *child)
{
    parent->m_children.removeOne(child);
    child->setParent(nullptr);
}

void Item::setPropertyDeclaration(const QString &name, const PropertyDeclaration &declaration)
{
    if (declaration.isExpired()) {
        m_propertyDeclarations.remove(name);
        m_expiredPropertyDeclarations.insert(name, declaration);
    } else {
        m_propertyDeclarations.insert(name, declaration);
    }
}

void Item::setPropertyDeclarations(const Item::PropertyDeclarationMap &decls)
{
    m_propertyDeclarations = decls;
}

void Item::overrideProperties(
        const QVariantMap &overridden,
        const QualifiedId &namePrefix,
        const SetupProjectParameters &parameters,
         Logger &logger)
{
    if (overridden.isEmpty())
        return;
    for (QVariantMap::const_iterator it = overridden.constBegin(); it != overridden.constEnd();
            ++it) {
        const PropertyDeclaration decl = propertyDeclaration(it.key());
        if (!decl.isValid()) {
            ErrorInfo error(Tr::tr("Unknown property: %1.%2").
                    arg(namePrefix.toString(), it.key()));
            handlePropertyError(error, parameters, logger);
            continue;
        }
        const auto overridenValue = VariantValue::create(PropertyDeclaration::convertToPropertyType(
            it.value(), decl.type(), namePrefix, it.key()));
        overridenValue->markAsSetByCommandLine();
        setProperty(it.key(), overridenValue);
    }
}

void Item::setModules(const Modules &modules)
{
    m_modules = modules;
}

Item *createNonPresentModule(ItemPool &pool, const QString &name, const QString &reason, Item *module)
{
    qCDebug(lcModuleLoader) << "Non-required module '" << name << "' not loaded (" << reason << ")."
                            << "Creating dummy module for presence check.";
    if (!module) {
        module = Item::create(&pool, ItemType::ModuleInstance);
        module->setFile(FileContext::create());
        module->setProperty(StringConstants::nameProperty(), VariantValue::create(name));
    }
    module->setType(ItemType::ModuleInstance);
    module->setProperty(StringConstants::presentProperty(), VariantValue::falseValue());
    return module;
}

void setScopeForDescendants(Item *item, Item *scope)
{
    for (Item * const child : item->children()) {
        child->setScope(scope);
        setScopeForDescendants(child, scope);
    }
}

} // namespace Internal
} // namespace qbs
