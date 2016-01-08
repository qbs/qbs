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

#include "settingscreator.h"

#include "fileinfo.h"
#include "hostosinfo.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace qbs {
namespace Internal {

static QSettings::Format format()
{
    return HostOsInfo::isWindowsHost() ? QSettings::IniFormat : QSettings::NativeFormat;
}


SettingsCreator::SettingsCreator(const QString &baseDir)
    : m_settingsBaseDir(baseDir), m_qbsVersion(Version::fromString(QLatin1String(QBS_VERSION)))
{
}

QSettings *SettingsCreator::getQSettings()
{
    createQSettings();
    migrate();
    return m_settings.release();
}

void SettingsCreator::migrate()
{
    if (!m_settings->allKeys().isEmpty()) // We already have settings for this qbs version.
        return;

    m_settings.reset();

    // Find settings from highest qbs version lower than this one and copy all settings data.
    const Version thePredecessor = predecessor();
    QString oldSettingsDir = m_settingsBaseDir;
    if (thePredecessor.isValid())
        oldSettingsDir.append(QLatin1String("/qbs/")).append(thePredecessor.toString());
    QString oldProfilesDir = oldSettingsDir;
    if (!thePredecessor.isValid())
        oldProfilesDir += QLatin1String("/qbs");
    oldProfilesDir += QLatin1String("/profiles");
    const QString newProfilesDir = m_newSettingsDir + QLatin1String("/profiles");
    QString errorMessage;
    if (QFileInfo(oldProfilesDir).exists()
            && !copyFileRecursion(oldProfilesDir, newProfilesDir, false, true, &errorMessage)) {
        qWarning() << "Error in settings migration: " << errorMessage;
    }
    const QString oldSettingsFilePath = oldSettingsDir + QLatin1Char('/') + m_settingsFileName;
    if (QFileInfo(oldSettingsFilePath).exists()
            && (!QDir::root().mkpath(m_newSettingsDir)
                || !QFile::copy(oldSettingsFilePath, m_newSettingsFilePath))) {
        qWarning() << "Error in settings migration: Could not copy" << oldSettingsFilePath
                   << "to" << m_newSettingsFilePath;
    }

    // Adapt all paths in settings that point to the old location. At the time of this writing,
    // that's only preferences.qbsSearchPaths as written by libqtprofilesetup, but we don't want
    // to hardcode that here.
    m_settings.reset(new QSettings(m_newSettingsFilePath, format()));
    foreach (const QString &key, m_settings->allKeys()) {
        QVariant v = m_settings->value(key);
        if (v.type() == QVariant::String) {
            QString s = v.toString();
            if (s.contains(oldProfilesDir))
                m_settings->setValue(key, s.replace(oldProfilesDir, newProfilesDir));
        } else if (v.type() == QVariant::StringList) {
            const QStringList oldList = v.toStringList();
            QStringList newList;
            foreach (const QString &oldString, oldList) {
                QString newString = oldString;
                newList << newString.replace(oldProfilesDir, newProfilesDir);
            }
            if (newList != oldList)
                m_settings->setValue(key, newList);
        }
    }
}

void SettingsCreator::createQSettings()
{
    std::unique_ptr<QSettings> tmp(m_settingsBaseDir.isEmpty()
            ? new QSettings(format(), QSettings::UserScope, QLatin1String("QtProject"),
                            QLatin1String("qbs"))
            : new QSettings(m_settingsBaseDir + QLatin1String("/qbs.conf"), format()));
    const QFileInfo fi(tmp->fileName());
    m_settingsBaseDir = fi.path();
    m_newSettingsDir = m_settingsBaseDir + QLatin1String("/qbs/") + m_qbsVersion.toString();
    m_settingsFileName = fi.fileName();
    m_newSettingsFilePath = m_newSettingsDir + QLatin1Char('/') + m_settingsFileName;
    m_settings.reset(new QSettings(m_newSettingsFilePath, tmp->format()));
}

Version SettingsCreator::predecessor() const
{
    QDirIterator dit(m_settingsBaseDir + QLatin1String("/qbs"));
    Version thePredecessor;
    while (dit.hasNext()) {
        dit.next();
        const auto currentVersion = Version::fromString(dit.fileName());
        if (currentVersion > thePredecessor && currentVersion < m_qbsVersion)
            thePredecessor = currentVersion;
    }
    return thePredecessor;
}

} // namespace Internal
} // namespace qbs
