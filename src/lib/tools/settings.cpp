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

#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace qbs {

Settings::Settings()
    : m_globalSettings(0),
      m_localSettings(0)
{
    m_globalSettings = new QSettings(QSettings::UserScope,
                                     QLatin1String("Nokia"),
                                     QLatin1String("qbs"));
    m_globalSettings->setFallbacksEnabled(false);
}

Settings::~Settings()
{
    delete m_globalSettings;
    delete m_localSettings;
}

void Settings::loadProjectSettings(const QString &projectFileName)
{
    delete m_localSettings;
    m_localSettings = new QSettings(QFileInfo(projectFileName).path()
            + "/.qbs/config", QSettings::IniFormat);
    m_localSettings->setFallbacksEnabled(false);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    if (m_localSettings && m_localSettings->contains(key))
        return m_localSettings->value(key, defaultValue);
    return m_globalSettings->value(key, defaultValue);
}

QVariant Settings::value(Scope scope, const QString &key, const QVariant &defaultValue) const
{
    QSettings *s = (scope == Global ? m_globalSettings : m_localSettings);
    return s ? s->value(key, defaultValue) : defaultValue;
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
    QStringList keys;
    if (m_localSettings)
        keys = m_localSettings->allKeys();
    keys.append(m_globalSettings->allKeys());
    keys.sort();
    std::unique(keys.begin(), keys.end());
    return keys;
}

QStringList Settings::allKeysWithPrefix(const QString &group)
{
    QStringList keys;
    if (m_localSettings) {
        m_localSettings->beginGroup(group);
        keys = m_localSettings->allKeys();
        m_localSettings->endGroup();
    }

    m_globalSettings->beginGroup(group);
    keys.append(m_globalSettings->allKeys());
    m_globalSettings->endGroup();
    keys.sort();
    std::unique(keys.begin(), keys.end());
    keys.removeDuplicates();
    return keys;
}

QStringList Settings::allKeys(Scope scope) const
{
    QSettings *s = (scope == Global ? m_globalSettings : m_localSettings);
    return s ? s->allKeys() : QStringList();
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    setValue(Global, key, value);
}

void Settings::setValue(Scope scope, const QString &key, const QVariant &value)
{
    QSettings *s = (scope == Global ? m_globalSettings : m_localSettings);
    if (s) {
        s->setValue(key, value);
        checkStatus(s);
    }
}

void Settings::remove(Settings::Scope scope, const QString &key)
{
    QSettings *s = (scope == Global ? m_globalSettings : m_localSettings);
    if (s) {
        s->remove(key);
        checkStatus(s);
    }
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

void Settings::checkStatus(QSettings *s)
{
    s->sync();
    switch (s->status()) {
    case QSettings::NoError:
        break;
    case QSettings::AccessError:
        throw Error(tr("%1 is not accessible.").arg(s->fileName()));
    case QSettings::FormatError:
        throw Error(tr("Format error in %1.").arg(s->fileName()));
    }
}

} // namespace qbs
