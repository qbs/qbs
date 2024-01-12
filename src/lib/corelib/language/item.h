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

#ifndef QBS_ITEM_H
#define QBS_ITEM_H

#include "forward_decls.h"
#include "itemtype.h"
#include "propertydeclaration.h"
#include "qualifiedid.h"
#include <parser/qmljsmemorypool_p.h>
#include <tools/codelocation.h>
#include <tools/deprecationwarningmode.h>
#include <tools/error.h>
#include <tools/version.h>

#include <QtCore/qlist.h>
#include <QtCore/qmap.h>

#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace qbs {

class SetupProjectParameters;

namespace Internal {
class ItemObserver;
class ItemPool;
class Logger;
class ModuleItemLocker;
class ProductContext;

class QBS_AUTOTEST_EXPORT Item : public QbsQmlJS::Managed
{
    friend class ASTPropertiesItemHandler;
    friend class ItemPool;
    friend class ItemReaderASTVisitor;
    friend class ModuleItemLocker;
    Q_DISABLE_COPY(Item)
    Item(ItemType type) : m_type(type) {}

public:
    struct Module
    {
        QualifiedId name;
        Item *item = nullptr;
        ProductContext *product = nullptr; // Set if and only if the dep is a product.

        // All items that declared an explicit dependency on this module. Can contain any
        // number of module instances and at most one product.
        using ParametersWithPriority = std::pair<QVariantMap, int>;
        using LoadingItemInfo = std::pair<Item *, ParametersWithPriority>;
        std::vector<LoadingItemInfo> loadingItems;

        QVariantMap parameters;
        VersionRange versionRange;

        // The shorter this value, the "closer to the product" we consider the module,
        // and the higher its precedence is when merging property values.
        int maxDependsChainLength = 0;

        bool required = true;
    };
    using Modules = std::vector<Module>;
    using PropertyDeclarationMap = QMap<QString, PropertyDeclaration>;
    using PropertyMap = QMap<QString, ValuePtr>;

    static Item *create(ItemPool *pool, ItemType type);
    Item *clone(ItemPool &pool) const;

    const QString &id() const { return m_id; }
    const CodeLocation &location() const { return m_location; }
    Item *prototype() const { return m_prototype; }
    Item *rootPrototype();
    Item *scope() const { return m_scope; }
    Item *outerItem() const { return m_outerItem; }
    Item *parent() const { return m_parent; }
    const FileContextPtr &file() const { return m_file; }
    const QList<Item *> &children() const { return m_children; }
    QList<Item *> &children() { return m_children; }
    Item *child(ItemType type, bool checkForMultiple = true) const;
    const PropertyMap &properties() const { assertModuleLocked(); return m_properties; }
    PropertyMap &properties() { assertModuleLocked(); return m_properties; }
    const PropertyDeclarationMap &propertyDeclarations() const { return m_propertyDeclarations; }
    PropertyDeclaration propertyDeclaration(const QString &name, bool allowExpired = true) const;

    // The list of modules is ordered such that dependencies appear before the modules
    // depending on them.
    const Modules &modules() const { return m_modules; }
    Modules &modules() { return m_modules; }

    void addModule(const Module &module);
    void removeModules() { m_modules.clear(); }
    void setModules(const Modules &modules);

    ItemType type() const { return m_type; }
    void setType(ItemType type) { m_type = type; }
    QString typeName() const;

    bool hasProperty(const QString &name) const;
    bool hasOwnProperty(const QString &name) const;
    ValuePtr property(const QString &name) const;
    ValuePtr ownProperty(const QString &name) const;
    ItemValuePtr itemProperty(const QString &name, ItemPool &pool, const Item *itemTemplate = nullptr);
    ItemValuePtr itemProperty(const QString &name, const ItemValueConstPtr &value, ItemPool &pool);
    JSSourceValuePtr sourceProperty(const QString &name) const;
    VariantValuePtr variantProperty(const QString &name) const;
    bool isOfTypeOrhasParentOfType(ItemType type) const;
    void addObserver(ItemObserver *observer) const;
    void removeObserver(ItemObserver *observer) const;
    void setProperty(const QString &name, const ValuePtr &value);
    void setProperties(const PropertyMap &props) { assertModuleLocked(); m_properties = props; }
    void removeProperty(const QString &name);
    void setPropertyDeclaration(const QString &name, const PropertyDeclaration &declaration);
    void setPropertyDeclarations(const PropertyDeclarationMap &decls);
    void setLocation(const CodeLocation &location) { m_location = location; }
    void setPrototype(Item *prototype) { m_prototype = prototype; }
    void setFile(const FileContextPtr &file) { m_file = file; }
    void setId(const QString &id) { m_id = id; }
    void setScope(Item *item) { m_scope = item; }
    void setOuterItem(Item *item) { m_outerItem = item; }
    void setChildren(const QList<Item *> &children) { m_children = children; }
    void setParent(Item *item) { m_parent = item; }
    void childrenReserve(int size) { m_children.reserve(size); }
    static void addChild(Item *parent, Item *child);
    static void removeChild(Item *parent, Item *child);
    void dump() const;
    bool isPresentModule() const;
    bool isFallbackModule() const;
    void setupForBuiltinType(DeprecationWarningMode deprecationMode, Logger &logger);
    void copyProperty(const QString &propertyName, Item *target) const;
    void overrideProperties(
        const QVariantMap &config,
        const QString &key,
        const SetupProjectParameters &parameters,
        Logger &logger);
    void overrideProperties(
        const QVariantMap &config,
        const QualifiedId &namePrefix,
        const SetupProjectParameters &parameters,
        Logger &logger);

private:
    ItemValuePtr itemProperty(const QString &name, const Item *itemTemplate,
                              const ItemValueConstPtr &itemValue, ItemPool &pool);

    void dump(int indentation) const;

    void lockModule() const;
    void unlockModule() const;
    void assertModuleLocked() const;

    mutable std::vector<ItemObserver *> m_observers;
    mutable std::mutex m_observersMutex;
    QString m_id;
    CodeLocation m_location;
    Item *m_prototype = nullptr;
    Item *m_scope = nullptr;
    Item *m_outerItem = nullptr;
    Item *m_parent = nullptr;
    QList<Item *> m_children;
    FileContextPtr m_file;
    PropertyMap m_properties;
    PropertyDeclarationMap m_propertyDeclarations;
    PropertyDeclarationMap m_expiredPropertyDeclarations;
    Modules m_modules;
    ItemType m_type;
    mutable std::mutex m_moduleMutex;
#ifndef NDEBUG
    mutable std::atomic_bool m_moduleLocked = false;
#endif
};

inline bool operator<(const Item::Module &m1, const Item::Module &m2) { return m1.name < m2.name; }

Item *createNonPresentModule(ItemPool &pool, const QString &name, const QString &reason,
                             Item *module);
void setScopeForDescendants(Item *item, Item *scope);

// This mechanism is needed because Module items are shared between products (not doing so
// would be prohibitively expensive).
// The competing accesses are between
//  - Attaching a temporary qbs module for evaluating the Module condition.
//  - Cloning the module when creating an instance.
//  - Directly accessing Module properties, which happens rarely (as opposed to properties of
//    an instance).
class ModuleItemLocker
{
public:
    ModuleItemLocker(const Item &item) : m_item(item) { item.lockModule(); }
    ~ModuleItemLocker() { m_item.unlockModule(); }
private:
    const Item &m_item;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEM_H
