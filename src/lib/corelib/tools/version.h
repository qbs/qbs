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

#ifndef QBS_VERSION_H
#define QBS_VERSION_H

#include "qbs_export.h"

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

class QBS_EXPORT Version
{
public:
    explicit Version(int majorVersion = 0, int minorVersion = 0, int patchLevel = 0,
            int buildNr = 0);

    bool isValid() const { return m_major || m_minor || m_patch || m_build; }

    int majorVersion() const;
    void setMajorVersion(int majorVersion);

    int minorVersion() const;
    void setMinorVersion(int minorVersion);

    int patchLevel() const;
    void setPatchLevel(int patchLevel);

    int buildNumber() const;
    void setBuildNumber(int nr);

    static Version fromString(const QString &versionString, bool buildNumberAllowed = false);
    QString toString() const;

    static const Version &qbsVersion();

private:
    int m_major;
    int m_minor;
    int m_patch;
    int m_build;
};

QBS_EXPORT int compare(const Version &lhs, const Version &rhs);
inline bool operator==(const Version &lhs, const Version &rhs) { return compare(lhs, rhs) == 0; }
inline bool operator!=(const Version &lhs, const Version &rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const Version &lhs, const Version &rhs) { return compare(lhs, rhs) < 0; }
inline bool operator>(const Version &lhs, const Version &rhs) { return compare(lhs, rhs) > 0; }
inline bool operator<=(const Version &lhs, const Version &rhs) { return !operator>(lhs, rhs); }
inline bool operator>=(const Version &lhs, const Version &rhs) { return !operator<(lhs, rhs); }

} // namespace Internal
} // namespace qbs

#endif // QBS_VERSION_H
