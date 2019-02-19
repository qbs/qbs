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
#ifndef QBS_PREFERENCES_H
#define QBS_PREFERENCES_H

#include "qbs_export.h"

#include "commandechomode.h"
#include "joblimits.h"
#include "settings.h"

#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

namespace qbs {
class Settings;

class QBS_EXPORT Preferences
{
public:
    explicit Preferences(Settings *settings, QString profileName = QString());
    Preferences(Settings *settings, QVariantMap profileContents);

    bool useColoredOutput() const;
    int jobs() const;
    QString shell() const;
    QString defaultBuildDirectory() const;
    CommandEchoMode defaultEchoMode() const;
    QStringList searchPaths(const QString &baseDir = QString()) const;
    QStringList pluginPaths(const QString &baseDir = QString()) const;
    JobLimits jobLimits() const;

private:
    QVariant getPreference(const QString &key, const QVariant &defaultValue = QVariant()) const;
    QStringList pathList(const QString &key, const QString &defaultValue) const;

    bool ignoreSystemSearchPaths() const;
    Settings::Scopes scopesForSearchPaths() const;

    Settings *m_settings;
    QString m_profile;
    QVariantMap m_profileContents;
};

} // namespace qbs


#endif // Header guard
