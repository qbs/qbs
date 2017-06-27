/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Petroules Corporation.
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

#include "architectures.h"

#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>

namespace qbs {

QString canonicalTargetArchitecture(const QString &architecture,
                                    const QString &endianness,
                                    const QString &vendor,
                                    const QString &system,
                                    const QString &abi)
{
    const QString arch = canonicalArchitecture(architecture);
    const bool isApple = (vendor == QStringLiteral("apple")
                          || system == QStringLiteral("darwin")
                          || system == QStringLiteral("macosx")
                          || system == QStringLiteral("ios")
                          || system == QStringLiteral("tvos")
                          || system == QStringLiteral("watchos")
                          || abi == QStringLiteral("macho"));
    const bool isQnx = (system == QStringLiteral("nto")
                        || abi.startsWith(QStringLiteral("qnx")));

    if (arch == QStringLiteral("armv7a")) {
        if (isApple)
            return QStringLiteral("armv7");
        if (isQnx)
            return QStringLiteral("arm");
    }

    if (arch == QStringLiteral("arm64") && isQnx)
        return QStringLiteral("aarch64");

    if (arch == QStringLiteral("x86")) {
        if (isQnx)
            return QStringLiteral("i586");
        return QStringLiteral("i386");
    }

    if (arch == QStringLiteral("mips") || arch == QStringLiteral("mips64")) {
        if (endianness == QStringLiteral("big"))
            return arch + QStringLiteral("eb");
        if (endianness == QStringLiteral("little"))
            return arch + QStringLiteral("el");
    }

    if (arch == QStringLiteral("ppc"))
        return QStringLiteral("powerpc");

    if (arch == QStringLiteral("ppc64") && endianness == QStringLiteral("little"))
        return arch + QStringLiteral("le");

    return arch;
}

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
        << QLatin1String("ppc64le")
        << QLatin1String("powerpc64")
        << QLatin1String("powerpc64le"));

    archMap.insert(QLatin1String("mips"), QStringList()
        << QLatin1String("mipseb")
        << QLatin1String("mipsel"));

    archMap.insert(QLatin1String("mips64"), QStringList()
        << QLatin1String("mips64eb")
        << QLatin1String("mips64el"));

    QMapIterator<QString, QStringList> i(archMap);
    while (i.hasNext()) {
        i.next();
        if (i.value().contains(architecture.toLower()))
            return i.key();
    }

    return architecture;

}

} // namespace qbs
