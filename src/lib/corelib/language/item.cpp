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
#include "deprecationinfo.h"
#include "filecontext.h"
#include "itemobserver.h"
#include "itempool.h"
#include "value.h"

#include <api/languageinfo.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <algorithm>

namespace qbs {
namespace Internal {

Item::Item(ItemPool *pool, ItemType type)
    : m_pool(pool)
    , m_observer(nullptr)
    , m_prototype(nullptr)
    , m_scope(nullptr)
    , m_outerItem(nullptr)
    , m_parent(nullptr)
    , m_type(type)
{
}

Item *Item::create(ItemPool *pool, ItemType type)
{
    return pool->allocateItem(type);
}

Item *Item::clone() const
{
    Item *dup = create(pool(), type());
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
    for (const Item * const child : qAsConst(m_children)) {
        Item *clonedChild = child->clone();
        clonedChild->m_parent = dup;
        dup->m_children.push_back(clonedChild);
    }

    for (PropertyMap::const_iterator it = m_properties.constBegin(); it != m_properties.constEnd();
         ++it) {
        dup->m_properties.insert(it.key(), it.value()->clone());
    }

    return dup;
}

QString Item::typeName() const
{
    switch (type()) {
    case ItemType::IdScope: return QStringLiteral("[IdScope]");
    case ItemType::ModuleInstance: return QStringLiteral("[ModuleInstance]");
    case ItemType::ModuleParameters: return QStringLiteral("[ModuleParametersInstance]");
    case ItemType::ModulePrefix: return QStringLiteral("[ModulePrefix]");
    case ItemType::Outer: return QStringLiteral("[Outer]");
    case ItemType::Scope: return QStringLiteral("[Scope]");
    default: return BuiltinDeclarations::instance().nameForType(type());
    }
}

bool Item::hasProperty(const QString &name) const
{
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
    return m_properties.contains(name);
}

ValuePtr Item::property(const QString &name) const
{
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
    return m_properties.value(name);
}

ItemValuePtr Item::itemProperty(const QString &name, const Item *itemTemplate)
{
    return itemProperty(name, itemTemplate, ItemValueConstPtr());
}

ItemValuePtr Item::itemProperty(const QString &name, const ItemValueConstPtr &value)
{
    return itemProperty(name, value->item(), value);
}

ItemValuePtr Item::itemProperty(const QString &name, const Item *itemTemplate,
                                const ItemValueConstPtr &itemValue)
{
    const ValuePtr v = property(name);
    if (v && v->type() == Value::ItemValueType)
        return std::static_pointer_cast<ItemValue>(v);
    if (!itemTemplate)
        return ItemValuePtr();
    const bool createdByPropertiesBlock = itemValue && itemValue->createdByPropertiesBlock();
    const ItemValuePtr result = ItemValue::create(Item::create(m_pool, itemTemplate->type()),
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
    const auto it = std::lower_bound(m_modules.begin(), m_modules.end(), module);
    QBS_CHECK(it == m_modules.end() || (module.name != it->name && module.item != it->item));
    m_modules.insert(it, module);
}

void Item::setObserver(ItemObserver *observer) const
{
    QBS_ASSERT(!observer || !m_observer, return);   // warn if accidentally overwritten
    m_observer = observer;
}

void Item::setProperty(const QString &name, const ValuePtr &value)
{
    m_properties.insert(name, value);
    if (m_observer)
        m_observer->onItemPropertyChanged(this);
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

void Item::setupForBuiltinType(Logger &logger)
{
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
                                       ? QStringRef(&StringConstants::undefinedValue())
                                       : QStringRef(&pd.initialValueSource()));
            m_properties.insert(pd.name(), sourceValue);
        } else if (pd.isDeprecated()) {
            const DeprecationInfo &di = pd.deprecationInfo();
            if (di.removalVersion() <= LanguageInfo::qbsVersion()) {
                QString message = Tr::tr("The property '%1' is no longer valid for %2 items. "
                        "It was removed in qbs %3.")
                        .arg(pd.name(), typeName(), di.removalVersion().toString());
                ErrorInfo error(message, value->location());
                if (!di.additionalUserInfo().isEmpty())
                    error.append(di.additionalUserInfo());
                throw error;
            }
            QString warning = Tr::tr("The property '%1' is deprecated and will be removed in "
                                     "qbs %2.").arg(pd.name(), di.removalVersion().toString());
            ErrorInfo error(warning, value->location());
            if (!di.additionalUserInfo().isEmpty())
                error.append(di.additionalUserInfo());
            logger.printWarning(error);
        }
    }
}

void Item::copyProperty(const QString &propertyName, Item *target) const
{
    target->setProperty(propertyName, property(propertyName));
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
    for (const Item * const child : qAsConst(m_children))
        child->dump(indentation + 4);
    if (prototype()) {
        qDebug("%sprototype:", indent.constData());
        prototype()->dump(indentation + 4);
    }
}

void Item::removeProperty(const QString &name)
{
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

} // namespace Internal
} // namespace qbs
