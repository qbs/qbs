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
#ifndef QBS_PROFILE_H
#define QBS_PROFILE_H

#include "qbs_export.h"

#include <QString>
#include <QStringList>
#include <QVariant>

namespace qbs {
class Settings;

class QBS_EXPORT Profile
{
public:
    explicit Profile(const QString &name, Settings *settings);

    bool exists() const;
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);

    QString name() const;

    QString baseProfile() const;
    void setBaseProfile(const QString &baseProfile);
    void removeBaseProfile();

    void removeProfile();

    enum KeySelection { KeySelectionRecursive,  KeySelectionNonRecursive };
    QStringList allKeys(KeySelection selection) const;

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

} // namespace qbs

#endif // Header guard
