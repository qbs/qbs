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
#include "astpropertiesitemhandler.h"

#include "item.h"
#include "value.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>

namespace qbs {
namespace Internal {

ASTPropertiesItemHandler::ASTPropertiesItemHandler(Item *parentItem) : m_parentItem(parentItem)
{
}

void ASTPropertiesItemHandler::handlePropertiesItems()
{
    // TODO: Simply forbid Properties items to have child items and get rid of this check.
    if (m_parentItem->type() != ItemType::Properties)
        setupAlternatives();
}

void ASTPropertiesItemHandler::setupAlternatives()
{
    auto it = m_parentItem->m_children.begin();
    while (it != m_parentItem->m_children.end()) {
        Item * const child = *it;
        bool remove = false;
        if (child->type() == ItemType::Properties) {
            handlePropertiesBlock(child);
            remove = m_parentItem->type() != ItemType::Export;
        }
        if (remove)
            it = m_parentItem->m_children.erase(it);
        else
            ++it;
    }
}

class PropertiesBlockConverter
{
public:
    PropertiesBlockConverter(const JSSourceValue::AltProperty &condition,
                             const JSSourceValue::AltProperty &overrideListProperties,
                             Item *propertiesBlockContainer, const Item *propertiesBlock)
        : m_propertiesBlockContainer(propertiesBlockContainer)
        , m_propertiesBlock(propertiesBlock)
    {
        m_alternative.condition = condition;
        m_alternative.overrideListProperties = overrideListProperties;
    }

    void apply()
    {
        doApply(m_propertiesBlockContainer, m_propertiesBlock);
    }

private:
    JSSourceValue::Alternative m_alternative;
    Item * const m_propertiesBlockContainer;
    const Item * const m_propertiesBlock;

    void doApply(Item *outer, const Item *inner)
    {
        for (auto it = inner->properties().constBegin();
                it != inner->properties().constEnd(); ++it) {
            if (inner == m_propertiesBlock
                    && (it.key() == StringConstants::conditionProperty()
                        || it.key() == StringConstants::overrideListPropertiesProperty())) {
                continue;
            }
            if (it.value()->type() == Value::ItemValueType) {
                Item * const innerVal = std::static_pointer_cast<ItemValue>(it.value())->item();
                ItemValuePtr outerVal = outer->itemProperty(it.key());
                if (!outerVal) {
                    outerVal = ItemValue::create(Item::create(outer->pool(), innerVal->type()),
                                                 true);
                    outer->setProperty(it.key(), outerVal);
                }
                doApply(outerVal->item(), innerVal);
            } else if (it.value()->type() == Value::JSSourceValueType) {
                const ValuePtr outerVal = outer->property(it.key());
                if (Q_UNLIKELY(outerVal && outerVal->type() != Value::JSSourceValueType)) {
                    throw ErrorInfo(Tr::tr("Incompatible value type in unconditional value at %1.")
                                    .arg(outerVal->location().toString()));
                }
                doApply(it.key(), outer, std::static_pointer_cast<JSSourceValue>(outerVal),
                        std::static_pointer_cast<JSSourceValue>(it.value()));
            } else {
                QBS_CHECK(!"Unexpected value type in conditional value.");
            }
        }
    }

    void doApply(const QString &propertyName, Item *item, JSSourceValuePtr value,
               const JSSourceValuePtr &conditionalValue)
    {
        if (!value) {
            value = JSSourceValue::create(true);
            value->setFile(conditionalValue->file());
            item->setProperty(propertyName, value);
            value->setSourceCode(QStringRef(&StringConstants::baseVar()));
            value->setSourceUsesBaseFlag();
        }
        m_alternative.value = conditionalValue;
        value->addAlternative(m_alternative);
    }
};

static JSSourceValue::AltProperty getPropertyData(const Item *propertiesItem, const QString &name)
{
    const ValuePtr value = propertiesItem->property(name);
    if (!value) {
        if (name == StringConstants::conditionProperty()) {
            throw ErrorInfo(Tr::tr("Properties.condition must be provided."),
                            propertiesItem->location());
        }
        return JSSourceValue::AltProperty(StringConstants::falseValue(),
                                          propertiesItem->location());
    }
    if (Q_UNLIKELY(value->type() != Value::JSSourceValueType)) {
        throw ErrorInfo(Tr::tr("Properties.%1 must be a value binding.").arg(name),
                    propertiesItem->location());
    }
    if (name == StringConstants::overrideListPropertiesProperty()) {
        const Item *parent = propertiesItem->parent();
        while (parent) {
            if (parent->type() == ItemType::Product)
                break;
            parent = parent->parent();
        }
        if (!parent) {
            throw ErrorInfo(Tr::tr("Properties.overrideListProperties can only be set "
                                   "in a Product item."));
        }

    }
    const JSSourceValuePtr srcval = std::static_pointer_cast<JSSourceValue>(value);
    return JSSourceValue::AltProperty(srcval->sourceCodeForEvaluation(), srcval->location());
}

void ASTPropertiesItemHandler::handlePropertiesBlock(const Item *propertiesItem)
{
    const auto condition = getPropertyData(propertiesItem, StringConstants::conditionProperty());
    const auto overrideListProperties = getPropertyData(propertiesItem,
            StringConstants::overrideListPropertiesProperty());
    PropertiesBlockConverter(condition, overrideListProperties, m_parentItem,
                             propertiesItem).apply();
}

} // namespace Internal
} // namespace qbs
