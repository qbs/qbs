/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "settingsmodel.h"

#include "../shared/qbssettings.h"

#include <QList>
#include <QString>

struct Node
{
    Node() : parent(0) {}
    ~Node() { qDeleteAll(children); }

    QString uniqueChildName() const;
    bool hasDirectChildWithName(const QString &name) const;

    QString name;
    QString value;
    Node *parent;
    QList<Node *> children;
};

class SettingsModel::SettingsModelPrivate
{
public:
    void readSettings();
    void addNode(Node *parentNode, const QString &fullyQualifiedName);
    void doSave(const Node *node, const QString &prefix);
    Node *indexToNode(const QModelIndex &index);

    Node rootNode;
    SettingsPtr settings;
    bool dirty;
};

SettingsModel::SettingsModel(QObject *parent)
    : QAbstractItemModel(parent), d(new SettingsModelPrivate)
{
    d->settings = qbsSettings();
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

void SettingsModel::addNewKey(const QModelIndex &parent)
{
    Node *parentNode = d->indexToNode(parent);
    if (!parentNode)
        return;
    Node * const newNode = new Node;
    newNode->parent = parentNode;
    newNode->name = parentNode->uniqueChildName();
    beginInsertRows(parent, parentNode->children.count(), parentNode->children.count());
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

Qt::ItemFlags SettingsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemFlags();
    const Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == keyColumn())
        return flags | Qt::ItemIsEditable;
    if (index.column() == valueColumn()) {
        const Node * const node = d->indexToNode(index);
        if (!node)
            return Qt::ItemFlags();

        // Only leaf nodes have values.
        return node->children.isEmpty() ? flags | Qt::ItemIsEditable : flags;
    }
    return Qt::ItemFlags();
}

QVariant SettingsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();
    if (section == keyColumn())
        return tr("Key");
    if (section == valueColumn())
        return tr("Value");
    return QVariant();
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
    return node->children.count();
}

QVariant SettingsModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();
    const Node * const node = d->indexToNode(index);
    if (!node)
        return QVariant();
    if (index.column() == keyColumn())
        return node->name;
    if (index.column() == valueColumn() && node->children.isEmpty())
        return node->value;
    return QVariant();
}

bool SettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    Node * const node = d->indexToNode(index);
    if (!node)
        return false;
    const QString valueString = value.toString();
    QString *toChange = 0;
    if (index.column() == keyColumn() && !valueString.isEmpty()
            && !node->parent->hasDirectChildWithName(valueString)) {
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
    if (parentNode->children.count() <= row)
        return QModelIndex();
    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex SettingsModel::parent(const QModelIndex &child) const
{
    Node * const childNode = static_cast<Node *>(child.internalPointer());
    Q_ASSERT(childNode);
    Node * const parentNode = childNode->parent;
    if (parentNode == &d->rootNode)
        return QModelIndex();
    const Node * const grandParentNode = parentNode->parent;
    Q_ASSERT(grandParentNode);
    return createIndex(grandParentNode->children.indexOf(parentNode), 0, parentNode);
}


void SettingsModel::SettingsModelPrivate::readSettings()
{
    qDeleteAll(rootNode.children);
    rootNode.children.clear();
    foreach (const QString &topLevelKey, settings->directChildren(QString()))
        addNode(&rootNode, topLevelKey);
    dirty = false;
}

void SettingsModel::SettingsModelPrivate::addNode(Node *parentNode,
                                                  const QString &fullyQualifiedName)
{
    Node * const node = new Node;
    node->name = fullyQualifiedName.mid(fullyQualifiedName.lastIndexOf(QLatin1Char('.')) + 1);
    node->value = settingsValueToRepresentation(settings->value(fullyQualifiedName));
    node->parent = parentNode;
    parentNode->children << node;
    foreach (const QString &childKey, settings->directChildren(fullyQualifiedName))
        addNode(node, fullyQualifiedName + QLatin1Char('.') + childKey);
    dirty = true;
}

void SettingsModel::SettingsModelPrivate::doSave(const Node *node, const QString &prefix)
{
    if (node->children.isEmpty()) {
        settings->setValue(prefix + node->name, representationToSettingsValue(node->value));
        return;
    }

    const QString newPrefix = prefix + node->name + QLatin1Char('.');
    foreach (const Node * const child, node->children)
        doSave(child, newPrefix);
}

Node *SettingsModel::SettingsModelPrivate::indexToNode(const QModelIndex &index)
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : &rootNode;
}


QString Node::uniqueChildName() const
{
    QString newName = QLatin1String("newkey");
    bool unique;
    do {
        unique = true;
        foreach (const Node *childNode, children) {
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
    foreach (const Node * const child, children) {
        if (child->name == name)
            return true;
    }
    return false;
}
