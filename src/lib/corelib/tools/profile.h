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
#ifndef QBS_PROFILE_H
#define QBS_PROFILE_H

#include "qbs_export.h"

#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE
class QStringList;
QT_END_NAMESPACE

namespace qbs {
class ErrorInfo;
class Settings;

class QBS_EXPORT Profile
{
public:
    Profile(QString name, Settings *settings, QVariantMap profiles = QVariantMap());

    bool exists() const;
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant(),
                   ErrorInfo *error = nullptr) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);

    QString name() const;

    QString baseProfile() const;
    void setBaseProfile(const QString &baseProfile);
    void removeBaseProfile();

    void removeProfile();

    enum KeySelection { KeySelectionRecursive,  KeySelectionNonRecursive };
    QStringList allKeys(KeySelection selection, ErrorInfo *error = nullptr) const;

    static QString cleanName(const QString &name);

    static QString fallbackName() { return QStringLiteral("none"); }

private:
    static QString baseProfileKey();
    void checkBaseProfileExistence(const Profile &baseProfile) const;
    QString profileKey() const;
    QVariant localValue(const QString &key) const;
    QString fullyQualifiedKey(const QString &key) const;
    QVariant possiblyInheritedValue(const QString &key, const QVariant &defaultValue,
                                    QStringList profileChain) const;
    QStringList allKeysInternal(KeySelection selection, QStringList profileChain) const;
    void extendAndCheckProfileChain(QStringList &chain) const;

    QString m_name;
    Settings *m_settings;
    QVariantMap m_values;
    QVariantMap m_profiles;
};

namespace Internal {
// Exported for autotests.
class QBS_EXPORT TemporaryProfile {
public:
    TemporaryProfile(const QString &name, Settings *settings) : p(name, settings) {}
    ~TemporaryProfile() { p.removeProfile(); }

    Profile p;
};
} // namespace Internal

} // namespace qbs

#endif // Header guard
