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

#include "settings.h"

#include "error.h"
#include "profile.h"
#include "settingscreator.h"

#include <logging/translator.h>

#include <QtCore/qsettings.h>

#include <algorithm>

namespace qbs {
using namespace Internal;

Settings::Settings(const QString &baseDir)
    : m_settings(SettingsCreator(baseDir).getQSettings()), m_baseDir(baseDir)
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
    if (key.startsWith(QLatin1String("profiles.") + Profile::fallbackName()))  {
        throw ErrorInfo(Tr::tr("Invalid use of special profile name '%1'.")
                        .arg(Profile::fallbackName()));
    }
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
