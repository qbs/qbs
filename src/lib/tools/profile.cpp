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
#include "profile.h"

#include "settings.h"

namespace qbs {

/*!
 * \class Profile
 * \brief The \c Profile class gives access to the settings of a given profile.
 */

/*!
 * \brief Creates an object giving access to the settings for profile \c name.
 */
Profile::Profile(const QString &name, Settings *settings) : m_name(name)
{
    if (settings) {
        m_settings = settings;
        m_deleteSettings = false;
    } else {
        m_settings = new Settings;
        m_deleteSettings = true;
    }
}

Profile::~Profile()
{
    if (m_deleteSettings)
        delete m_settings;
}

/*!
 * \brief Returns the value for property \c key in this profile.
 */
// TODO: Take untranslated key (using dots), let Settings class handle conversion
QVariant Profile::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(fullyQualifiedKey(key), defaultValue);
}

/*!
 * \brief Gives value \c value to the property \c key in this profile.
 */
void Profile::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(fullyQualifiedKey(key), value);
}

/*!
 * \brief Returns all property keys in this profile.
 */
QStringList Profile::allKeys() const
{
    return m_settings->allKeysWithPrefix(profileKey());
}

QString Profile::profileKey() const
{
    return QLatin1String("profiles/") + m_name;
}

QString Profile::fullyQualifiedKey(const QString &key) const
{
    return profileKey() + QLatin1Char('/') + key;
}

} // namespace qbs
