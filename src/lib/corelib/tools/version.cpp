/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "version.h"

#include <QRegExp>
#include <QString>

namespace qbs {
namespace Internal {

Version::Version(int major, int minor, int patch, int buildNr)
    : m_major(major), m_minor(minor), m_patch(patch), m_build(buildNr)
{
}

int Version::majorVersion() const
{
    return m_major;
}

void Version::setMajorVersion(int major)
{
    m_major = major;
}

int Version::minorVersion() const
{
    return m_minor;
}

void Version::setMinorVersion(int minor)
{
    m_minor = minor;
}
int Version::patchLevel() const
{
    return m_patch;
}

void Version::setPatchLevel(int patch)
{
    m_patch = patch;
}

int Version::buildNumber() const
{
    return m_build;
}

void Version::setBuildNumber(int nr)
{
    m_build = nr;
}

Version Version::fromString(const QString &versionString, bool buildNumberAllowed)
{
    QString pattern = QLatin1String("(\\d+)"); // At least one number.
    for (int i = 0; i < 2; ++i)
        pattern += QLatin1String("(?:\\.(\\d+))?"); // Followed by a dot and a number up to two times.
    if (buildNumberAllowed)
        pattern += QLatin1String("(?:-(\\d+))?"); // And possibly a dash followed by the build number.
    QRegExp rex(pattern);
    if (!rex.exactMatch(versionString))
        return Version();
    const int majorNr = rex.cap(1).toInt();
    const int minorNr = rex.captureCount() >= 2 ? rex.cap(2).toInt() : 0;
    const int patchNr = rex.captureCount() >= 3 ? rex.cap(3).toInt() : 0;
    const int buildNr = rex.captureCount() >= 4 ? rex.cap(4).toInt() : 0;
    return Version(majorNr, minorNr, patchNr, buildNr);
}

QString Version::toString() const
{
    QString s;
    if (m_build)
        s.sprintf("%d.%d.%d-%d", m_major, m_minor, m_patch, m_build);
    else
        s.sprintf("%d.%d.%d", m_major, m_minor, m_patch);
    return s;
}

const Version &Version::qbsVersion()
{
    static const Version v = fromString(QLatin1String(QBS_VERSION));
    return v;
}

int compare(const Version &lhs, const Version &rhs)
{
    if (lhs.majorVersion() < rhs.majorVersion())
        return -1;
    if (lhs.majorVersion() > rhs.majorVersion())
        return 1;
    if (lhs.minorVersion() < rhs.minorVersion())
        return -1;
    if (lhs.minorVersion() > rhs.minorVersion())
        return 1;
    if (lhs.patchLevel() < rhs.patchLevel())
        return -1;
    if (lhs.patchLevel() > rhs.patchLevel())
        return 1;
    if (lhs.buildNumber() < rhs.buildNumber())
        return -1;
    if (lhs.buildNumber() > rhs.buildNumber())
        return 1;
    return 0;
}

} // namespace Internal
} // namespace qbs
