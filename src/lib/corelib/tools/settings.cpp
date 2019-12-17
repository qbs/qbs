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
#include <tools/hostosinfo.h>
#include <tools/stringconstants.h>

#include <QtCore/qsettings.h>

#include <algorithm>

namespace qbs {
using namespace Internal;

QString Settings::defaultSystemSettingsBaseDir()
{
    switch (HostOsInfo::hostOs()) {
    case HostOsInfo::HostOsWindows: {
        const char key[] = "ALLUSERSAPPDATA";
        if (qEnvironmentVariableIsSet(key))
            return QLatin1String(key);
        return QStringLiteral("C:/ProgramData");
    }
    case HostOsInfo::HostOsMacos:
        return QStringLiteral("/Library/Application Support");
    case HostOsInfo::HostOsLinux:
    case HostOsInfo::HostOsOtherUnix:
        return QStringLiteral("/etc/xdg");
    default:
        return {};
    }
}

static QString systemSettingsBaseDir()
{
#ifdef QBS_ENABLE_UNIT_TESTS
    const char key[] = "QBS_AUTOTEST_SYSTEM_SETTINGS_DIR";
    if (qEnvironmentVariableIsSet(key))
        return QLatin1String(qgetenv(key));
#endif
#ifdef QBS_SYSTEM_SETTINGS_DIR
    return QLatin1String(QBS_SYSTEM_SETTINGS_DIR);
#else
    return Settings::defaultSystemSettingsBaseDir() + QStringLiteral("/qbs");
#endif
}

Settings::Settings(const QString &baseDir) : Settings(baseDir, systemSettingsBaseDir()) { }

Settings::Settings(const QString &baseDir, const QString &systemBaseDir)
    : m_settings(SettingsCreator(baseDir).getQSettings()),
      m_systemSettings(std::make_unique<QSettings>(systemBaseDir + QStringLiteral("/qbs.conf"),
                                                   QSettings::IniFormat)),
      m_baseDir(baseDir)
{
    // Actual qbs settings are stored transparently within a group, because QSettings
    // can see non-qbs fallback settings e.g. from QtProject that we're not interested in.
    m_settings->beginGroup(QStringLiteral("org/qt-project/qbs"));
}

Settings::~Settings() = default;

QVariant Settings::value(const QString &key, Scopes scopes, const QVariant &defaultValue) const
{
    QVariant userValue;
    if (scopes & UserScope)
        userValue = m_settings->value(internalRepresentation(key));
    QVariant systemValue;
    if (scopes & SystemScope)
        systemValue = m_systemSettings->value(internalRepresentation(key));
    if (!userValue.isValid()) {
        if (systemValue.isValid())
            return systemValue;
        return defaultValue;
    }
    if (!systemValue.isValid())
        return userValue;
    if (static_cast<QMetaType::Type>(userValue.type()) == QMetaType::QStringList)
        return userValue.toStringList() + systemValue.toStringList();
    if (static_cast<QMetaType::Type>(userValue.type()) == QMetaType::QVariantList)
        return userValue.toList() + systemValue.toList();
    return userValue;
}

QStringList Settings::allKeys(Scopes scopes) const
{
    QStringList keys;
    if (scopes & UserScope)
        keys = m_settings->allKeys();
    if (scopes & SystemScope)
        keys += m_systemSettings->allKeys();
    fixupKeys(keys);
    return keys;
}

QStringList Settings::directChildren(const QString &parentGroup, Scope scope) const
{
    QSettings * const settings = settingsForScope(scope);
    settings->beginGroup(internalRepresentation(parentGroup));
    QStringList children = settings->childGroups();
    children << settings->childKeys();
    settings->endGroup();
    fixupKeys(children);
    return children;
}

QStringList Settings::allKeysWithPrefix(const QString &group, Scopes scopes) const
{
    QStringList keys;
    if (scopes & UserScope) {
        m_settings->beginGroup(internalRepresentation(group));
        keys = m_settings->allKeys();
        m_settings->endGroup();
    }
    if (scopes & SystemScope) {
        m_systemSettings->beginGroup(internalRepresentation(group));
        keys += m_systemSettings->allKeys();
        m_systemSettings->endGroup();
    }
    fixupKeys(keys);
    return keys;
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    if (key.startsWith(StringConstants::profilesSettingsPrefix() + Profile::fallbackName()))  {
        throw ErrorInfo(Tr::tr("Invalid use of special profile name '%1'.")
                        .arg(Profile::fallbackName()));
    }
    targetForWriting()->setValue(internalRepresentation(key), value);
    checkForWriteError();
}

void Settings::remove(const QString &key)
{
    targetForWriting()->remove(internalRepresentation(key));
    checkForWriteError();
}

void Settings::clear()
{
    targetForWriting()->clear();
    checkForWriteError();
}

void Settings::sync()
{
    targetForWriting()->sync();
}

QString Settings::defaultProfile() const
{
    return value(QStringLiteral("defaultProfile"), allScopes()).toString();
}

QStringList Settings::profiles() const
{
    QStringList result;
    if (m_scopeForWriting == UserScope) {
        m_settings->beginGroup(StringConstants::profilesSettingsKey());
        result = m_settings->childGroups();
        m_settings->endGroup();
    }
    m_systemSettings->beginGroup(StringConstants::profilesSettingsKey());
    result += m_systemSettings->childGroups();
    m_systemSettings->endGroup();
    result.removeDuplicates();
    return result;
}

QString Settings::fileName() const
{
    return targetForWriting()->fileName();
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
    for (auto &key : keys)
        key = externalRepresentation(key);
}

QSettings *Settings::settingsForScope(Settings::Scope scope) const
{
    return scope == UserScope ? m_settings.get() : m_systemSettings.get();
}

QSettings *Settings::targetForWriting() const
{
    return settingsForScope(m_scopeForWriting);
}

void Settings::checkForWriteError()
{
    if (m_scopeForWriting == SystemScope && m_systemSettings->status() == QSettings::NoError) {
        sync();
        if (m_systemSettings->status() == QSettings::AccessError)
            throw ErrorInfo(Tr::tr("Failure writing system settings file '%1': "
                                   "You do not have permission to write to that location.")
                            .arg(fileName()));
    }
}

} // namespace qbs
