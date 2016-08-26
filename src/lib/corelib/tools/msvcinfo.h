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

#ifndef QBS_MSVCINFO_H
#define QBS_MSVCINFO_H

#include <logging/translator.h>
#include <tools/error.h>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcessEnvironment>
#include <QStringList>

namespace qbs {
namespace Internal {

class Version;

class MSVC
{
public:
    QString version;
    QString installPath;
    QString pathPrefix;
    QStringList architectures;

    typedef QHash<QString, QProcessEnvironment> EnvironmentPerArch;
    EnvironmentPerArch environments;

    MSVC() { }

    MSVC(const QString &clPath)
    {
        QDir parentDir = QFileInfo(clPath).dir();
        QString arch = parentDir.dirName().toLower();
        if (arch == QLatin1String("bin"))
            arch = QString(); // x86
        else
            parentDir.cdUp();
        architectures << arch;
        installPath = parentDir.path();
    }

    QString clPath(const QString &arch = QString()) const {
        return QDir::cleanPath(
                    installPath + QLatin1Char('/') +
                    pathPrefix + QLatin1Char('/') +
                    arch + QLatin1Char('/') +
                    QLatin1String("cl.exe"));
    }

    QBS_EXPORT QVariantMap compilerDefines(const QString &compilerFilePath) const;
};

class WinSDK : public MSVC
{
public:
    bool isDefault;

    WinSDK()
    {
        pathPrefix = QLatin1String("bin");
    }
};

} // namespace Internal
} // namespace qbs

#endif // QBS_MSVCINFO_H
