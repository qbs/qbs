/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QCoreApplication>
#include <QSharedPointer>
#include <QStringList>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace qbs {

class Settings
{
    Q_DECLARE_TR_FUNCTIONS(Settings)
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
    QVariant moduleValue(const QString &key, const QString &profile,
            const QVariant &defaultValue = QVariant());
    QStringList allKeys() const;
    QStringList allKeys(Scope scope) const;
    QStringList allKeysWithPrefix(const QString &group);
    void setValue(const QString &key, const QVariant &value);
    void setValue(Scope scope, const QString &key, const QVariant &value);
    void remove(Scope scope, const QString &key);

    // Add convenience functions here.
    bool useColoredOutput() const;
    QStringList searchPaths() const;
    QStringList pluginPaths() const;
    QString buildVariant() const;

private:
    Settings();
    static void checkStatus(QSettings *s);
    QStringList pathList(const QString &key, const QString &defaultValue) const;

    QSettings *m_globalSettings;
    QSettings *m_localSettings;
};

}

#endif // SETTINGS_H
