/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#ifndef QBS_SETTINGSCREATOR_H
#define QBS_SETTINGSCREATOR_H

#include "version.h"

#include <QString>

#include <memory>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

class SettingsCreator
{
public:
    SettingsCreator(const QString &baseDir);

    QSettings *getQSettings();

private:
    void migrate();
    void createQSettings();
    Version predecessor() const;

    QString m_settingsBaseDir;
    QString m_newSettingsDir;
    QString m_settingsFileName;
    QString m_newSettingsFilePath;
    std::unique_ptr<QSettings> m_settings;
    const Version m_qbsVersion;
};


} // namespace Internal
} // namespace qbs

#endif // Include guard.
