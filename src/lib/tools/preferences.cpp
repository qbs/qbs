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
#include "preferences.h"

#include "fileinfo.h"
#include "hostosinfo.h"
#include "settings.h"

#include <QThread>

namespace qbs {
using namespace Internal;

/*!
 * \class Preferences
 * \brief The \c Preferences class gives access to all general qbs preferences.
 */

Preferences::Preferences(Settings *settings)
{
    if (settings) {
        m_settings = settings;
        m_deleteSettings = false;
    } else {
        m_settings = new Settings;
        m_deleteSettings = true;
    }
}

Preferences::~Preferences()
{
    if (m_deleteSettings)
        delete m_settings;
}

/*!
 * \brief Returns true <=> colored output should be used for printing messages.
 * This is only relevant for command-line frontends.
 */
bool Preferences::useColoredOutput() const
{
    return getPreference(QLatin1String("useColoredOutput"), true).toBool();
}

/*!
 * \brief Returns the number of parallel jobs to use for building.
 */
int Preferences::jobs() const
{
    return getPreference(QLatin1String("jobs"), QThread::idealThreadCount()).toInt();
}

/*!
 * \brief Returns the list of paths where qbs looks for module definitions and such.
 * The separator is ":" on UNIX-like systems and ";" on Windows.
 */
QStringList Preferences::searchPaths() const
{
    return pathList(QLatin1String("qbsPath"), qbsRootPath() + QLatin1String("/share/qbs/"));
}

/*!
 * \brief Returns the list of paths where qbs looks for plugins.
 * The separator is ":" on UNIX-like systems and ";" on Windows.
 */
QStringList Preferences::pluginPaths() const
{
    return pathList(QLatin1String("pluginsPath"), qbsRootPath() + QLatin1String("/plugins/"));
}

QVariant Preferences::getPreference(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(QLatin1String("preferences/") + key, defaultValue);
}

QStringList Preferences::pathList(const QString &key, const QString &defaultValue) const
{
    QStringList paths = getPreference(key).toString().split(HostOsInfo::pathListSeparator(),
                                                            QString::SkipEmptyParts);
    if (paths.isEmpty())
        paths << defaultValue;
    return paths;
}

} // namespace qbs
