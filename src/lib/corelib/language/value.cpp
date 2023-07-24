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

Value::Value(Type t, bool createdByPropertiesBlock) : m_type(t)
{
    if (createdByPropertiesBlock)
        m_flags |= OriginPropertiesBlock;
}

Value::Value(const Value &other, ItemPool &pool)
    : m_type(other.m_type),
      m_scope(other.m_scope),
      m_scopeName(other.m_scopeName),
      m_next(other.m_next ? other.m_next->clone(pool) : ValuePtr()),
      m_candidates(other.m_candidates),
      m_flags(other.m_flags)
{
}

Value::~Value() = default;

void Value::setScope(Item *scope, const QString &scopeName)
{
    m_scope = scope;
    m_scopeName = scopeName;
}

int Value::priority(const Item *productItem) const
{
    if (m_priority == -1)
        m_priority = calculatePriority(productItem);
    return m_priority;
}

int Value::calculatePriority(const Item *productItem) const
{
    if (setInternally())
        return INT_MAX;
    if (setByCommandLine())
        return INT_MAX - 1;
    if (setByProfile())
        return 2;
    if (!scope())
        return 1;
    if (scope()->type() == ItemType::Product)
        return INT_MAX - 2;
    if (!scope()->isPresentModule())
        return 0;
    const auto it = std::find_if(
        productItem->modules().begin(), productItem->modules().end(),
        [this](const Item::Module &m) { return m.item == scope(); });
    QBS_CHECK(it != productItem->modules().end());
    return INT_MAX - 3 - it->maxDependsChainLength;
}

void Value::resetPriority()
{
    m_priority = -1;
    if (m_next)
        m_next->resetPriority();
    for (const ValuePtr &v : m_candidates)
        v->resetPriority();
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

bool Value::setInternally() const
{
    return type() == VariantValueType && !setByProfile() && !setByCommandLine();
}


JSSourceValue::JSSourceValue(bool createdByPropertiesBlock)
    : Value(JSSourceValueType, createdByPropertiesBlock)
    , m_line(-1)
    , m_column(-1)
{
}

JSSourceValue::JSSourceValue(const JSSourceValue &other, ItemPool &pool) : Value(other, pool)
{
    m_sourceCode = other.m_sourceCode;
    m_line = other.m_line;
    m_column = other.m_column;
    m_file = other.m_file;
    m_baseValue = other.m_baseValue
            ? std::static_pointer_cast<JSSourceValue>(other.m_baseValue->clone(pool))
            : JSSourceValuePtr();
    m_alternatives = transformed<std::vector<Alternative>>(
        other.m_alternatives, [&pool](const auto &alternative) {
        return alternative.clone(pool); });
}

JSSourceValuePtr JSSourceValue::create(bool createdByPropertiesBlock)
{
    return std::make_shared<JSSourceValue>(createdByPropertiesBlock);
}

JSSourceValue::~JSSourceValue() = default;

ValuePtr JSSourceValue::clone(ItemPool &pool) const
{
    return std::make_shared<JSSourceValue>(*this, pool);
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

void JSSourceValue::clearAlternatives()
{
    m_alternatives.clear();
}

void JSSourceValue::setScope(Item *scope, const QString &scopeName)
{
    Value::setScope(scope, scopeName);
    if (m_baseValue)
        m_baseValue->setScope(scope, scopeName);
    for (const JSSourceValue::Alternative &a : m_alternatives)
        a.value->setScope(scope, scopeName);
}

void JSSourceValue::resetPriority()
{
    Value::resetPriority();
    if (m_baseValue)
        m_baseValue->resetPriority();
    for (const JSSourceValue::Alternative &a : m_alternatives)
        a.value->resetPriority();
}

void JSSourceValue::addCandidate(const ValuePtr &v)
{
    Value::addCandidate(v);
    if (m_baseValue)
        m_baseValue->addCandidate(v);
    for (const JSSourceValue::Alternative &a : m_alternatives)
        a.value->addCandidate(v);
}

void JSSourceValue::setCandidates(const std::vector<ValuePtr> &candidates)
{
    Value::setCandidates(candidates);
    if (m_baseValue)
        m_baseValue->setCandidates(candidates);
    for (const JSSourceValue::Alternative &a : m_alternatives)
        a.value->setCandidates(candidates);
}

ItemValue::ItemValue(Item *item, bool createdByPropertiesBlock)
    : Value(ItemValueType, createdByPropertiesBlock)
    , m_item(item)
{
    QBS_CHECK(m_item);
}

ItemValuePtr ItemValue::create(Item *item, bool createdByPropertiesBlock)
{
    return std::make_shared<ItemValue>(item, createdByPropertiesBlock);
}

ValuePtr ItemValue::clone(ItemPool &pool) const
{
    return create(m_item->clone(pool), createdByPropertiesBlock());
}

class StoredVariantValue : public VariantValue
{
public:
    explicit StoredVariantValue(QVariant v) : VariantValue(std::move(v)) {}

    quintptr id() const override { return quintptr(this); }
};

VariantValue::VariantValue(QVariant v)
    : Value(VariantValueType, false)
    , m_value(std::move(v))
{
}

VariantValue::VariantValue(const VariantValue &other, ItemPool &pool)
    : Value(other, pool), m_value(other.m_value) {}

template<typename T>
VariantValuePtr createImpl(const QVariant &v)
{
    if (!v.isValid())
        return VariantValue::invalidValue();
    if (static_cast<QMetaType::Type>(v.userType()) == QMetaType::Bool)
        return v.toBool() ? VariantValue::trueValue() : VariantValue::falseValue();
    return std::make_shared<T>(v);
}

VariantValuePtr VariantValue::create(const QVariant &v)
{
    return createImpl<VariantValue>(v);
}

VariantValuePtr VariantValue::createStored(const QVariant &v)
{
    return createImpl<StoredVariantValue>(v);
}

ValuePtr VariantValue::clone(ItemPool &pool) const
{
    return std::make_shared<VariantValue>(*this, pool);
}

const VariantValuePtr &VariantValue::falseValue()
{
    static const VariantValuePtr v = std::make_shared<VariantValue>(false);
    return v;
}

const VariantValuePtr &VariantValue::trueValue()
{
    static const VariantValuePtr v = std::make_shared<VariantValue>(true);
    return v;
}

const VariantValuePtr &VariantValue::invalidValue()
{
    static const VariantValuePtr v = std::make_shared<VariantValue>(QVariant());
    return v;
}

} // namespace Internal
} // namespace qbs
