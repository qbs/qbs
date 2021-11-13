#include <memory>
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

#include "settingscreator.h"

#include "fileinfo.h"
#include "hostosinfo.h"

#include <QtCore/qdiriterator.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstringlist.h>

namespace qbs {
namespace Internal {

static QSettings::Format format()
{
    return HostOsInfo::isWindowsHost() ? QSettings::IniFormat : QSettings::NativeFormat;
}


SettingsCreator::SettingsCreator(QString baseDir)
    : m_settingsBaseDir(std::move(baseDir))
    , m_qbsVersion(Version::fromString(QLatin1String(QBS_VERSION)))
{
}

std::unique_ptr<QSettings> SettingsCreator::getQSettings()
{
    createQSettings();
    migrate();
    return std::move(m_settings);
}

void SettingsCreator::migrate()
{
    if (!m_settings->allKeys().empty()) // We already have settings for this qbs version.
        return;

    m_settings.reset();

    // Find settings from highest qbs version lower than this one and copy all settings data.
    const Version thePredecessor = predecessor();
    QString oldSettingsDir = m_settingsBaseDir;
    if (thePredecessor.isValid())
        oldSettingsDir.append(QLatin1String("/qbs/")).append(thePredecessor.toString());
    const QString oldSettingsFilePath = oldSettingsDir + QLatin1Char('/') + m_settingsFileName;
    if (QFileInfo::exists(oldSettingsFilePath)
            && (!QDir::root().mkpath(m_newSettingsDir)
                || !QFile::copy(oldSettingsFilePath, m_newSettingsFilePath))) {
        qWarning() << "Error in settings migration: Could not copy" << oldSettingsFilePath
                   << "to" << m_newSettingsFilePath;
    }

    m_settings = std::make_unique<QSettings>(m_newSettingsFilePath, format());
}

void SettingsCreator::createQSettings()
{
    std::unique_ptr<QSettings> tmp(m_settingsBaseDir.isEmpty()
            ? new QSettings(format(), QSettings::UserScope, QStringLiteral("QtProject"),
                            QStringLiteral("qbs"))
            : new QSettings(m_settingsBaseDir + QLatin1String("/qbs.conf"), format()));
    const QFileInfo fi(tmp->fileName());
    m_settingsBaseDir = fi.path();
    m_newSettingsDir = m_settingsBaseDir + QLatin1String("/qbs/") + m_qbsVersion.toString();
    m_settingsFileName = fi.fileName();
    m_newSettingsFilePath = m_newSettingsDir + QLatin1Char('/') + m_settingsFileName;
    m_settings = std::make_unique<QSettings>(m_newSettingsFilePath, tmp->format());
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
