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

#ifndef QBS_VERSION_H
#define QBS_VERSION_H

#include "qbs_export.h"

#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs {

class Version
{
public:
    constexpr explicit Version(int majorVersion = 0, int minorVersion = 0, int patchLevel = 0,
                     int buildNr = 0)
        : m_major(majorVersion), m_minor(minorVersion), m_patch(patchLevel), m_build(buildNr)
    { }

    constexpr bool isValid() const { return m_major || m_minor || m_patch || m_build; }

    constexpr int majorVersion() const { return m_major; }
    constexpr void setMajorVersion(int majorVersion) { m_major = majorVersion; }

    constexpr int minorVersion() const { return m_minor; }
    constexpr void setMinorVersion(int minorVersion) { m_minor = minorVersion;}

    constexpr int patchLevel() const { return m_patch; }
    constexpr void setPatchLevel(int patchLevel) { m_patch = patchLevel; }

    constexpr int buildNumber() const { return m_build; }
    constexpr void setBuildNumber(int nr) { m_build = nr; }

    static QBS_EXPORT Version fromString(const QString &versionString, bool buildNumberAllowed = false);
    QString QBS_EXPORT toString(const QChar &separator = QLatin1Char('.'),
                                const QChar &buildSeparator = QLatin1Char('-')) const;

private:
    int m_major;
    int m_minor;
    int m_patch;
    int m_build;
};

class VersionRange
{
public:
    constexpr VersionRange() = default;
    constexpr VersionRange(const Version &minVersion, const Version &maxVersion)
        : minimum(minVersion), maximum(maxVersion)
    { }

    Version minimum;
    Version maximum; // exclusive

    VersionRange &narrowDown(const VersionRange &other);
};

constexpr inline int compare(const Version &lhs, const Version &rhs)
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

constexpr inline bool operator==(const Version &lhs, const Version &rhs)
{ return compare(lhs, rhs) == 0; }
constexpr inline bool operator!=(const Version &lhs, const Version &rhs)
{ return !operator==(lhs, rhs); }
constexpr inline bool operator<(const Version &lhs, const Version &rhs)
{ return compare(lhs, rhs) < 0; }
constexpr inline bool operator>(const Version &lhs, const Version &rhs)
{ return compare(lhs, rhs) > 0; }
constexpr inline bool operator<=(const Version &lhs, const Version &rhs)
{ return !operator>(lhs, rhs); }
constexpr inline bool operator>=(const Version &lhs, const Version &rhs)
{ return !operator<(lhs, rhs); }

} // namespace qbs

#endif // QBS_VERSION_H
