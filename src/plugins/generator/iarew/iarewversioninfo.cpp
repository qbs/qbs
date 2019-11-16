/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "iarewversioninfo.h"

#include <QtCore/qdebug.h>

namespace qbs {

IarewVersionInfo::IarewVersionInfo(
        const Version &version,
        const std::set<gen::utils::Architecture> &archs)
    : gen::VersionInfo(version, archs)
{
}

std::set<IarewVersionInfo> IarewVersionInfo::knownVersions()
{
    static const std::set<IarewVersionInfo> known = {
        {Version(8), {gen::utils::Architecture::Arm}},
        {Version(7), {gen::utils::Architecture::Avr,
                      gen::utils::Architecture::Msp430}},
        {Version(10), {gen::utils::Architecture::Mcs51}},
        {Version(3), {gen::utils::Architecture::Stm8}},
    };
    return known;
}

int IarewVersionInfo::marketingVersion() const
{
    const auto mv = gen::VersionInfo::marketingVersion();
    for (const IarewVersionInfo &known : knownVersions()) {
        if (known.version().majorVersion() == mv)
            return mv;
    }
    qWarning() << QStringLiteral("Unrecognized IAR EW version: ")
               << version().toString();
    return 0;
}

} // namespace qbs
