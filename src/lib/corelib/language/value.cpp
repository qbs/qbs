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

#include "value.h"

#include "filecontext.h"
#include "item.h"

#include <tools/qbsassert.h>
#include <tools/qttools.h>

namespace qbs {
namespace Internal {

Value::Value(Type t, bool createdByPropertiesBlock)
    : m_type(t), m_definingItem(nullptr), m_createdByPropertiesBlock(createdByPropertiesBlock)
{
}

Value::Value(const Value &other)
    : m_type(other.m_type),
      m_definingItem(other.m_definingItem),
      m_next(other.m_next ? other.m_next->clone() : ValuePtr()),
      m_createdByPropertiesBlock(other.m_createdByPropertiesBlock)
{
}

Value::~Value() = default;

Item *Value::definingItem() const
{
    return m_definingItem;
}

void Value::setDefiningItem(Item *item)
{
    m_definingItem = item;
}

ValuePtr Value::next() const
{
    return m_next;
}

void Value::setNext(const ValuePtr &next)
{
    QBS_ASSERT(next.get() != this, return);
    QBS_CHECK(type() != VariantValueType);
    m_next = next;
}


JSSourceValue::JSSourceValue(bool createdByPropertiesBlock)
    : Value(JSSourceValueType, createdByPropertiesBlock)
    , m_line(-1)
    , m_column(-1)
{
}

JSSourceValue::JSSourceValue(const JSSourceValue &other) : Value(other)
{
    m_sourceCode = other.m_sourceCode;
    m_line = other.m_line;
    m_column = other.m_column;
    m_file = other.m_file;
    m_flags = other.m_flags;
    m_baseValue = other.m_baseValue
            ? std::static_pointer_cast<JSSourceValue>(other.m_baseValue->clone())
            : JSSourceValuePtr();
    m_alternatives.reserve(other.m_alternatives.size());
    for (const Alternative &otherAlt : other.m_alternatives)
        m_alternatives.push_back(otherAlt.clone());
}

JSSourceValuePtr JSSourceValue::create(bool createdByPropertiesBlock)
{
    return JSSourceValuePtr(new JSSourceValue(createdByPropertiesBlock));
}

JSSourceValue::~JSSourceValue() = default;

ValuePtr JSSourceValue::clone() const
{
    return JSSourceValuePtr(new JSSourceValue(*this));
}

QString JSSourceValue::sourceCodeForEvaluation() const
{
    if (!hasFunctionForm())
        return m_sourceCode.toString();

    // rewrite blocks to be able to use return statements in property assignments
    static const QString prefix = QStringLiteral("(function()");
    static const QString suffix = QStringLiteral(")()");
    return prefix + m_sourceCode.toString() + suffix;
}

void JSSourceValue::setLocation(int line, int column)
{
    m_line = line;
    m_column = column;
}

CodeLocation JSSourceValue::location() const
{
    return CodeLocation(m_file->filePath(), m_line, m_column);
}

void JSSourceValue::setHasFunctionForm(bool b)
{
    if (b)
        m_flags |= HasFunctionForm;
    else
        m_flags &= ~HasFunctionForm;
}

void JSSourceValue::clearAlternatives()
{
    m_alternatives.clear();
}

void JSSourceValue::setDefiningItem(Item *item)
{
    Value::setDefiningItem(item);
    for (const JSSourceValue::Alternative &a : m_alternatives)
        a.value->setDefiningItem(item);
}

ItemValue::ItemValue(Item *item, bool createdByPropertiesBlock)
    : Value(ItemValueType, createdByPropertiesBlock)
    , m_item(item)
{
    QBS_CHECK(m_item);
}

ItemValuePtr ItemValue::create(Item *item, bool createdByPropertiesBlock)
{
    return ItemValuePtr(new ItemValue(item, createdByPropertiesBlock));
}

ValuePtr ItemValue::clone() const
{
    return create(m_item->clone(), createdByPropertiesBlock());
}

VariantValue::VariantValue(QVariant v)
    : Value(VariantValueType, false)
    , m_value(std::move(v))
{
}

VariantValuePtr VariantValue::create(const QVariant &v)
{
    if (!v.isValid())
        return invalidValue();
    if (static_cast<QMetaType::Type>(v.type()) == QMetaType::Bool)
        return v.toBool() ? VariantValue::trueValue() : VariantValue::falseValue();
    return VariantValuePtr(new VariantValue(v));
}

ValuePtr VariantValue::clone() const
{
    return std::make_shared<VariantValue>(*this);
}

const VariantValuePtr &VariantValue::falseValue()
{
    static const VariantValuePtr v = VariantValuePtr(new VariantValue(false));
    return v;
}

const VariantValuePtr &VariantValue::trueValue()
{
    static const VariantValuePtr v = VariantValuePtr(new VariantValue(true));
    return v;
}

const VariantValuePtr &VariantValue::invalidValue()
{
    static const VariantValuePtr v = VariantValuePtr(new VariantValue(QVariant()));
    return v;
}

} // namespace Internal
} // namespace qbs
