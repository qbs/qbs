/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef FILETIME_H
#define FILETIME_H

#include <QtCore/QDataStream>
#include <QtCore/QDebug>

#if defined(Q_OS_UNIX)
#include <time.h>
#endif

namespace qbs {

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

    void clear();
    bool isValid() const;
    QString toString() const;

    static FileTime currentTime();

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

} // namespace qbs

QT_BEGIN_NAMESPACE

inline QDataStream& operator>>(QDataStream &stream, qbs::FileTime &ft)
{
    quint64 u;
    stream >> u;
    ft.m_fileTime = u;
    return stream;
}

inline QDataStream& operator<<(QDataStream &stream, const qbs::FileTime &ft)
{
    stream << (quint64)ft.m_fileTime;
    return stream;
}

inline QDebug operator<<(QDebug dbg, const qbs::FileTime &t)
{
    dbg.nospace() << t.toString();
    return dbg.space();
}

QT_END_NAMESPACE

#endif // FILETIME_H
