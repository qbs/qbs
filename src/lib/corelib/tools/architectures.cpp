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

#include "stringconstants.h"

#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>

namespace qbs {

using namespace Internal;

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
            return StringConstants::armv7Arch();
        if (isQnx)
            return StringConstants::armArch();
    }

    if (arch == StringConstants::arm64Arch() && isQnx)
        return StringConstants::aarch64Arch();

    if (arch == StringConstants::x86Arch()) {
        if (isQnx)
            return StringConstants::i586Arch();
        return StringConstants::i386Arch();
    }

    if (arch == StringConstants::mipsArch() || arch == StringConstants::mips64Arch()) {
        if (endianness == QStringLiteral("big"))
            return arch + QStringLiteral("eb");
        if (endianness == QStringLiteral("little"))
            return arch + QStringLiteral("el");
    }

    if (arch == StringConstants::ppcArch())
        return StringConstants::powerPcArch();

    if (arch == StringConstants::ppc64Arch() && endianness == QStringLiteral("little"))
        return arch + QStringLiteral("le");

    return arch;
}

QString canonicalArchitecture(const QString &architecture)
{
    QMap<QString, QStringList> archMap;
    archMap.insert(StringConstants::x86Arch(), QStringList()
        << StringConstants::i386Arch()
        << QStringLiteral("i486")
        << StringConstants::i586Arch()
        << QStringLiteral("i686")
        << QStringLiteral("ia32")
        << QStringLiteral("ia-32")
        << QStringLiteral("x86_32")
        << QStringLiteral("x86-32")
        << QStringLiteral("intel32")
        << QStringLiteral("mingw32"));

    archMap.insert(StringConstants::x86_64Arch(), QStringList()
        << QStringLiteral("x86-64")
        << QStringLiteral("x64")
        << StringConstants::amd64Arch()
        << QStringLiteral("ia32e")
        << QStringLiteral("em64t")
        << QStringLiteral("intel64")
        << QStringLiteral("mingw64"));

    archMap.insert(StringConstants::arm64Arch(), QStringList()
        << StringConstants::aarch64Arch());

    archMap.insert(QStringLiteral("ia64"), QStringList()
        << QStringLiteral("ia-64")
        << QStringLiteral("itanium"));

    archMap.insert(StringConstants::ppcArch(), QStringList()
        << StringConstants::powerPcArch());

    archMap.insert(StringConstants::ppc64Arch(), QStringList()
        << QStringLiteral("ppc64le")
        << QStringLiteral("powerpc64")
        << QStringLiteral("powerpc64le"));

    archMap.insert(StringConstants::mipsArch(), QStringList()
        << QStringLiteral("mipseb")
        << QStringLiteral("mipsel"));

    archMap.insert(StringConstants::mips64Arch(), QStringList()
        << QStringLiteral("mips64eb")
        << QStringLiteral("mips64el"));

    QMapIterator<QString, QStringList> i(archMap);
    while (i.hasNext()) {
        i.next();
        if (i.value().contains(architecture.toLower()))
            return i.key();
    }

    return architecture;

}

} // namespace qbs
