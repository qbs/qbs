#include <utility>

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
#include "preferences.h"

#include "buildoptions.h"
#include "hostosinfo.h"
#include "profile.h"
#include "stringconstants.h"

namespace qbs {

/*!
 * \class Preferences
 * \brief The \c Preferences class gives access to all general qbs preferences.
 * If a non-empty \c profileName is given, the profile's preferences take precedence over global
 * ones. Otherwise, the global preferences are used.
 */
Preferences::Preferences(Settings *settings, QString profileName)
    : m_settings(settings), m_profile(std::move(profileName))
{
}

Preferences::Preferences(Settings *settings, QVariantMap profileContents)
    : m_settings(settings), m_profileContents(std::move(profileContents))
{
}


/*!
 * \brief Returns true <=> colored output should be used for printing messages.
 * This is only relevant for command-line frontends.
 */
bool Preferences::useColoredOutput() const
{
    return getPreference(QStringLiteral("useColoredOutput"), true).toBool();
}

/*!
 * \brief Returns the number of parallel jobs to use for building.
 * Uses a sensible default value if there is no such setting.
 */
int Preferences::jobs() const
{
    return getPreference(QStringLiteral("jobs"), BuildOptions::defaultMaxJobCount()).toInt();
}

/*!
 * \brief Returns the shell to use for the "qbs shell" command.
 * This is only relevant for command-line frontends.
 */
QString Preferences::shell() const
{
    return getPreference(QStringLiteral("shell")).toString();
}

/*!
 * \brief Returns the default build directory used by Qbs if none is specified.
 */
QString Preferences::defaultBuildDirectory() const
{
    return getPreference(QStringLiteral("defaultBuildDirectory")).toString();
}

/*!
 * \brief Returns the default echo mode used by Qbs if none is specified.
 */
CommandEchoMode Preferences::defaultEchoMode() const
{
    return commandEchoModeFromName(getPreference(QStringLiteral("defaultEchoMode")).toString());
}

/*!
 * \brief Returns the list of paths where qbs looks for modules and imports.
 * In addition to user-supplied locations, they will also be looked up at \c{baseDir}/share/qbs.
 */
QStringList Preferences::searchPaths(const QString &baseDir) const
{
    return pathList(Internal::StringConstants::qbsSearchPathsProperty(),
                    baseDir + QLatin1String("/share/qbs"));
}

/*!
 * \brief Returns the list of paths where qbs looks for plugins.
 * In addition to user-supplied locations, they will be looked up at \c{baseDir}/qbs/plugins.
 */
QStringList Preferences::pluginPaths(const QString &baseDir) const
{
    return pathList(QStringLiteral("pluginsPath"), baseDir + QStringLiteral("/qbs/plugins"));
}

/*!
 * \brief Returns the per-pool job limits.
 */
JobLimits Preferences::jobLimits() const
{
    const QString prefix = QStringLiteral("preferences.jobLimit");
    JobLimits limits;
    const auto keys = m_settings->allKeysWithPrefix(prefix, Settings::allScopes());
    for (const QString &key : keys) {
        limits.setJobLimit(key, m_settings->value(prefix + QLatin1Char('.') + key,
                                                  Settings::allScopes()).toInt());
    }
    const QString fullPrefix = prefix + QLatin1Char('.');
    if (!m_profile.isEmpty()) {
        Profile p(m_profile, m_settings, m_profileContents);
        const auto keys = p.allKeys(Profile::KeySelectionRecursive);
        for (const QString &key : keys) {
            if (!key.startsWith(fullPrefix))
                continue;
            const QString jobPool = key.mid(fullPrefix.size());
            const int limit = p.value(key).toInt();
            if (limit >= 0)
                limits.setJobLimit(jobPool, limit);
        }
    }
    return limits;
}

QVariant Preferences::getPreference(const QString &key, const QVariant &defaultValue) const
{
    static const QString keyPrefix = QStringLiteral("preferences");
    const QString fullKey = keyPrefix + QLatin1Char('.') + key;
    const bool isSearchPaths = key == Internal::StringConstants::qbsSearchPathsProperty();
    if (!m_profile.isEmpty()) {
        QVariant value = Profile(m_profile, m_settings).value(fullKey);
        if (value.isValid()) {
            if (isSearchPaths) { // Merge with top-level value.
                value = value.toStringList() + m_settings->value(
                            fullKey, scopesForSearchPaths()).toStringList();
            }
            return value;
        }
    }

    QVariant value = m_profileContents.value(keyPrefix).toMap().value(key);
    if (value.isValid()) {
        if (isSearchPaths) {// Merge with top-level value
            value = value.toStringList() + m_settings->value(
                        fullKey, scopesForSearchPaths()).toStringList();
        }
        return value;
    }

    return m_settings->value(fullKey,
                             isSearchPaths ? scopesForSearchPaths() : Settings::allScopes(),
                             defaultValue);
}

QStringList Preferences::pathList(const QString &key, const QString &defaultValue) const
{
    QStringList paths = getPreference(key).toStringList();
    paths << defaultValue;
    return paths;
}

bool Preferences::ignoreSystemSearchPaths() const
{
    return getPreference(QStringLiteral("ignoreSystemSearchPaths")).toBool();
}

Settings::Scopes Preferences::scopesForSearchPaths() const
{
    return ignoreSystemSearchPaths() ? Settings::UserScope : Settings::allScopes();
}

} // namespace qbs
