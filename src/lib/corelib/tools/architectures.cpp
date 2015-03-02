/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Petroules Corporation.
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

#include "architectures.h"

#include <QMap>
#include <QMapIterator>
#include <QStringList>

namespace qbs {

QString canonicalArchitecture(const QString &architecture)
{
    QMap<QString, QStringList> archMap;
    archMap.insert(QLatin1String("x86"), QStringList()
        << QLatin1String("i386")
        << QLatin1String("i486")
        << QLatin1String("i586")
        << QLatin1String("i686")
        << QLatin1String("ia32")
        << QLatin1String("ia-32")
        << QLatin1String("x86_32")
        << QLatin1String("x86-32")
        << QLatin1String("intel32")
        << QLatin1String("mingw32"));

    archMap.insert(QLatin1String("x86_64"), QStringList()
        << QLatin1String("x86-64")
        << QLatin1String("x64")
        << QLatin1String("amd64")
        << QLatin1String("ia32e")
        << QLatin1String("em64t")
        << QLatin1String("intel64")
        << QLatin1String("mingw64"));

    archMap.insert(QLatin1String("arm64"), QStringList()
        << QLatin1String("aarch64"));

    archMap.insert(QLatin1String("ia64"), QStringList()
        << QLatin1String("ia-64")
        << QLatin1String("itanium"));

    archMap.insert(QLatin1String("ppc"), QStringList()
        << QLatin1String("powerpc"));

    archMap.insert(QLatin1String("ppc64"), QStringList()
        << QLatin1String("powerpc64"));

    archMap.insert(QLatin1String("ppc64le"), QStringList()
        << QLatin1String("powerpc64le"));

    QMapIterator<QString, QStringList> i(archMap);
    while (i.hasNext()) {
        i.next();
        if (i.value().contains(architecture.toLower()))
            return i.key();
    }

    return architecture;

}

} // namespace qbs
