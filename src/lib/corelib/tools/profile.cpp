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
#include "profile.h"
#include "qbsassert.h"
#include "settings.h"
#include "stringconstants.h"

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
Profile::Profile(QString name, Settings *settings, QVariantMap profiles)
    : m_name(std::move(name)),
      m_settings(settings),
      m_values(profiles.value(m_name).toMap()),
      m_profiles(std::move(profiles))
{
    QBS_ASSERT(m_name == cleanName(m_name), return);
}

bool Profile::exists() const
{
    return m_name == fallbackName() || !m_values.empty()
            || !m_settings->allKeysWithPrefix(profileKey(), Settings::allScopes()).empty();
}

/*!
 * \brief Returns the value for property \c key in this profile.
 */
QVariant Profile::value(const QString &key, const QVariant &defaultValue, ErrorInfo *error) const
{
    try {
        return possiblyInheritedValue(key, defaultValue, QStringList());
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return {};
    }
}

/*!
 * \brief Gives value \c value to the property \c key in this profile.
 */
void Profile::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(fullyQualifiedKey(key), value);

    if (key == baseProfileKey()) {
        QBS_ASSERT(value.toString() == cleanName(value.toString()), return);
    }
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
QStringList Profile::allKeys(KeySelection selection, ErrorInfo *error) const
{
    try {
        return allKeysInternal(selection, QStringList());
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return {};
    }
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

/*!
 * \brief Returns a string suitiable as a profile name.
 * Removes all dots and replaces them with hyphens.
 */
QString Profile::cleanName(const QString &name)
{
    QString newName = name;
    return newName.replace(QLatin1Char('.'), QLatin1Char('-'));
}

QString Profile::profileKey() const
{
    return Internal::StringConstants::profilesSettingsPrefix() + m_name;
}

QString Profile::baseProfileKey()
{
    return Internal::StringConstants::baseProfileProperty();
}

void Profile::checkBaseProfileExistence(const Profile &baseProfile) const
{
    if (!baseProfile.exists())
        throw ErrorInfo(Internal::Tr::tr("Profile \"%1\" has a non-existent base profile \"%2\".").arg(
                        name(), baseProfile.name()));
}

QVariant Profile::localValue(const QString &key) const
{
    QVariant val = m_values.value(key);
    if (!val.isValid())
        val = m_settings->value(fullyQualifiedKey(key), Settings::allScopes());
    return val;
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
    Profile parentProfile(baseProfileName, m_settings, m_profiles);
    checkBaseProfileExistence(parentProfile);
    return parentProfile.possiblyInheritedValue(key, defaultValue, profileChain);
}

QStringList Profile::allKeysInternal(Profile::KeySelection selection,
                                     QStringList profileChain) const
{
    extendAndCheckProfileChain(profileChain);
    QStringList keys = m_values.keys();
    if (keys.empty())
        keys = m_settings->allKeysWithPrefix(profileKey(), Settings::allScopes());
    if (selection == KeySelectionNonRecursive)
        return keys;
    const QString baseProfileName = baseProfile();
    if (baseProfileName.isEmpty())
        return keys;
    Profile parentProfile(baseProfileName, m_settings, m_profiles);
    checkBaseProfileExistence(parentProfile);
    keys += parentProfile.allKeysInternal(KeySelectionRecursive, profileChain);
    keys.removeDuplicates();
    keys.removeOne(baseProfileKey());
    keys.sort();
    return keys;
}

void Profile::extendAndCheckProfileChain(QStringList &chain) const
{
    chain << m_name;
    if (Q_UNLIKELY(chain.count(m_name) > 1)) {
        throw ErrorInfo(Internal::Tr::tr("Circular profile inheritance. Cycle is '%1'.")
                    .arg(chain.join(QLatin1String(" -> "))));
    }
}

} // namespace qbs
