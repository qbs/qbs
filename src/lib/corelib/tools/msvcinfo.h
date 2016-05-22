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
