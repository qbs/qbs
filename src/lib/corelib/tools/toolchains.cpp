/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "toolchains.h"
#include <QMap>
#include <QSet>

namespace qbs {

QStringList canonicalToolchain(const QStringList &toolchain)
{
    static const QStringList knownToolchains {
        QStringLiteral("xcode"),
        QStringLiteral("clang"),
        QStringLiteral("llvm"),
        QStringLiteral("mingw"),
        QStringLiteral("gcc"),
        QStringLiteral("msvc")
    };

    // Canonicalize each toolchain in the toolchain list,
    // which gets us the aggregate canonicalized (unsorted) list
    QStringList toolchains;
    for (const QString &toolchainName : toolchain)
        toolchains << canonicalToolchain(toolchainName);
    toolchains.removeDuplicates();

    // Find all known toolchains in the canonicalized list,
    // removing them from the main list as we go.
    QStringList usedKnownToolchains;
    for (int i = 0; i < toolchains.size(); ++i) {
        if (knownToolchains.contains(toolchains[i])) {
            usedKnownToolchains << toolchains[i];
            toolchains.removeAt(i--);
        }
    }

    // Sort the list of known toolchains into their canonical order.
    std::sort(usedKnownToolchains.begin(), usedKnownToolchains.end(), [](
              const QString &a,
              const QString &b) {
        return knownToolchains.indexOf(a) < knownToolchains.indexOf(b);
    });

    // Re-add the known toolchains to the main list (the custom ones go first).
    toolchains << usedKnownToolchains;

    // The toolchain list still needs further validation as it may contain mututally exclusive
    // toolchain types (for example, llvm and msvc).
    return toolchains;
}

QStringList canonicalToolchain(const QString &name)
{
    const QString &toolchainName = name.toLower();
    QStringList toolchains(toolchainName);
    if (toolchainName == QLatin1String("xcode"))
        toolchains << canonicalToolchain(QLatin1String("clang"));
    else if (toolchainName == QLatin1String("clang"))
        toolchains << canonicalToolchain(QLatin1String("llvm"));
    else if (toolchainName == QLatin1String("llvm") ||
             toolchainName == QLatin1String("mingw")) {
        toolchains << canonicalToolchain(QLatin1String("gcc"));
    }
    return toolchains;
}

} // namespace qbs
