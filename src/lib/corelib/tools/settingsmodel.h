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

#ifndef QBS_SETTINGSMODEL_H
#define QBS_SETTINGSMODEL_H

#include <tools/qbs_export.h>
#include <tools/settings.h>

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qvariant.h>

namespace qbs {

class QBS_EXPORT SettingsModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    SettingsModel(const QString &settingsDir, Settings::Scope scope, QObject *parent = nullptr);
    ~SettingsModel() override;

    int keyColumn() const { return 0; }
    int valueColumn() const { return 1; }
    bool hasUnsavedChanges() const;

    void setEditable(bool isEditable);
    void setAdditionalProperties(const QVariantMap &properties); // Flat map.
    void reload();
    void save();
    void updateSettingsDir(const QString &settingsDir);

    void addNewKey(const QModelIndex &parent);
    void removeKey(const QModelIndex &index);

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

private:
    class SettingsModelPrivate;
    SettingsModelPrivate * const d;
};

} // namespace qbs

#endif // QBS_SETTINGSMODEL_H
