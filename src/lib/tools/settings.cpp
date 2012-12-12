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

#include "settings.h"

#include "error.h"
#include "fileinfo.h"
#include "hostosinfo.h"
#include <logging/translator.h>

#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace qbs {
using namespace Internal;

Settings::Settings()
    : m_settings(new QSettings(QSettings::UserScope, QLatin1String("QtProject"),
                               QLatin1String("qbs")))
{
    m_settings->setFallbacksEnabled(false);

    // Fetch data from old Nokia settings, if necessary. TODO: Remove in 0.4.
    if (m_settings->allKeys().isEmpty()) {
        QSettings oldSettings(QSettings::UserScope, QLatin1String("Nokia"), QLatin1String("qbs"));
        oldSettings.setFallbacksEnabled(false);
        foreach (const QString &key, oldSettings.allKeys())
            m_settings->setValue(key, oldSettings.value(key));
    }
}

Settings::~Settings()
{
    delete m_settings;
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

QVariant Settings::moduleValue(const QString &key, const QString &profile,
        const QVariant &defaultValue)
{
    const QString profileKey = QString::fromLatin1("profiles/%1/%2").arg(profile, key);
    const QVariant val = value(profileKey);
    if (val.isValid())
        return val;
    const QString modulesKey = QString::fromLatin1("modules/%1").arg(key);
    return value(modulesKey, defaultValue);
}

QStringList Settings::allKeys() const
{
    QStringList keys  = m_settings->allKeys();
    keys.sort();
    std::unique(keys.begin(), keys.end());
    return keys;
}

QStringList Settings::allKeysWithPrefix(const QString &group)
{
    m_settings->beginGroup(group);
    QStringList keys = m_settings->allKeys();
    m_settings->endGroup();
    keys.sort();
    std::unique(keys.begin(), keys.end());
    keys.removeDuplicates();
    return keys;
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
    checkStatus();
}

void Settings::remove(const QString &key)
{
    m_settings->remove(key);
    checkStatus();
}

bool Settings::useColoredOutput() const
{
    return value(QLatin1String("preferences/useColoredOutput"), true).toBool();
}

QStringList Settings::searchPaths() const
{
    return pathList(QLatin1String("preferences/qbsPath"),
            qbsRootPath() + QLatin1String("/share/qbs/"));
}

QStringList Settings::pluginPaths() const
{
    return pathList(QLatin1String("preferences/pluginsPath"),
                    qbsRootPath() + QLatin1String("/plugins/"));
}

QStringList Settings::pathList(const QString &key, const QString &defaultValue) const
{
    QStringList paths = value(key).toString().split(HostOsInfo::pathListSeparator(),
                                                    QString::SkipEmptyParts);
    if (paths.isEmpty())
        paths << defaultValue;
    return paths;
}

QString Settings::buildVariant() const
{
    return value(QLatin1String("modules/qbs/buildVariant"), QLatin1String("debug")).toString();
}

void Settings::checkStatus()
{
    m_settings->sync();
    switch (m_settings->status()) {
    case QSettings::NoError:
        break;
    case QSettings::AccessError:
        throw Error(Tr::tr("%1 is not accessible.").arg(m_settings->fileName()));
    case QSettings::FormatError:
        throw Error(Tr::tr("Format error in %1.").arg(m_settings->fileName()));
    }
}

} // namespace qbs
