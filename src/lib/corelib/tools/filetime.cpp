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

#include "filetime.h"

#include <QtCore/qstring.h>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#else
#include <QtCore/qdatetime.h>
#endif

namespace qbs {
namespace Internal {

#if defined(Q_OS_WIN)

FileTime::FileTime()
{
    static_assert(sizeof(FileTime::InternalType) == sizeof(FILETIME),
                  "FileTime::InternalType has wrong size.");
    m_fileTime = 0;
}

FileTime::FileTime(const FileTime::InternalType &ft) : m_fileTime(ft)
{
}

int FileTime::compare(const FileTime &other) const
{
    auto const t1 = reinterpret_cast<const FILETIME *>(&m_fileTime);
    auto const t2 = reinterpret_cast<const FILETIME *>(&other.m_fileTime);
    return CompareFileTime(t1, t2);
}

void FileTime::clear()
{
    m_fileTime = 0;
}

bool FileTime::isValid() const
{
    return *this != FileTime();
}

FileTime FileTime::currentTime()
{
    FileTime result;
    SYSTEMTIME st;
    GetSystemTime(&st);
    auto const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
}

FileTime FileTime::oldestTime()
{
    SYSTEMTIME st = {
        1601,
        1,
        5,
        2,
        0,
        0,
        0,
        0
    };
    FileTime result;
    auto const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
}

double FileTime::asDouble() const
{
    return static_cast<double>(m_fileTime);
}

QString FileTime::toString() const
{
    auto const ft = reinterpret_cast<const FILETIME *>(&m_fileTime);
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    const QString result = QStringLiteral("%1.%2.%3 %4:%5:%6")
            .arg(stLocal.wDay, 2, 10, QLatin1Char('0')).arg(stLocal.wMonth, 2, 10, QLatin1Char('0')).arg(stLocal.wYear)
            .arg(stLocal.wHour, 2, 10, QLatin1Char('0')).arg(stLocal.wMinute, 2, 10, QLatin1Char('0')).arg(stLocal.wSecond, 2, 10, QLatin1Char('0'));
    return result;
}

#else // defined(Q_OS_WIN)

// based on https://github.com/qt/qtbase/blob/5.13/src/corelib/kernel/qelapsedtimer_unix.cpp
// for details why it is implemented this way, see Qt source code
#  if !defined(CLOCK_REALTIME)
#    define CLOCK_REALTIME 0
static inline void qbs_clock_gettime(int, struct timespec *ts)
{
    // support clock_gettime with gettimeofday
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
}

#    ifdef _POSIX_MONOTONIC_CLOCK
#      undef _POSIX_MONOTONIC_CLOCK
#      define _POSIX_MONOTONIC_CLOCK -1
#    endif
#  else
static inline void qbs_clock_gettime(clockid_t clock, struct timespec *ts)
{
    clock_gettime(clock, ts);
}
#  endif

FileTime::FileTime()
{
    m_fileTime = {0, 0};
}

FileTime::FileTime(const FileTime::InternalType &ft) : m_fileTime(ft)
{
    if (m_fileTime.tv_sec == 0)
        m_fileTime.tv_nsec = 0; // stat() sets only the first member to 0 for non-existing files.
}

int FileTime::compare(const FileTime &other) const
{
    if (m_fileTime.tv_sec < other.m_fileTime.tv_sec)
        return -1;
    if (m_fileTime.tv_sec > other.m_fileTime.tv_sec)
        return 1;
    if (m_fileTime.tv_nsec < other.m_fileTime.tv_nsec)
        return -1;
    if (m_fileTime.tv_nsec > other.m_fileTime.tv_nsec)
        return 1;
    return 0;
}

void FileTime::clear()
{
    m_fileTime = { 0, 0 };
}

bool FileTime::isValid() const
{
    return *this != FileTime();
}

FileTime FileTime::currentTime()
{
    InternalType t;
    qbs_clock_gettime(CLOCK_REALTIME, &t);
    return t;
}

FileTime FileTime::oldestTime()
{
    return FileTime({1, 0});
}

double FileTime::asDouble() const
{
    return static_cast<double>(m_fileTime.tv_sec);
}

QString FileTime::toString() const
{
    QDateTime dt;
    dt.setMSecsSinceEpoch(m_fileTime.tv_sec * 1000 + m_fileTime.tv_nsec / 1000000);
    return dt.toString(Qt::ISODateWithMs);
}

#endif // defined(Q_OS_WIN)

} // namespace Internal
} // namespace qbs
