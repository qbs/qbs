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
#include "settingsmodel.h"
#include "settingsrepresentation.h"

#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>
#include <tools/stringconstants.h>

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#ifdef QT_GUI_LIB
#include <QtGui/qbrush.h>
#endif

#include <memory>

namespace qbs {
namespace Internal {

struct Node
{
    Node() : parent(nullptr), isFromSettings(true) {}
    ~Node() { qDeleteAll(children); }

    QString uniqueChildName() const;
    bool hasDirectChildWithName(const QString &name) const;

    QString name;
    QString value;
    Node *parent;
    QList<Node *> children;
    bool isFromSettings;
};

QString Node::uniqueChildName() const
{
    QString newName = QStringLiteral("newkey");
    bool unique;
    do {
        unique = true;
        for (const Node *childNode : qAsConst(children)) {
            if (childNode->name == newName) {
                unique = false;
                newName += QLatin1Char('_');
                break;
            }
        }
    } while (!unique);
    return newName;
}

bool Node::hasDirectChildWithName(const QString &name) const
{
    for (const Node * const child : qAsConst(children)) {
        if (child->name == name)
            return true;
    }
    return false;
}

} // namespace Internal

using Internal::Node;

class SettingsModel::SettingsModelPrivate
{
public:
    SettingsModelPrivate() : dirty(false), editable(true) {}

    void readSettings();
    void addNodeFromSettings(Node *parentNode, const QString &fullyQualifiedName);
    void addNode(Node *parentNode, const QString &currentNamePart,
                 const QStringList &restOfName, const QVariant &value);
    void doSave(const Node *node, const QString &prefix);
    Node *indexToNode(const QModelIndex &index);

    Settings::Scope scope() const { return settings->scopeForWriting(); }

    Node rootNode;
    std::unique_ptr<qbs::Settings> settings;
    QVariantMap additionalProperties;
    bool dirty;
    bool editable;
};

SettingsModel::SettingsModel(const QString &settingsDir, Settings::Scope scope, QObject *parent)
    : QAbstractItemModel(parent), d(new SettingsModelPrivate)
{
    d->settings = std::make_unique<qbs::Settings>(settingsDir);
    d->settings->setScopeForWriting(scope);
    d->readSettings();
}

SettingsModel::~SettingsModel()
{
    delete d;
}

void SettingsModel::reload()
{
    beginResetModel();
    d->readSettings();
    endResetModel();
}

void SettingsModel::save()
{
    if (!d->dirty)
        return;
    d->settings->clear();
    d->doSave(&d->rootNode, QString());
    d->dirty = false;
}

void SettingsModel::updateSettingsDir(const QString &settingsDir)
{
    const Settings::Scope scope = d->scope();
    beginResetModel();
    d->settings = std::make_unique<qbs::Settings>(settingsDir);
    d->settings->setScopeForWriting(scope);
    d->readSettings();
    endResetModel();
}

void SettingsModel::addNewKey(const QModelIndex &parent)
{
    Node *parentNode = d->indexToNode(parent);
    if (!parentNode)
        return;
    const auto newNode = new Node;
    newNode->parent = parentNode;
    newNode->name = parentNode->uniqueChildName();
    beginInsertRows(parent, parentNode->children.size(), parentNode->children.size());
    parentNode->children << newNode;
    endInsertRows();
    d->dirty = true;
}

void SettingsModel::removeKey(const QModelIndex &index)
{
    Node * const node = d->indexToNode(index);
    if (!node || node == &d->rootNode)
        return;
    const int positionInParent = node->parent->children.indexOf(node);
    beginRemoveRows(parent(index), positionInParent, positionInParent);
    node->parent->children.removeAt(positionInParent);
    delete node;
    endRemoveRows();
    d->dirty = true;
}

bool SettingsModel::hasUnsavedChanges() const
{
    return d->dirty;
}

void SettingsModel::setEditable(bool isEditable)
{
    d->editable = isEditable;
}

void SettingsModel::setAdditionalProperties(const QVariantMap &properties)
{
    d->additionalProperties = properties;
    reload();
}

Qt::ItemFlags SettingsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    const Qt::ItemFlags flags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if (index.column() == keyColumn()) {
        if (d->editable)
            return flags | Qt::ItemIsEditable;
        return flags;
    }
    if (index.column() == valueColumn()) {
        const Node * const node = d->indexToNode(index);
        if (!node)
            return Qt::NoItemFlags;

        // Only leaf nodes have values.
        return d->editable && node->children.empty() ? flags | Qt::ItemIsEditable : flags;
    }
    return {};
}

QVariant SettingsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};
    if (role != Qt::DisplayRole)
        return {};
    if (section == keyColumn())
        return tr("Key");
    if (section == valueColumn())
        return tr("Value");
    return {};
}

int SettingsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

int SettingsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    const Node * const node = d->indexToNode(parent);
    Q_ASSERT(node);
    return node->children.size();
}

QVariant SettingsModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ForegroundRole)
        return {};
    const Node * const node = d->indexToNode(index);
    if (!node)
        return {};
    if (role == Qt::ForegroundRole) {
#ifdef QT_GUI_LIB
        if (index.column() == valueColumn() && !node->isFromSettings)
            return QBrush(Qt::red);
#endif
        return {};
    }
    if (index.column() == keyColumn())
        return node->name;
    if (index.column() == valueColumn() && node->children.empty())
        return node->value;
    return {};
}

bool SettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    Node * const node = d->indexToNode(index);
    if (!node)
        return false;
    const QString valueString = value.toString();
    QString *toChange = nullptr;
    if (index.column() == keyColumn() && !valueString.isEmpty()
            && !node->parent->hasDirectChildWithName(valueString)
            && !(node->parent->parent == &d->rootNode
                 && node->parent->name == Internal::StringConstants::profilesSettingsKey()
                 && valueString == Profile::fallbackName())) {
        toChange = &node->name;
    } else if (index.column() == valueColumn() && valueString != node->value) {
        toChange = &node->value;
    }

    if (toChange) {
        *toChange = valueString;
        emit dataChanged(index, index);
        d->dirty = true;
    }
    return toChange;
}

QModelIndex SettingsModel::index(int row, int column, const QModelIndex &parent) const
{
    const Node * const parentNode = d->indexToNode(parent);
    Q_ASSERT(parentNode);
    if (parentNode->children.size() <= row)
        return {};
    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex SettingsModel::parent(const QModelIndex &child) const
{
    const auto childNode = static_cast<const Node *>(child.internalPointer());
    Q_ASSERT(childNode);
    Node * const parentNode = childNode->parent;
    if (parentNode == &d->rootNode)
        return {};
    const Node * const grandParentNode = parentNode->parent;
    Q_ASSERT(grandParentNode);
    return createIndex(grandParentNode->children.indexOf(parentNode), 0, parentNode);
}


void SettingsModel::SettingsModelPrivate::readSettings()
{
    qDeleteAll(rootNode.children);
    rootNode.children.clear();
    const auto topLevelKeys = settings->directChildren(QString(), scope());
    for (const QString &topLevelKey : topLevelKeys)
        addNodeFromSettings(&rootNode, topLevelKey);
    for (QVariantMap::ConstIterator it = additionalProperties.constBegin();
         it != additionalProperties.constEnd(); ++it) {
        const QStringList nameAsList = it.key().split(QLatin1Char('.'), QBS_SKIP_EMPTY_PARTS);
        addNode(&rootNode, nameAsList.front(), nameAsList.mid(1), it.value());
    }
    dirty = false;
}

static Node *createNode(Node *parentNode, const QString &name)
{
    const auto node = new Node;
    node->name = name;
    node->parent = parentNode;
    parentNode->children.push_back(node);
    return node;
}

void SettingsModel::SettingsModelPrivate::addNodeFromSettings(Node *parentNode,
                                                  const QString &fullyQualifiedName)
{
    const QString &nodeName
            = fullyQualifiedName.mid(fullyQualifiedName.lastIndexOf(QLatin1Char('.')) + 1);
    Node * const node = createNode(parentNode, nodeName);
    node->value = settingsValueToRepresentation(settings->value(fullyQualifiedName, scope()));
    const auto childKeys = settings->directChildren(fullyQualifiedName, scope());
    for (const QString &childKey : childKeys)
        addNodeFromSettings(node, fullyQualifiedName + QLatin1Char('.') + childKey);
    dirty = true;
}

void SettingsModel::SettingsModelPrivate::addNode(qbs::Internal::Node *parentNode,
        const QString &currentNamePart, const QStringList &restOfName, const QVariant &value)
{
    Node *currentNode = nullptr;
    for (Node * const n : qAsConst(parentNode->children)) {
        if (n->name == currentNamePart) {
            currentNode = n;
            break;
        }
    }
    if (!currentNode)
        currentNode = createNode(parentNode, currentNamePart);
    if (restOfName.empty()) {
        currentNode->value = settingsValueToRepresentation(value);
        currentNode->isFromSettings = false;
    } else {
        addNode(currentNode, restOfName.front(), restOfName.mid(1), value);
    }
}

void SettingsModel::SettingsModelPrivate::doSave(const Node *node, const QString &prefix)
{
    if (node->children.empty()) {
        settings->setValue(prefix + node->name, representationToSettingsValue(node->value));
        return;
    }

    const QString newPrefix = prefix + node->name + QLatin1Char('.');
    for (const Node * const child : qAsConst(node->children))
        doSave(child, newPrefix);
}

Node *SettingsModel::SettingsModelPrivate::indexToNode(const QModelIndex &index)
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : &rootNode;
}


} // namespace qbs
