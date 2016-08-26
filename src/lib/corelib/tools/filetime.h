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

#ifndef QBS_FILETIME_H
#define QBS_FILETIME_H

#include <QDataStream>
#include <QDebug>

#if defined(Q_OS_UNIX)
#include <time.h>
#endif

namespace qbs {
namespace Internal {

class FileTime
{
public:
#if defined(Q_OS_UNIX)
    typedef time_t InternalType;
#elif defined(Q_OS_WIN)
    typedef quint64 InternalType;
#else
#   error unknown platform
#endif

    FileTime();
    FileTime(const InternalType &ft)
        : m_fileTime(ft)
    { }

    bool operator < (const FileTime &rhs) const;
    bool operator > (const FileTime &rhs) const;
    bool operator <= (const FileTime &rhs) const;
    bool operator >= (const FileTime &rhs) const;
    bool operator == (const FileTime &rhs) const;
    bool operator != (const FileTime &rhs) const;

    void clear();
    bool isValid() const;
    QString toString() const;

    static FileTime currentTime();
    static FileTime oldestTime();

    friend class FileInfo;
    InternalType m_fileTime;
};

inline bool FileTime::operator > (const FileTime &rhs) const
{
    return rhs < *this;
}

inline bool FileTime::operator <= (const FileTime &rhs) const
{
    return operator < (rhs) || operator == (rhs);
}

inline bool FileTime::operator >= (const FileTime &rhs) const
{
    return operator > (rhs) || operator == (rhs);
}

inline bool FileTime::operator == (const FileTime &rhs) const
{
    return m_fileTime == rhs.m_fileTime;
}

inline bool FileTime::operator != (const FileTime &rhs) const
{
    return !operator==(rhs);
}

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE

inline QDataStream& operator>>(QDataStream &stream, qbs::Internal::FileTime &ft)
{
    quint64 u;
    stream >> u;
    ft.m_fileTime = u;
    return stream;
}

inline QDataStream& operator<<(QDataStream &stream, const qbs::Internal::FileTime &ft)
{
    stream << (quint64)ft.m_fileTime;
    return stream;
}

inline QDebug operator<<(QDebug dbg, const qbs::Internal::FileTime &t)
{
    dbg.nospace() << t.toString();
    return dbg.space();
}

QT_END_NAMESPACE

#endif // QBS_FILETIME_H
