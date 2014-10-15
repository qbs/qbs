/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://www.qt.io/licensing.  For further information
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
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "version.h"

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

QString Version::toString() const
{
    QString s;
    if (m_build)
        s.sprintf("%d.%d.%d-%d", m_major, m_minor, m_patch, m_build);
    else
        s.sprintf("%d.%d.%d", m_major, m_minor, m_patch);
    return s;
}

int compare(const Version &lhs, const Version &rhs)
{
    if (lhs.m_major < rhs.m_major)
        return -1;
    if (lhs.m_major > rhs.m_major)
        return 1;
    if (lhs.m_minor < rhs.m_minor)
        return -1;
    if (lhs.m_minor > rhs.m_minor)
        return 1;
    if (lhs.m_patch < rhs.m_patch)
        return -1;
    if (lhs.m_patch > rhs.m_patch)
        return 1;
    if (lhs.m_build < rhs.m_build)
        return -1;
    if (lhs.m_build > rhs.m_build)
        return 1;
    return 0;
}

} // namespace Internal
} // namespace qbs
