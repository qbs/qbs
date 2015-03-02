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

#include <tools/qbs_export.h>

#include <QAbstractItemModel>
#include <QVariantMap>

namespace qbs {

class QBS_EXPORT SettingsModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    SettingsModel(const QString &settingsDir, QObject *parent = 0);
    ~SettingsModel();

    int keyColumn() const { return 0; }
    int valueColumn() const { return 1; }
    bool hasUnsavedChanges() const;

    void setEditable(bool isEditable);
    void setAdditionalProperties(const QVariantMap &properties); // Flat map.
    void reload();
    void save();

    void addNewKey(const QModelIndex &parent);
    void removeKey(const QModelIndex &index);

    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

private:
    class SettingsModelPrivate;
    SettingsModelPrivate * const d;
};

QBS_EXPORT QString settingsValueToRepresentation(const QVariant &value);
QBS_EXPORT QVariant representationToSettingsValue(const QString &representation);

} // namespace qbs
