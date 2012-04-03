/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace qbs {

class Settings
{
public:
    ~Settings();

    typedef QSharedPointer<Settings> Ptr;
    static Ptr create() { return Ptr(new Settings); }

    enum Scope
    {
        Local, Global
    };

    void loadProjectSettings(const QString &projectFileName);
    bool hasProjectSettings() const { return m_localSettings; }
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QVariant value(Scope scope, const QString &key, const QVariant &defaultValue = QVariant()) const;
    QVariant moduleValue(const QString &key, const QList<QString> &profiles, const QVariant &defaultValue = QVariant());
    QStringList allKeys() const;
    QStringList allKeys(Scope scope) const;
    QStringList allKeysWithPrefix(const QString &group);
    void setValue(const QString &key, const QVariant &value);
    void setValue(Scope scope, const QString &key, const QVariant &value);
    void remove(Scope scope, const QString &key);

protected:
    Settings();

protected:
    QSettings *m_globalSettings;
    QSettings *m_localSettings;
};

}

#endif // SETTINGS_H
