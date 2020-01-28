/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
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

#include "visualstudioversioninfo.h"
#include <tools/qbsassert.h>
#include <QtCore/qdebug.h>
#include <QtCore/qglobal.h>

namespace qbs {
namespace Internal {

VisualStudioVersionInfo::VisualStudioVersionInfo() = default;

VisualStudioVersionInfo::VisualStudioVersionInfo(const Version &version)
    : m_version(version)
{
    QBS_CHECK(version.minorVersion() == 0 || version == Version(7, 1)
              || version.majorVersion() >= 15);
}

std::set<VisualStudioVersionInfo> VisualStudioVersionInfo::knownVersions()
{
    static const std::set<VisualStudioVersionInfo> known = {
        Version(16), Version(15), Version(14), Version(12), Version(11), Version(10), Version(9),
        Version(8), Version(7, 1), Version(7), Version(6)
    };
    return known;
}

bool VisualStudioVersionInfo::operator<(const VisualStudioVersionInfo &other) const
{
    return m_version < other.m_version;
}

bool VisualStudioVersionInfo::operator==(const VisualStudioVersionInfo &other) const
{
    return m_version == other.m_version;
}

bool VisualStudioVersionInfo::usesMsBuild() const
{
    return m_version.majorVersion() >= 10;
}

bool VisualStudioVersionInfo::usesVcBuild() const
{
    return m_version.majorVersion() <= 9;
}

bool VisualStudioVersionInfo::usesSolutions() const
{
    return m_version.majorVersion() >= 7;
}

Version VisualStudioVersionInfo::version() const
{
    return m_version;
}

int VisualStudioVersionInfo::marketingVersion() const
{
    switch (m_version.majorVersion()) {
    case 6:
        return 6;
    case 7:
        switch (m_version.minorVersion()) {
        case 0:
            return 2002;
        case 1:
            return 2003;
        default:
            Q_UNREACHABLE();
        }
        break;
    case 8:
        return 2005;
    case 9:
        return 2008;
    case 10:
        return 2010;
    case 11:
        return 2012;
    case 12:
        return 2013;
    case 14:
        return 2015;
    case 15:
        return 2017;
    case 16:
        return 2019;
    default:
        qWarning() << QStringLiteral("unrecognized Visual Studio version: ")
                   << m_version.toString();
        return 0;
    }
}

QString VisualStudioVersionInfo::solutionVersion() const
{
    // Visual Studio 2012 finally stabilized the solution version
    if (m_version >= Version(11))
        return QStringLiteral("12.00");

    if (m_version >= Version(8))
        return QStringLiteral("%1.00").arg(m_version.majorVersion() + 1);

    if (m_version >= Version(7, 1))
        return QStringLiteral("8.00");

    if (m_version >= Version(7))
        return QStringLiteral("7.00");

    // these versions do not use solution files
    // Visual Studio 6 uses .dsw files which are format version 6.00 but these are different
    Q_ASSERT(!usesSolutions());
    Q_UNREACHABLE();
}

QString VisualStudioVersionInfo::toolsVersion() const
{
    // "https://msdn.microsoft.com/en-us/library/bb383796.aspx"
    // Starting in Visual Studio 2013, the MSBuild Toolset version is the same as the Visual Studio
    // version number"... again
    if (m_version >= Version(12))
        return QStringLiteral("%1.0").arg(m_version.majorVersion());

    if (m_version >= Version(10))
        return QStringLiteral("4.0");

    // pre-MSBuild
    return QStringLiteral("%1,00").arg(m_version.majorVersion());
}

QString VisualStudioVersionInfo::platformToolsetVersion() const
{
    static std::pair<int, QString> table[] = {
        {16, QStringLiteral("v142")},             // VS 2019
        {15, QStringLiteral("v141")}              // VS 2017
    };
    for (const auto &p : table) {
        if (p.first == m_version.majorVersion())
            return p.second;
    }
    return QStringLiteral("v%1").arg(m_version.majorVersion() * 10);
}

quint32 qHash(const VisualStudioVersionInfo &info)
{
    return qHash(info.version().toString());
}

} // namespace Internal
} // namespace qbs
