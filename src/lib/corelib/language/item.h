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

#ifndef QBS_ITEM_H
#define QBS_ITEM_H

#include "forward_decls.h"
#include "functiondeclaration.h"
#include "propertydeclaration.h"
#include "qualifiedid.h"
#include <parser/qmljsmemorypool_p.h>
#include <tools/codelocation.h>
#include <tools/error.h>

#include <QList>
#include <QMap>

namespace qbs {
namespace Internal {
class ItemObserver;
class ItemPool;
class Logger;

class Item : public QbsQmlJS::Managed
{
    friend class ItemPool;
    friend class ItemReaderASTVisitor;
    Q_DISABLE_COPY(Item)
    Item(ItemPool *pool);

public:
    ~Item();

    struct Module
    {
        Module()
            : item(0), isProduct(false), required(true)
        {}

        QualifiedId name;
        Item *item;
        bool isProduct;
        bool required;
    };
    typedef QList<Module> Modules;
    typedef QMap<QString, PropertyDeclaration> PropertyDeclarationMap;
    typedef QMap<QString, ValuePtr> PropertyMap;

    static Item *create(ItemPool *pool);
    Item *clone() const;
    ItemPool *pool() const { return m_pool; }

    const QString &id() const { return m_id; }
    const QString &typeName() const { return m_typeName; }
    const CodeLocation &location() const { return m_location; }
    Item *prototype() const { return m_prototype; }
    Item *scope() const { return m_scope; }
    bool isModuleInstance() const { return m_moduleInstance; }
    Item *outerItem() const { return m_outerItem; }
    Item *parent() const { return m_parent; }
    const FileContextPtr file() const { return m_file; }
    QList<Item *> children() const { return m_children; }
    Item *child(const QString &type, bool checkForMultiple = true) const;
    const PropertyMap &properties() const { return m_properties; }
    const PropertyDeclarationMap &propertyDeclarations() const { return m_propertyDeclarations; }
    PropertyDeclaration propertyDeclaration(const QString &name) const;
    const Modules &modules() const { return m_modules; }
    void addModule(const Module &module);
    void removeModules() { m_modules.clear(); }
    void setModules(const Modules &modules) { m_modules = modules; }

    bool hasProperty(const QString &name) const;
    bool hasOwnProperty(const QString &name) const;
    ValuePtr property(const QString &name) const;
    ItemValuePtr itemProperty(const QString &name, bool create = false);
    JSSourceValuePtr sourceProperty(const QString &name) const;
    VariantValuePtr variantProperty(const QString &name) const;
    void setPropertyObserver(ItemObserver *observer) const;
    void setProperty(const QString &name, const ValuePtr &value);
    void setProperties(const PropertyMap &props) { m_properties = props; }
    void removeProperty(const QString &name);
    void setPropertyDeclaration(const QString &name, const PropertyDeclaration &declaration);
    void setTypeName(const QString &name) { m_typeName = name; }
    void setLocation(const CodeLocation &location) { m_location = location; }
    void setPrototype(Item *prototype) { m_prototype = prototype; }
    void setFile(const FileContextPtr &file) { m_file = file; }
    void setScope(Item *item) { m_scope = item; }
    void setModuleInstanceFlag(bool b) { m_moduleInstance = b; }
    void setOuterItem(Item *item) { m_outerItem = item; }
    void setChildren(const QList<Item *> &children) { m_children = children; }
    void setParent(Item *item) { m_parent = item; }
    static void addChild(Item *parent, Item *child);
    void dump() const;
    bool isPresentModule() const;
    void setupForBuiltinType(Logger &logger);
    void copyProperty(const QString &propertyName, Item *target) const;

    void setDelayedError(const ErrorInfo &e) { m_delayedError = e; }
    ErrorInfo delayedError() const { return m_delayedError; }

private:
    void dump(int indentation) const;

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
    ErrorInfo m_delayedError;
};

inline bool operator<(const Item::Module &m1, const Item::Module &m2) { return m1.name < m2.name; }

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEM_H
