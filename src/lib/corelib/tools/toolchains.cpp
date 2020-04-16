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

#include "toolchains.h"

#include "stringconstants.h"

#include <QtCore/qmap.h>

#include <set>

namespace qbs {

namespace Internal {
static const QString clangToolchain() { return QStringLiteral("clang"); }
static const QString clangClToolchain() { return QStringLiteral("clang-cl"); }
static const QString gccToolchain() { return QStringLiteral("gcc"); }
static const QString llvmToolchain() { return QStringLiteral("llvm"); }
static const QString mingwToolchain() { return QStringLiteral("mingw"); }
static const QString msvcToolchain() { return QStringLiteral("msvc"); }
}

using namespace Internal;

QStringList canonicalToolchain(const QStringList &toolchain)
{
    static const QStringList knownToolchains {
        StringConstants::xcode(),
        clangToolchain(),
        llvmToolchain(),
        mingwToolchain(),
        gccToolchain(),
        clangClToolchain(),
        msvcToolchain()
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
    if (toolchainName == StringConstants::xcode())
        toolchains << canonicalToolchain(clangToolchain());
    else if (toolchainName == clangToolchain())
        toolchains << canonicalToolchain(llvmToolchain());
    else if (toolchainName == llvmToolchain() ||
             toolchainName == mingwToolchain()) {
        toolchains << canonicalToolchain(QStringLiteral("gcc"));
    } else if (toolchainName == clangClToolchain()) {
        toolchains << canonicalToolchain(msvcToolchain());
    }
    return toolchains;
}

} // namespace qbs
