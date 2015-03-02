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
#ifndef QBS_PREFERENCES_H
#define QBS_PREFERENCES_H

#include "qbs_export.h"

#include "commandechomode.h"

#include <QStringList>
#include <QVariant>

namespace qbs {
class Settings;

class QBS_EXPORT Preferences
{
public:
    explicit Preferences(Settings *settings, const QString &profileName = QString());

    bool useColoredOutput() const;
    int jobs() const;
    QString shell() const;
    QString defaultBuildDirectory() const;
    CommandEchoMode defaultEchoMode() const;
    QStringList searchPaths(const QString &baseDir = QString()) const;
    QStringList pluginPaths(const QString &baseDir = QString()) const;

private:
    QVariant getPreference(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QStringList pathList(const QString &key, const QString &defaultValue) const;

    Settings *m_settings;
    QString m_profile;
};

} // namespace qbs


#endif // Header guard
