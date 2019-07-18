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

#include "keiluvversioninfo.h"

#include "archs/mcs51/mcs51utils.h"

#include <tools/qbsassert.h>

#include <QtCore/qdebug.h>
#include <QtCore/qglobal.h>

namespace qbs {

KeiluvVersionInfo::KeiluvVersionInfo(
        const Version &version,
        const std::set<KeiluvUtils::Architecture> &archs)
    : m_version(version), m_archs(archs)
{
}

std::set<KeiluvVersionInfo> KeiluvVersionInfo::knownVersions()
{
    static const std::set<KeiluvVersionInfo> known = {
        {Version(keiluv::mcs51::v5::KeiluvConstants::kUVisionVersion),
         {KeiluvUtils::Architecture::Mcs51Architecture}},
    };
    return known;
}

bool KeiluvVersionInfo::operator<(const KeiluvVersionInfo &other) const
{
    return m_version < other.m_version;
}

bool KeiluvVersionInfo::operator==(const KeiluvVersionInfo &other) const
{
    return m_version == other.m_version
            && m_archs == other.m_archs;
}

Version KeiluvVersionInfo::version() const
{
    return m_version;
}

int KeiluvVersionInfo::marketingVersion() const
{
    const auto mv = m_version.majorVersion();
    for (const KeiluvVersionInfo &known : knownVersions()) {
        if (known.version().majorVersion() == mv)
            return mv;
    }
    qWarning() << QStringLiteral("Unrecognized KEIL UV version: ")
               << m_version.toString();
    return 0;
}

bool KeiluvVersionInfo::containsArchitecture(KeiluvUtils::Architecture arch) const
{
    return m_archs.find(arch) != m_archs.cend();
}

quint32 qHash(const KeiluvVersionInfo &info)
{
    return qHash(info.version().toString());
}

} // namespace qbs
