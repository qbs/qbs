/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "value.h"
#include "item.h"

namespace qbs {
namespace Internal {

Value::Value(Type t)
    : m_type(t)
{
}

Value::~Value()
{
}


JSSourceValue::JSSourceValue()
    : Value(JSSourceValueType)
    , m_sourceUsesBase(false)
    , m_sourceUsesOuter(false)
    , m_hasFunctionForm(false)
{
}

JSSourceValuePtr JSSourceValue::create()
{
    return JSSourceValuePtr(new JSSourceValue);
}

JSSourceValue::~JSSourceValue()
{
}


ItemValue::ItemValue(Item *item)
    : Value(ItemValueType)
    , m_item(item)
{
}

ItemValuePtr ItemValue::create(Item *item)
{
    return ItemValuePtr(new ItemValue(item));
}

ItemValue::~ItemValue()
{
}

VariantValue::VariantValue(const QVariant &v)
    : Value(VariantValueType)
    , m_value(v)
{
}

VariantValuePtr VariantValue::create(const QVariant &v)
{
    return VariantValuePtr(new VariantValue(v));
}

} // namespace Internal
} // namespace qbs
