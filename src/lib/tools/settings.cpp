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

#include "settings.h"

#include "error.h"
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QSettings>

#include <algorithm>

namespace qbs {
using namespace Internal;

static QSettings::Format format()
{
    return HostOsInfo::isWindowsHost() ? QSettings::IniFormat : QSettings::NativeFormat;
}

Settings::Settings(const QString &organization, const QString &application)
    : m_settings(new QSettings(format(), QSettings::UserScope, organization, application))
{
    m_settings->setFallbacksEnabled(false);
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
    m_settings->setValue(internalRepresentation(key), value);
    checkStatus();
}

void Settings::remove(const QString &key)
{
    m_settings->remove(internalRepresentation(key));
    checkStatus();
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

void Settings::checkStatus()
{
    m_settings->sync();
    switch (m_settings->status()) {
    case QSettings::NoError:
        break;
    case QSettings::AccessError:
        throw ErrorInfo(Tr::tr("%1 is not accessible.").arg(m_settings->fileName()));
    case QSettings::FormatError:
        throw ErrorInfo(Tr::tr("Format error in %1.").arg(m_settings->fileName()));
    }
}

} // namespace qbs
