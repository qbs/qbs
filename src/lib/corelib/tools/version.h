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
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef QBS_VERSION_H
#define QBS_VERSION_H

#include "qbs_export.h"
#include <QString>

namespace qbs {
namespace Internal {

class QBS_EXPORT Version
{
    friend int compare(const Version &lhs, const Version &rhs);
public:
    explicit Version(int majorVersion = 0, int minorVersion = 0, int patchLevel = 0,
            int buildNr = 0);

    bool isValid() const { return m_major || m_minor || m_patch || m_build; }

    int &majorVersionRef() { return m_major; }
    int majorVersion() const;
    void setMajorVersion(int majorVersion);

    int &minorVersionRef() { return m_minor; }
    int minorVersion() const;
    void setMinorVersion(int minorVersion);

    int &patchLevelRef() { return m_patch; }
    int patchLevel() const;
    void setPatchLevel(int patchLevel);

    int &buildNumberRef() { return m_build; }
    int buildNumber() const;
    void setBuildNumber(int nr);

    QString toString() const;

private:
    int m_major;
    int m_minor;
    int m_patch;
    int m_build;
};

int compare(const Version &lhs, const Version &rhs);
inline bool operator==(const Version &lhs, const Version &rhs) { return compare(lhs, rhs) == 0; }
inline bool operator!=(const Version &lhs, const Version &rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const Version &lhs, const Version &rhs) { return compare(lhs, rhs) < 0; }
inline bool operator>(const Version &lhs, const Version &rhs) { return compare(lhs, rhs) > 0; }
inline bool operator<=(const Version &lhs, const Version &rhs) { return !operator>(lhs, rhs); }
inline bool operator>=(const Version &lhs, const Version &rhs) { return !operator<(lhs, rhs); }

} // namespace Internal
} // namespace qbs

#endif // QBS_VERSION_H
