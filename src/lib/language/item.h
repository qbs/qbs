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
#include <parser/qmljsmemorypool_p.h>
#include <tools/codelocation.h>
#include <tools/error.h>
#include <tools/weakpointer.h>

#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QStringList>

namespace qbs {
namespace Internal {

class ItemPool;
class ProjectFile;

class Item : public QbsQmlJS::Managed
{
    friend class BuiltinDeclarations;
    friend class ItemPool;
    friend class ItemReaderASTVisitor;
    Q_DISABLE_COPY(Item)
    Item(ItemPool *pool);

public:
    ~Item();

    struct Module
    {
        Module()
            : item(0)
        {}

        QStringList name;
        Item *item;
    };
    typedef QList<Module> Modules;
    typedef QMap<QString, PropertyDeclaration> PropertyDeclarationMap;
    typedef QMap<QString, ValuePtr> PropertyMap;

    static Item *create(ItemPool *pool);
    Item *clone(ItemPool *pool) const;
    ItemPool *pool() const;

    const QString &id() const;
    const QString &typeName() const;
    const CodeLocation &location() const;
    Item *prototype() const;
    Item *scope() const;
    bool isModuleInstance() const;
    Item *outerItem() const;
    Item *parent() const;
    const FileContextPtr file() const;
    QList<Item *> children() const;
    Item *child(const QString &type, bool checkForMultiple = true) const;
    const PropertyMap &properties() const;
    const PropertyDeclarationMap &propertyDeclarations() const;
    const Modules &modules() const;
    Modules &modules();
    const ErrorInfo &error() const { return m_error; }

    bool hasProperty(const QString &name) const;
    bool hasOwnProperty(const QString &name) const;
    ValuePtr property(const QString &name) const;
    ItemValuePtr itemProperty(const QString &name, bool create = false);
    JSSourceValuePtr sourceProperty(const QString &name) const;
    void setPropertyObserver(ItemObserver *observer) const;
    void setProperty(const QString &name, const ValuePtr &value);
    void setPropertyDeclaration(const QString &name, const PropertyDeclaration &declaration);
    void setTypeName(const QString &name);
    void setLocation(const CodeLocation &location);
    void setPrototype(Item *prototype);
    void setFile(const FileContextPtr &file);
    void setScope(Item *item);
    void setModuleInstanceFlag(bool b);
    void setOuterItem(Item *item);
    void setChildren(const QList<Item *> &children);
    void setParent(Item *item);
    static void addChild(Item *parent, Item *child);
    void setError(const ErrorInfo &error) { m_error = error; }

private:
    ItemPool *m_pool;
    mutable ItemObserver *m_propertyObserver;
    QString m_id;
    QString m_typeName;
    CodeLocation m_location;
    bool m_moduleInstance;
    Item *m_prototype;
    Item *m_scope;
    Item *m_outerItem;
    Item *m_parent;
    QList<Item *> m_children;
    FileContextPtr m_file;
    PropertyMap m_properties;
    PropertyDeclarationMap m_propertyDeclarations;
    QList<FunctionDeclaration> m_functions;
    Modules m_modules;
    ErrorInfo m_error; // For SubProject items. May or may not be reported depending on their condition.
};

inline ItemPool *Item::pool() const
{
    return m_pool;
}

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

inline Item *Item::prototype() const
{
    return m_prototype;
}

inline Item *Item::scope() const
{
    return m_scope;
}

inline bool Item::isModuleInstance() const
{
    return m_moduleInstance;
}

inline Item *Item::outerItem() const
{
    return m_outerItem;
}

inline Item *Item::parent() const
{
    return m_parent;
}

inline const FileContextPtr Item::file() const
{
    return m_file;
}

inline QList<Item *> Item::children() const
{
    return m_children;
}

inline const Item::PropertyMap &Item::properties() const
{
    return m_properties;
}

inline const Item::PropertyDeclarationMap &Item::propertyDeclarations() const
{
    return m_propertyDeclarations;
}

inline void Item::setProperty(const QString &name, const ValuePtr &value)
{
    m_properties.insert(name, value);
    if (m_propertyObserver)
        m_propertyObserver->onItemPropertyChanged(this);
}

inline void Item::setPropertyDeclaration(const QString &name,
                                         const PropertyDeclaration &declaration)
{
    m_propertyDeclarations.insert(name, declaration);
}

inline void Item::setTypeName(const QString &name)
{
    m_typeName = name;
}

inline void Item::setLocation(const CodeLocation &location)
{
    m_location = location;
}

inline void Item::setPrototype(Item *prototype)
{
    m_prototype = prototype;
}

inline void Item::setFile(const FileContextPtr &file)
{
    m_file = file;
}

inline void Item::setScope(Item *item)
{
    m_scope = item;
}

inline void Item::setModuleInstanceFlag(bool b)
{
    m_moduleInstance = b;
}

inline void Item::setOuterItem(Item *item)
{
    m_outerItem = item;
}

inline void Item::setChildren(const QList<Item *> &children)
{
    m_children = children;
}

inline void Item::setParent(Item *item)
{
    m_parent = item;
}

inline void Item::addChild(Item *parent, Item *child)
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
