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

#ifndef QBS_SETTINGS_H
#define QBS_SETTINGS_H

#include "qbs_export.h"

#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <memory>

QT_BEGIN_NAMESPACE
class QSettings;
class QStringList;
QT_END_NAMESPACE

namespace qbs {

class QBS_EXPORT Settings
{
public:
    // The "pure" base directory without any version scope. Empty string means "system default".
    Settings(const QString &baseDir);
    Settings(const QString &baseDir, const QString &systemBaseDir);

    ~Settings();

    enum Scope { UserScope = 0x1, SystemScope = 0x2 };
    Q_DECLARE_FLAGS(Scopes, Scope)
    static Scopes allScopes() { return {UserScope, SystemScope}; }

    QVariant value(const QString &key, Scopes scopes,
                   const QVariant &defaultValue = QVariant()) const;
    QStringList allKeys(Scopes scopes) const;
    QStringList directChildren(const QString &parentGroup, Scope scope) const; // Keys and groups.
    QStringList allKeysWithPrefix(const QString &group, Scopes scopes) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    void clear();
    void sync();

    void setScopeForWriting(Scope scope) { m_scopeForWriting = scope; }
    Scope scopeForWriting() const { return m_scopeForWriting; }

    QString defaultProfile() const;
    QStringList profiles() const;

    QString fileName() const;
    QString baseDirectory() const { return m_baseDir; } // As passed into the constructor.

    static QString defaultSystemSettingsBaseDir();

private:
    QString internalRepresentation(const QString &externalKey) const;
    QString externalRepresentation(const QString &internalKey) const;
    void fixupKeys(QStringList &keys) const;
    QSettings *settingsForScope(Scope scope) const;
    QSettings *targetForWriting() const;
    void checkForWriteError();

    const std::unique_ptr<QSettings> m_settings;
    const std::unique_ptr<QSettings> m_systemSettings;
    const QString m_baseDir;
    Scope m_scopeForWriting = UserScope;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Settings::Scopes)

} // namespace qbs

#endif // QBS_SETTINGS_H
