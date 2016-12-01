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
#include <time.h>
#endif

namespace qbs {
namespace Internal {

#ifdef Q_OS_WIN
template<bool> struct CompileTimeAssert;
template<> struct CompileTimeAssert<true> {};
#endif

FileTime::FileTime()
    : m_fileTime(0)
{
#ifdef Q_OS_WIN
    static CompileTimeAssert<sizeof(FileTime::InternalType) == sizeof(FILETIME)> internal_type_has_wrong_size;
    Q_UNUSED(internal_type_has_wrong_size);
#endif
}

bool FileTime::operator < (const FileTime &rhs) const
{
#ifdef Q_OS_WIN
    const FILETIME *const t1 = reinterpret_cast<const FILETIME *>(&m_fileTime);
    const FILETIME *const t2 = reinterpret_cast<const FILETIME *>(&rhs.m_fileTime);
    return CompareFileTime(t1, t2) < 0;
#else
    return m_fileTime < rhs.m_fileTime;
#endif
}

void FileTime::clear()
{
    m_fileTime = 0;
}

bool FileTime::isValid() const
{
    return m_fileTime != 0;
}

FileTime FileTime::currentTime()
{
#ifdef Q_OS_WIN
    FileTime result;
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME *const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
#else
    return time(0);
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
    FILETIME *const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
#else
    return 1;
#endif
}

QString FileTime::toString() const
{
#ifdef Q_OS_WIN
    const FILETIME *const ft = reinterpret_cast<const FILETIME *>(&m_fileTime);
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    const QString result = QString::fromLatin1("%1.%2.%3 %4:%5:%6")
            .arg(stLocal.wDay, 2, 10, QLatin1Char('0')).arg(stLocal.wMonth, 2, 10, QLatin1Char('0')).arg(stLocal.wYear)
            .arg(stLocal.wHour, 2, 10, QLatin1Char('0')).arg(stLocal.wMinute, 2, 10, QLatin1Char('0')).arg(stLocal.wSecond, 2, 10, QLatin1Char('0'));
    return result;
#else
    QDateTime dt;
    dt.setTime_t(m_fileTime);
    return dt.toString();
#endif
}

} // namespace Internal
} // namespace qbs
