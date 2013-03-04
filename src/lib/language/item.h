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

#ifndef QBS_ITEM_H
#define QBS_ITEM_H

#include "forward_decls.h"
#include "itemobserver.h"
#include "value.h"
#include "functiondeclaration.h"
#include "propertydeclaration.h"
#include <tools/codelocation.h>
#include <tools/weakpointer.h>

#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QStringList>

namespace qbs {
namespace Internal {

class ProjectFile;

class Item
{
    friend class ItemReaderASTVisitor;
    Q_DISABLE_COPY(Item)
    Item();

public:
    ~Item();

    struct Module
    {
        QStringList name;
        ItemPtr item;
    };
    typedef QList<Module> Modules;

    static ItemPtr create();
    ItemPtr clone() const;

    const QString &id() const;
    const QString &typeName() const;
    const CodeLocation &location() const;
    const ItemPtr &prototype() const;
    ItemPtr scope() const;
    WeakPointer<Item> outerItem() const;
    WeakPointer<Item> parent() const;
    const FileContextPtr file() const;
    QList<ItemPtr> children() const;
    const QMap<QString, ValuePtr> &properties() const;
    const QMap<QString, PropertyDeclaration> &propertyDeclarations() const;
    const Modules &modules() const;
    Modules &modules();

    bool hasProperty(const QString &name) const;
    bool hasOwnProperty(const QString &name) const;
    ValuePtr property(const QString &name) const;
    ItemValuePtr itemProperty(const QString &name, bool create = false);
    JSSourceValuePtr sourceProperty(const QString &name) const;
    void setPropertyObserver(ItemObserver *observer) const;
    void setProperty(const QString &name, const ValuePtr &value);
    void setTypeName(const QString &name);
    void setLocation(const CodeLocation &location);
    void setPrototype(const ItemPtr &prototype);
    void setFile(const FileContextPtr &file);
    void setScope(const ItemPtr &item);
    void setOuterItem(const ItemPtr &item);
    void setChildren(const QList<ItemPtr> &children);
    void setParent(const ItemPtr &item);
    static void addChild(const ItemPtr &parent, const ItemPtr &child);

private:
    mutable ItemObserver *m_propertyObserver;
    QString m_id;
    QString m_typeName;
    CodeLocation m_location;
    ItemPtr m_prototype;
    ItemPtr m_scope;
    WeakPointer<Item> m_outerItem;
    WeakPointer<Item> m_parent;
    QList<ItemPtr> m_children;
    FileContextPtr m_file;
    QMap<QString, ValuePtr> m_properties;
    QMap<QString, PropertyDeclaration> m_propertyDeclarations;
    QList<FunctionDeclaration> m_functions;
    Modules m_modules;
};

inline const QString &Item::id() const
{
    return m_id;
}

inline const QString &Item::typeName() const
{
    return m_typeName;
}

inline const CodeLocation &Item::location() const
{
    return m_location;
}

inline const ItemPtr &Item::prototype() const
{
    return m_prototype;
}

inline ItemPtr Item::scope() const
{
    return m_scope;
}

inline WeakPointer<Item> Item::outerItem() const
{
    return m_outerItem;
}

inline WeakPointer<Item> Item::parent() const
{
    return m_parent;
}

inline const FileContextPtr Item::file() const
{
    return m_file;
}

inline QList<ItemPtr> Item::children() const
{
    return m_children;
}

inline const QMap<QString, ValuePtr> &Item::properties() const
{
    return m_properties;
}

inline const QMap<QString, PropertyDeclaration> &Item::propertyDeclarations() const
{
    return m_propertyDeclarations;
}

inline void Item::setProperty(const QString &name, const ValuePtr &value)
{
    m_properties.insert(name, value);
    if (m_propertyObserver)
        m_propertyObserver->onItemPropertyChanged(this);
}

inline void Item::setTypeName(const QString &name)
{
    m_typeName = name;
}

inline void Item::setLocation(const CodeLocation &location)
{
    m_location = location;
}

inline void Item::setPrototype(const ItemPtr &prototype)
{
    m_prototype = prototype;
}

inline void Item::setFile(const FileContextPtr &file)
{
    m_file = file;
}

inline void Item::setScope(const ItemPtr &item)
{
    m_scope = item;
}

inline void Item::setOuterItem(const ItemPtr &item)
{
    m_outerItem = item;
}

inline void Item::setChildren(const QList<ItemPtr> &children)
{
    m_children = children;
}

inline void Item::setParent(const ItemPtr &item)
{
    m_parent = item;
}

inline void Item::addChild(const ItemPtr &parent, const ItemPtr &child)
{
    parent->m_children.append(child);
    child->setParent(parent);
}

inline const Item::Modules &Item::modules() const
{
    return m_modules;
}

inline Item::Modules &Item::modules()
{
    return m_modules;
}

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEM_H
