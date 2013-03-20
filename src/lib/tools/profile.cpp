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

#include <logging/translator.h>
#include <tools/error.h>

namespace qbs {

/*!
 * \class Profile
 * \brief The \c Profile class gives access to the settings of a given profile.
 */

 /*!
 * \enum Profile::KeySelection
 * This enum type specifies whether to enumerate keys recursively.
 * \value KeySelectionRecursive Indicates that key enumeration should happen recursively, i.e.
 *        it should go up the base profile chain.
 * \value KeySelectionNonRecursive Indicates that only keys directly attached to a profile
 *        should be listed.
 */

/*!
 * \brief Creates an object giving access to the settings for profile \c name.
 */
Profile::Profile(const QString &name, Settings *settings) : m_name(name), m_settings(settings)
{
}

/*!
 * \brief Returns the value for property \c key in this profile.
 */
QVariant Profile::value(const QString &key, const QVariant &defaultValue) const
{
    return possiblyInheritedValue(key, defaultValue, QStringList());
}

/*!
 * \brief Gives value \c value to the property \c key in this profile.
 */
void Profile::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(fullyQualifiedKey(key), value);
}

/*!
 * \brief Removes a key and the associated value from this profile.
 */
void Profile::remove(const QString &key)
{
    m_settings->remove(fullyQualifiedKey(key));
}

/*!
 * \brief Returns the name of this profile.
 */
QString Profile::name() const
{
    return m_name;
}

/*!
 * \brief Returns all property keys in this profile.
 * If and only if selection is Profile::KeySelectionRecursive, this will also list keys defined
 * in base profiles.
 */
QStringList Profile::allKeys(KeySelection selection) const
{
    return allKeysInternal(selection, QStringList());
}

/*!
 * \brief Returns the name of this profile's base profile.
 * The returned value is empty if the profile does not have a base profile.
 */
QString Profile::baseProfile() const
{
    return localValue(baseProfileKey()).toString();
}

/*!
 * \brief Sets a new base profile for this profile.
 */
void Profile::setBaseProfile(const QString &baseProfile)
{
    setValue(baseProfileKey(), baseProfile);
}

/*!
 * \brief Removes this profile's base profile setting.
 */
void Profile::removeBaseProfile()
{
    remove(baseProfileKey());
}

/*!
 * \brief Removes this profile from the settings.
 */
void Profile::removeProfile()
{
    m_settings->remove(profileKey());
}

QString Profile::profileKey() const
{
    return QLatin1String("profiles.") + m_name;
}

QString Profile::baseProfileKey()
{
    return QLatin1String("baseProfile");
}

QVariant Profile::localValue(const QString &key) const
{
    return m_settings->value(fullyQualifiedKey(key));
}

QString Profile::fullyQualifiedKey(const QString &key) const
{
    return profileKey() + QLatin1Char('.') + key;
}

QVariant Profile::possiblyInheritedValue(const QString &key, const QVariant &defaultValue,
                                         QStringList profileChain) const
{
    extendAndCheckProfileChain(profileChain);
    const QVariant v = localValue(key);
    if (v.isValid())
        return v;
    const QString baseProfileName = baseProfile();
    if (baseProfileName.isEmpty())
        return defaultValue;
    Profile parentProfile(baseProfileName, m_settings);
    return parentProfile.possiblyInheritedValue(key, defaultValue, profileChain);
}

QStringList Profile::allKeysInternal(Profile::KeySelection selection,
                                     QStringList profileChain) const
{
    extendAndCheckProfileChain(profileChain);
    QStringList keys = m_settings->allKeysWithPrefix(profileKey());
    if (selection == KeySelectionNonRecursive)
        return keys;
    const QString baseProfileName = baseProfile();
    if (baseProfileName.isEmpty())
        return keys;
    Profile parentProfile(baseProfileName, m_settings);
    keys += parentProfile.allKeysInternal(KeySelectionRecursive, profileChain);
    keys.removeDuplicates();
    keys.removeOne(baseProfileKey());
    keys.sort();
    return keys;
}

void Profile::extendAndCheckProfileChain(QStringList &chain) const
{
    chain << m_name;
    if (chain.count(m_name) > 1) {
        throw Error(Internal::Tr::tr("Circular profile inheritance. Cycle is '%1'.")
                    .arg(chain.join(QLatin1String(" -> "))));
    }
}

} // namespace qbs
