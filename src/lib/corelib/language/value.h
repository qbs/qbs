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

#ifndef QBS_VALUE_H
#define QBS_VALUE_H

#include "forward_decls.h"
#include <tools/codelocation.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <functional>
#include <vector>

namespace qbs {
namespace Internal {
class Item;
class ItemPool;
class ItemValue;
class JSSourceValue;
class VariantValue;

template<typename T>
class ValueHandler
{
public:
    T handle(Value *v);

private:
    virtual T doHandle(JSSourceValue *value) = 0;
    virtual T doHandle(ItemValue *value) = 0;
    virtual T doHandle(VariantValue *value) = 0;
};

class Value
{
public:
    enum Type {
        JSSourceValueType,
        ItemValueType,
        VariantValueType
    };

    enum Flag {
        NoFlags = 0x00,
        SourceUsesBase = 0x01,
        SourceUsesOuter = 0x02,
        SourceUsesOriginal = 0x04,
        HasFunctionForm = 0x08,
        ExclusiveListValue = 0x10,
        BuiltinDefaultValue = 0x20,
        OriginPropertiesBlock = 0x40,
        OriginProfile = 0x80,
        OriginCommandLine = 0x100,
        Fallback = 0x200,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    Value(Type t, bool createdByPropertiesBlock);
    Value(const Value &other) = default;
    virtual ~Value();

    Type type() const { return m_type; }
    template<typename T>
    T apply(ValueHandler<T> *handler)
    {
        return handler->handle(this);
    };
    virtual ValuePtr clone(ItemPool &) const = 0;
    virtual CodeLocation location() const { return {}; }

    Item *scope() const { return m_scope; }
    virtual void setScope(Item *scope, const QString &scopeName);
    QString scopeName() const { return m_scopeName; }
    int priority(const Item *productItem) const;
    virtual void resetPriority();

    void addCandidate(const ValuePtr &v) { m_candidates.push_back(v); }

    const std::vector<ValuePtr> &candidates() const { return m_candidates; }
    void setCandidates(const std::vector<ValuePtr> &candidates) { m_candidates = candidates; }
    void removeExpiredCandidates(const Item *productItem);
    void sortCandidates(const std::function<bool(const ValuePtr &v1, const ValuePtr &v2)> &less);

    bool createdByPropertiesBlock() const { return m_flags & OriginPropertiesBlock; }
    void setFallback() { m_flags |= Fallback; }
    bool isFallback() const { return m_flags & Fallback; }
    void markAsSetByProfile() { m_flags |= OriginProfile; }
    bool setByProfile() const { return m_flags & OriginProfile; }
    void markAsSetByCommandLine() { m_flags |= OriginCommandLine; }
    bool setByCommandLine() const { return m_flags & OriginCommandLine; }
    bool setInternally() const;
    bool expired(const Item *productItem) const { return priority(productItem) == 0; }

    void setSourceUsesBase() { m_flags |= SourceUsesBase; }
    bool sourceUsesBase() const { return m_flags.testFlag(SourceUsesBase); }
    void setSourceUsesOuter() { m_flags |= SourceUsesOuter; }
    bool sourceUsesOuter() const { return m_flags.testFlag(SourceUsesOuter); }
    void setSourceUsesOriginal() { m_flags |= SourceUsesOriginal; }
    bool sourceUsesOriginal() const { return m_flags.testFlag(SourceUsesOriginal); }
    void setHasFunctionForm() { m_flags |= HasFunctionForm; }
    bool hasFunctionForm() const { return m_flags.testFlag(HasFunctionForm); }
    void setIsExclusiveListValue() { m_flags |= ExclusiveListValue; }
    bool isExclusiveListValue() { return m_flags.testFlag(ExclusiveListValue); }
    void setIsBuiltinDefaultValue() { m_flags |= BuiltinDefaultValue; }
    bool isBuiltinDefaultValue() const { return m_flags.testFlag(BuiltinDefaultValue); }

private:
    int calculatePriority(const Item *productItem) const;

    const Type m_type;
    Item *m_scope = nullptr;
    QString m_scopeName;
    std::vector<ValuePtr> m_candidates;
    Flags m_flags;
    mutable int m_priority = -1;
};

class JSSourceValue : public Value
{
    friend class ItemReaderASTVisitor;

public:
    explicit JSSourceValue(bool createdByPropertiesBlock);
    JSSourceValue(const JSSourceValue &other, ItemPool &pool);

    static JSSourceValuePtr QBS_AUTOTEST_EXPORT create(bool createdByPropertiesBlock = false);
    ~JSSourceValue() override;

    ValuePtr clone(ItemPool &pool) const override;

    void setSourceCode(QStringView sourceCode) { m_sourceCode = sourceCode; }
    QStringView sourceCode() const { return m_sourceCode; }
    QString sourceCodeForEvaluation() const;

    void setLocation(int line, int column);
    int line() const { return m_line; }
    int column() const { return m_column; }
    CodeLocation location() const override;

    void setFile(const FileContextPtr &file) { m_file = file; }
    const FileContextPtr &file() const { return m_file; }

    const JSSourceValuePtr &baseValue() const { return m_baseValue; }
    void setBaseValue(const JSSourceValuePtr &v) { m_baseValue = v; }

    struct Alternative
    {
        struct PropertyData
        {
            PropertyData() = default;
            PropertyData(QString v, const CodeLocation &l) : value(std::move(v)), location(l) {}
            QString value;
            CodeLocation location;
        };

        Alternative() = default;
        Alternative(PropertyData c, PropertyData o, JSSourceValuePtr v)
            : condition(std::move(c)), overrideListProperties(std::move(o)), value(std::move(v)) {}
        Alternative clone(ItemPool &pool) const
        {
            return Alternative(condition, overrideListProperties,
                               std::static_pointer_cast<JSSourceValue>(value->clone(pool)));
        }

        PropertyData condition;
        PropertyData overrideListProperties;
        JSSourceValuePtr value;
    };
    using AltProperty = Alternative::PropertyData;

    const std::vector<Alternative> &alternatives() const { return m_alternatives; }
    void addAlternative(const Alternative &alternative);
    void clearAlternatives();

    void setScope(Item *scope, const QString &scopeName) override;
    void resetPriority() override;

private:
    QStringView m_sourceCode;
    int m_line;
    int m_column;
    FileContextPtr m_file;
    JSSourceValuePtr m_baseValue;
    std::vector<Alternative> m_alternatives;
};


class ItemValue : public Value
{
public:
    ItemValue(Item *item, bool createdByPropertiesBlock);
    static ItemValuePtr create(Item *item, bool createdByPropertiesBlock = false);

    Item *item() const { return m_item; }
    void setItem(Item *item) { m_item = item; }

private:
    ValuePtr clone(ItemPool &pool) const override;

    Item *m_item;
};


class VariantValue : public Value
{
public:
    explicit VariantValue(QVariant v);
    VariantValue(const VariantValue &v) = default;
    static VariantValuePtr create(const QVariant &v = QVariant());
    static VariantValuePtr createStored(const QVariant &v = QVariant());

    ValuePtr clone(ItemPool &) const override;

    const QVariant &value() const { return m_value; }
    virtual quintptr id() const { return 0; }

    static const VariantValuePtr &falseValue();
    static const VariantValuePtr &trueValue();
    static const VariantValuePtr &invalidValue();

private:
    QVariant m_value;
};

template<typename T>
inline T ValueHandler<T>::handle(Value *v)
{
    switch (v->type()) {
    case Value::JSSourceValueType:
        return doHandle(static_cast<JSSourceValue *>(v));
    case Value::ItemValueType:
        return doHandle(static_cast<ItemValue *>(v));
    case Value::VariantValueType:
        return doHandle(static_cast<VariantValue *>(v));
    }
    if constexpr (!std::is_same_v<T, void>)
        return {};
}

} // namespace Internal
} // namespace qbs

#endif // QBS_VALUE_H
