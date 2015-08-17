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

#include "value.h"

#include "filecontext.h"
#include "item.h"

#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

Value::Value(Type t, bool createdByPropertiesBlock)
    : m_type(t), m_definingItem(0), m_createdByPropertiesBlock(createdByPropertiesBlock)
{
}

Value::~Value()
{
}

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
    QBS_ASSERT(next.data() != this, return);
    m_next = next;
}


JSSourceValue::JSSourceValue(bool createdByPropertiesBlock)
    : Value(JSSourceValueType, createdByPropertiesBlock)
    , m_line(-1)
    , m_column(-1)
    , m_exportScope(nullptr)
{
}

JSSourceValuePtr JSSourceValue::create(bool createdByPropertiesBlock)
{
    return JSSourceValuePtr(new JSSourceValue(createdByPropertiesBlock));
}

JSSourceValue::~JSSourceValue()
{
}

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

void JSSourceValue::setDefiningItem(Item *item)
{
    Value::setDefiningItem(item);
    foreach (const JSSourceValue::Alternative &a, m_alternatives)
        a.value->setDefiningItem(item);
}

Item *JSSourceValue::exportScope() const
{
    return m_exportScope;
}

void JSSourceValue::setExportScope(Item *extraScope)
{
    m_exportScope = extraScope;
}


ItemValue::ItemValue(Item *item, bool createdByPropertiesBlock)
    : Value(ItemValueType, createdByPropertiesBlock)
    , m_item(item)
{
}

ItemValuePtr ItemValue::create(Item *item, bool createdByPropertiesBlock)
{
    return ItemValuePtr(new ItemValue(item, createdByPropertiesBlock));
}

ItemValue::~ItemValue()
{
}

ValuePtr ItemValue::clone() const
{
    Item *clonedItem = m_item ? m_item->clone() : 0;
    return ItemValuePtr(new ItemValue(clonedItem, createdByPropertiesBlock()));
}

VariantValue::VariantValue(const QVariant &v)
    : Value(VariantValueType, false)
    , m_value(v)
{
}

VariantValuePtr VariantValue::create(const QVariant &v)
{
    return VariantValuePtr(new VariantValue(v));
}

ValuePtr VariantValue::clone() const
{
    return VariantValuePtr(new VariantValue(*this));
}

} // namespace Internal
} // namespace qbs
