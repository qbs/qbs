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

#ifndef GENERATORS_VERSION_INFO_H
#define GENERATORS_VERSION_INFO_H

#include "generatorutils.h"

#include <tools/qbs_export.h>
#include <tools/version.h>

#include <QFlags>

namespace qbs {
namespace gen {

class QBS_EXPORT VersionInfo
{
public:
    constexpr VersionInfo(const Version &version, utils::ArchitectureFlags archs)
        : m_version(version), m_archs(archs)
    {
    }

    constexpr bool operator<(const VersionInfo &other) const { return m_version < other.m_version; }
    constexpr bool operator==(const VersionInfo &other) const
    {
        return m_version == other.m_version && m_archs == other.m_archs;
    }

    constexpr Version version() const { return m_version; }
    constexpr bool containsArchitecture(utils::Architecture arch) const { return m_archs & arch; }

    int marketingVersion() const;

private:
    Version m_version;
    utils::ArchitectureFlags m_archs;
};

inline quint32 qHash(const VersionInfo &info)
{
    return qHash(info.version().toString());
}

} // namespace gen
} // namespace qbs

#endif // GENERATORS_VERSION_INFO_H
