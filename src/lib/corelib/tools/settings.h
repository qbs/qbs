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

#ifndef QBS_SETTINGS_H
#define QBS_SETTINGS_H

#include "qbs_export.h"

#include <QStringList>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace qbs {

class QBS_EXPORT Settings
{
public:
    Settings(const QString &baseDir); // Empty string means "system default"
    ~Settings();

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QStringList allKeys() const;
    QStringList directChildren(const QString &parentGroup); // Keys and groups.
    QStringList allKeysWithPrefix(const QString &group) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    void clear();
    void sync();

    QString defaultProfile() const;
    QStringList profiles() const;

    QString fileName() const;
    QString baseDirectoy() const { return m_baseDir; }

private:
    QString internalRepresentation(const QString &externalKey) const;
    QString externalRepresentation(const QString &internalKey) const;
    void fixupKeys(QStringList &keys) const;

    QSettings * const m_settings;
    const QString m_baseDir;
};

} // namespace qbs

#endif // QBS_SETTINGS_H
