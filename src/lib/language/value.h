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

#ifndef QBS_VALUE_H
#define QBS_VALUE_H

#include "filecontext.h"
#include "item.h"
#include <tools/codelocation.h>
#include <QVariant>

namespace qbs {
namespace Internal {

class ValueHandler;

class Value
{
public:
    enum Type
    {
        JSSourceValueType,
        ItemValueType,
        VariantValueType,
        BuiltinValueType
    };

    Value(Type t);
    virtual ~Value();

    Type type() const { return m_type; }
    virtual void apply(ValueHandler *) = 0;
    virtual CodeLocation location() const { return CodeLocation(); }

private:
    Type m_type;
};

class ValueHandler
{
public:
    virtual void handle(JSSourceValue *value) = 0;
    virtual void handle(ItemValue *value) = 0;
    virtual void handle(VariantValue *value) = 0;
    virtual void handle(BuiltinValue *value) = 0;
};

class JSSourceValue : public Value
{
    friend class ItemReaderASTVisitor;
    JSSourceValue();
public:
    static JSSourceValuePtr create();
    ~JSSourceValue();

    void apply(ValueHandler *handler) { handler->handle(this); }

    void setSourceCode(const QString &sourceCode) { m_sourceCode = sourceCode; }
    const QString &sourceCode() const { return m_sourceCode; }

    void setLocation(const CodeLocation &location) { m_location = location; }
    CodeLocation location() const { return m_location; }

    void setFile(const FileContextPtr &file) { m_file = file; }
    const FileContextPtr &file() const { return m_file; }

    bool sourceUsesBase() const { return m_sourceUsesBase; }
    bool sourceUsesOuter() const { return m_sourceUsesOuter; }
    bool hasFunctionForm() const { return m_hasFunctionForm; }

    const JSSourceValuePtr &baseValue() const { return m_baseValue; }
    void setBaseValue(const JSSourceValuePtr &v) { m_baseValue = v; }

    struct Alternative
    {
        QString condition;
        const Item *conditionScopeItem;
        JSSourceValuePtr value;
    };

    const QList<Alternative> &alternatives() const { return m_alternatives; }
    void setAlternatives(const QList<Alternative> &alternatives) { m_alternatives = alternatives; }
    void addAlternative(const Alternative &alternative) { m_alternatives.append(alternative); }

private:
    QString m_sourceCode;
    CodeLocation m_location;
    FileContextPtr m_file;
    bool m_sourceUsesBase;
    bool m_sourceUsesOuter;
    bool m_hasFunctionForm;
    JSSourceValuePtr m_baseValue;
    QList<Alternative> m_alternatives;
};

class Item;

class ItemValue : public Value
{
    ItemValue(Item *item);
public:
    static ItemValuePtr create(Item *item = 0);
    ~ItemValue();

    void apply(ValueHandler *handler) { handler->handle(this); }
//    virtual ItemValue *toItemValue() { return this; }
    Item *item() const;

    void setItem(Item *ptr);

private:
    Item *m_item;
};

inline Item *ItemValue::item() const
{
    return m_item;
}

inline void ItemValue::setItem(Item *ptr)
{
    m_item = ptr;
}


class VariantValue : public Value
{
    VariantValue(const QVariant &v);
public:
    static VariantValuePtr create(const QVariant &v = QVariant());

    void apply(ValueHandler *handler) { handler->handle(this); }

    void setValue(const QVariant &v) { m_value = v; }
    const QVariant &value() const { return m_value; }

private:
    QVariant m_value;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_VALUE_H
