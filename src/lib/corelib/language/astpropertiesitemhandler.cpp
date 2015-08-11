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
#include "astpropertiesitemhandler.h"

#include "item.h"
#include "value.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ASTPropertiesItemHandler::ASTPropertiesItemHandler(Item *parentItem) : m_parentItem(parentItem)
{
}

void ASTPropertiesItemHandler::handlePropertiesItems()
{
    if (m_parentItem->typeName() != QLatin1String("Properties")
            && m_parentItem->typeName() != QLatin1String("SubProject")) {
        setupAlternatives();
    }
}

void ASTPropertiesItemHandler::setupAlternatives()
{
    auto it = m_parentItem->m_children.begin();
    while (it != m_parentItem->m_children.end()) {
        Item * const child = *it;
        if (child->typeName() == QLatin1String("Properties")) {
            handlePropertiesBlock(child);
            it = m_parentItem->m_children.erase(it);
        } else {
            ++it;
        }
    }
}

class PropertiesBlockConverter
{
public:
    PropertiesBlockConverter(const QString &condition, Item *propertiesBlockContainer,
                             const Item *propertiesBlock)
        : m_propertiesBlockContainer(propertiesBlockContainer)
        , m_propertiesBlock(propertiesBlock)
    {
        m_alternative.condition = condition;
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
            if (inner == m_propertiesBlock && it.key() == QLatin1String("condition"))
                continue;
            if (it.value()->type() == Value::ItemValueType) {
                doApply(outer->itemProperty(it.key(), true)->item(),
                      it.value().staticCast<ItemValue>()->item());
            } else if (it.value()->type() == Value::JSSourceValueType) {
                const ValuePtr outerVal = outer->property(it.key());
                if (Q_UNLIKELY(outerVal && outerVal->type() != Value::JSSourceValueType)) {
                    throw ErrorInfo(Tr::tr("Incompatible value type in unconditional value at %1.")
                                    .arg(outerVal->location().toString()));
                }
                doApply(it.key(), outer, outerVal.staticCast<JSSourceValue>(),
                        it.value().staticCast<JSSourceValue>());
            } else {
                QBS_CHECK(!"Unexpected value type in conditional value.");
            }
        }
    }

    void doApply(const QString &propertyName, Item *item, JSSourceValuePtr value,
               const JSSourceValuePtr &conditionalValue)
    {
        QBS_ASSERT(!value || value->file() == conditionalValue->file(), return);
        if (!value) {
            value = JSSourceValue::create();
            value->setFile(conditionalValue->file());
            item->setProperty(propertyName, value);
            static const QString baseKeyword = QLatin1String("base");
            value->setSourceCode(QStringRef(&baseKeyword));
            value->setSourceUsesBaseFlag();
        }
        m_alternative.value = conditionalValue;
        value->addAlternative(m_alternative);
    }
};

void ASTPropertiesItemHandler::handlePropertiesBlock(const Item *propertiesItem)
{
    const ValuePtr value = propertiesItem->property(QLatin1String("condition"));
    if (Q_UNLIKELY(!value))
        throw ErrorInfo(Tr::tr("Properties.condition must be provided."),
                    propertiesItem->location());
    if (Q_UNLIKELY(value->type() != Value::JSSourceValueType))
        throw ErrorInfo(Tr::tr("Properties.condition must be a value binding."),
                    propertiesItem->location());
    const JSSourceValuePtr srcval = value.staticCast<JSSourceValue>();
    const QString condition = srcval->sourceCodeForEvaluation();
    PropertiesBlockConverter(condition, m_parentItem, propertiesItem).apply();
}

} // namespace Internal
} // namespace qbs
