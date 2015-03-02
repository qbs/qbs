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
#ifndef QBS_PROFILE_H
#define QBS_PROFILE_H

#include "qbs_export.h"

#include <QString>
#include <QStringList>
#include <QVariant>

namespace qbs {
class ErrorInfo;
class Settings;

class QBS_EXPORT Profile
{
public:
    explicit Profile(const QString &name, Settings *settings);

    bool exists() const;
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant(),
                   ErrorInfo *error = 0) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);

    QString name() const;

    QString baseProfile() const;
    void setBaseProfile(const QString &baseProfile);
    void removeBaseProfile();

    void removeProfile();

    enum KeySelection { KeySelectionRecursive,  KeySelectionNonRecursive };
    QStringList allKeys(KeySelection selection, ErrorInfo *error = 0) const;

    static QString cleanName(const QString &name);

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
