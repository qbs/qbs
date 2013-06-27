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
#include "settingsmodel.h"

#include "../shared/qbssettings.h"

#include <QList>
#include <QString>

struct Node
{
    Node() : parent(0) {}
    ~Node() { qDeleteAll(children); }

    QString name;
    QString value;
    Node *parent;
    QList<Node *> children;
};

class SettingsModel::SettingsModelPrivate
{
public:
    void readSettings();
    void addNode(qbs::Settings *settings, Node *parentNode, const QString &fullyQualifiedName);
    Node *indexToNode(const QModelIndex &index);

    Node rootNode;
};

SettingsModel::SettingsModel(QObject *parent)
    : QAbstractItemModel(parent), d(new SettingsModelPrivate)
{
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

Qt::ItemFlags SettingsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemFlags();
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant SettingsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();
    if (section == 0)
        return tr("Key");
    return tr("Value");
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
    if (role != Qt::DisplayRole)
        return QVariant();
    const Node * const node = static_cast<Node *>(index.internalPointer());
    Q_ASSERT(node);
    if (index.column() == 0)
        return node->name;
    return node->value;
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
    SettingsPtr settings = qbsSettings();
    foreach (const QString &topLevelKey, settings->directChildren(QString()))
        addNode(settings.data(), &rootNode, topLevelKey);
}

void SettingsModel::SettingsModelPrivate::addNode(qbs::Settings *settings, Node *parentNode,
                                                  const QString &fullyQualifiedName)
{
    Node * const node = new Node;
    node->name = fullyQualifiedName.mid(fullyQualifiedName.lastIndexOf(QLatin1Char('.')) + 1);
    node->value = settings->value(fullyQualifiedName).toStringList().join(QLatin1String(","));
    node->parent = parentNode;
    parentNode->children << node;
    foreach (const QString &childKey, settings->directChildren(fullyQualifiedName))
        addNode(settings, node, fullyQualifiedName + QLatin1Char('.') + childKey);
}

Node *SettingsModel::SettingsModelPrivate::indexToNode(const QModelIndex &index)
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : &rootNode;
}
