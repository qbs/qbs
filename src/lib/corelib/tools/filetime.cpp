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

#ifdef APPLE_CUSTOM_CLOCK_GETTIME
#include <sys/time.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

// clk_id isn't used, only the CLOCK_REALTIME case is implemented.
int clock_gettime(int /*clk_id*/, struct timespec *t)
{
    struct timeval tv;
    // Resolution of gettimeofday is 1000nsecs = 1 microsecond.
    int ret = gettimeofday(&tv, NULL);
    t->tv_sec  = tv.tv_sec;
    t->tv_nsec = tv.tv_usec * 1000;
    return ret;
}
#endif

FileTime::FileTime()
{
#ifdef Q_OS_WIN
    static_assert(sizeof(FileTime::InternalType) == sizeof(FILETIME),
                  "FileTime::InternalType has wrong size.");
    m_fileTime = 0;
#elif HAS_CLOCK_GETTIME
    m_fileTime = {0, 0};
#else
    m_fileTime = 0;
#endif
}

FileTime::FileTime(const FileTime::InternalType &ft) : m_fileTime(ft)
{
#if HAS_CLOCK_GETTIME
    if (m_fileTime.tv_sec == 0)
        m_fileTime.tv_nsec = 0; // stat() sets only the first member to 0 for non-existing files.
#endif
}

int FileTime::compare(const FileTime &other) const
{
#ifdef Q_OS_WIN
    auto const t1 = reinterpret_cast<const FILETIME *>(&m_fileTime);
    auto const t2 = reinterpret_cast<const FILETIME *>(&other.m_fileTime);
    return CompareFileTime(t1, t2);
#elif HAS_CLOCK_GETTIME
    if (m_fileTime.tv_sec < other.m_fileTime.tv_sec)
        return -1;
    if (m_fileTime.tv_sec > other.m_fileTime.tv_sec)
        return 1;
    if (m_fileTime.tv_nsec < other.m_fileTime.tv_nsec)
        return -1;
    if (m_fileTime.tv_nsec > other.m_fileTime.tv_nsec)
        return 1;
    return 0;
#else
    if (m_fileTime < other.m_fileTime)
        return -1;
    if (m_fileTime > other.m_fileTime)
        return 1;
    return 0;
#endif
}

void FileTime::clear()
{
#if HAS_CLOCK_GETTIME
    m_fileTime = { 0, 0 };
#else
    m_fileTime = 0;
#endif
}

bool FileTime::isValid() const
{
    return *this != FileTime();
}

FileTime FileTime::currentTime()
{
#ifdef Q_OS_WIN
    FileTime result;
    SYSTEMTIME st;
    GetSystemTime(&st);
    auto const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
#elif defined APPLE_CUSTOM_CLOCK_GETTIME
    InternalType t;
    // Explicitly use our custom version, so that we don't get an additional unresolved symbol on a
    // system that actually provides one, but isn't used due to the minimium deployment target
    // being lower.
    qbs::Internal::clock_gettime(CLOCK_REALTIME, &t);
    return t;
#elif HAS_CLOCK_GETTIME
    InternalType t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t;
#else
    return time(nullptr);
#endif
}

FileTime FileTime::oldestTime()
{
#ifdef Q_OS_WIN
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
#elif HAS_CLOCK_GETTIME
    return FileTime({1, 0});
#else
    return 1;
#endif
}

double FileTime::asDouble() const
{
#if HAS_CLOCK_GETTIME
    return static_cast<double>(m_fileTime.tv_sec);
#else
    return static_cast<double>(m_fileTime);
#endif
}

QString FileTime::toString() const
{
#ifdef Q_OS_WIN
    auto const ft = reinterpret_cast<const FILETIME *>(&m_fileTime);
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    const QString result = QStringLiteral("%1.%2.%3 %4:%5:%6")
            .arg(stLocal.wDay, 2, 10, QLatin1Char('0')).arg(stLocal.wMonth, 2, 10, QLatin1Char('0')).arg(stLocal.wYear)
            .arg(stLocal.wHour, 2, 10, QLatin1Char('0')).arg(stLocal.wMinute, 2, 10, QLatin1Char('0')).arg(stLocal.wSecond, 2, 10, QLatin1Char('0'));
    return result;
#else
    QDateTime dt;
#if HAS_CLOCK_GETTIME
   dt.setMSecsSinceEpoch(m_fileTime.tv_sec * 1000 + m_fileTime.tv_nsec / 1000000);
#else
    dt.setTime_t(m_fileTime);
#endif
    return dt.toString(Qt::ISODateWithMs);
#endif
}

} // namespace Internal
} // namespace qbs
