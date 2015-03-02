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

#include "settings.h"

#include "error.h"
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QSettings>

#include <algorithm>

namespace qbs {
using namespace Internal;

static QSettings::Format format()
{
    return HostOsInfo::isWindowsHost() ? QSettings::IniFormat : QSettings::NativeFormat;
}

static QSettings *createQSettings(const QString &baseDir)
{
    return baseDir.isEmpty()
            ? new QSettings(format(), QSettings::UserScope, QLatin1String("QtProject"),
                            QLatin1String("qbs"))
            : new QSettings(baseDir + QLatin1String("/qbs.conf"), format());
}

Settings::Settings(const QString &baseDir)
    : m_settings(createQSettings(baseDir)), m_baseDir(baseDir)
{
    // Actual qbs settings are stored transparently within a group, because QSettings
    // can see non-qbs fallback settings e.g. from QtProject that we're not interested in.
    m_settings->beginGroup(QLatin1String("org/qt-project/qbs"));
}

Settings::~Settings()
{
    delete m_settings;
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(internalRepresentation(key), defaultValue);
}

QStringList Settings::allKeys() const
{
    QStringList keys  = m_settings->allKeys();
    fixupKeys(keys);
    return keys;
}

QStringList Settings::directChildren(const QString &parentGroup)
{
    m_settings->beginGroup(internalRepresentation(parentGroup));
    QStringList children = m_settings->childGroups();
    children << m_settings->childKeys();
    m_settings->endGroup();
    fixupKeys(children);
    return children;
}

QStringList Settings::allKeysWithPrefix(const QString &group) const
{
    m_settings->beginGroup(internalRepresentation(group));
    QStringList keys = m_settings->allKeys();
    m_settings->endGroup();
    fixupKeys(keys);
    return keys;
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(internalRepresentation(key), value);
}

void Settings::remove(const QString &key)
{
    m_settings->remove(internalRepresentation(key));
}

void Settings::clear()
{
    m_settings->clear();
}

void Settings::sync()
{
    m_settings->sync();
}

QString Settings::defaultProfile() const
{
    return value(QLatin1String("defaultProfile")).toString();
}

QStringList Settings::profiles() const
{
    m_settings->beginGroup(QLatin1String("profiles"));
    QStringList result = m_settings->childGroups();
    m_settings->endGroup();
    return result;
}

QString Settings::fileName() const
{
    return m_settings->fileName();
}

QString Settings::internalRepresentation(const QString &externalKey) const
{
    QString internalKey = externalKey;
    return internalKey.replace(QLatin1Char('.'), QLatin1Char('/'));
}

QString Settings::externalRepresentation(const QString &internalKey) const
{
    QString externalKey = internalKey;
    return externalKey.replace(QLatin1Char('/'), QLatin1Char('.'));
}

void Settings::fixupKeys(QStringList &keys) const
{
    keys.sort();
    keys.removeDuplicates();
    for (QStringList::Iterator it = keys.begin(); it != keys.end(); ++it)
        *it = externalRepresentation(*it);
}

} // namespace qbs
