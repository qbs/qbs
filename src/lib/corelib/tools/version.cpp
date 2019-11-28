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

#include "version.h"

#include <QtCore/qregexp.h>
#include <QtCore/qstring.h>

namespace qbs {

Version Version::fromString(const QString &versionString, bool buildNumberAllowed)
{
    QString pattern = QStringLiteral("(\\d+)"); // At least one number.
    for (int i = 0; i < 2; ++i)
        pattern += QStringLiteral("(?:\\.(\\d+))?"); // Followed by a dot and a number up to two times.
    if (buildNumberAllowed)
        pattern += QStringLiteral("(?:[-.](\\d+))?"); // And possibly a dash or dot followed by the build number.
    QRegExp rex(pattern);
    if (!rex.exactMatch(versionString))
        return Version{};
    const int majorNr = rex.cap(1).toInt();
    const int minorNr = rex.captureCount() >= 2 ? rex.cap(2).toInt() : 0;
    const int patchNr = rex.captureCount() >= 3 ? rex.cap(3).toInt() : 0;
    const int buildNr = rex.captureCount() >= 4 ? rex.cap(4).toInt() : 0;
    return Version{majorNr, minorNr, patchNr, buildNr};
}

QString Version::toString(const QChar &separator, const QChar &buildSeparator) const
{
    if (m_build) {
        return QStringLiteral("%1%5%2%5%3%6%4")
                .arg(QString::number(m_major), QString::number(m_minor),
                     QString::number(m_patch), QString::number(m_build),
                     separator, buildSeparator);
    }
    return QStringLiteral("%1%4%2%4%3")
            .arg(QString::number(m_major), QString::number(m_minor),
                 QString::number(m_patch), separator);
}

VersionRange &VersionRange::narrowDown(const VersionRange &other)
{
    if (other.minimum > minimum)
        minimum = other.minimum;
    if (other.maximum.isValid() && other.maximum < maximum)
        maximum = other.maximum;
    return *this;
}

} // namespace qbs
