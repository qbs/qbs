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

#include "filetime.h"

#include <QString>
#include <qt_windows.h>

namespace qbs {
namespace Internal {

template<bool> struct CompileTimeAssert;
template<> struct CompileTimeAssert<true> {};

FileTime::FileTime()
    : m_fileTime(0)
{
    static CompileTimeAssert<sizeof(FileTime::InternalType) == sizeof(FILETIME)> internal_type_has_wrong_size;
    Q_UNUSED(internal_type_has_wrong_size);
}

bool FileTime::operator < (const FileTime &rhs) const
{
    const FILETIME *const t1 = reinterpret_cast<const FILETIME *>(&m_fileTime);
    const FILETIME *const t2 = reinterpret_cast<const FILETIME *>(&rhs.m_fileTime);
    return CompareFileTime(t1, t2) < 0;
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
    FileTime result;
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME *const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
}

FileTime FileTime::oldestTime()
{
    SYSTEMTIME st = {
        1601,
        1,
        5,
        2
    };
    FileTime result;
    FILETIME *const ft = reinterpret_cast<FILETIME *>(&result.m_fileTime);
    SystemTimeToFileTime(&st, ft);
    return result;
}

QString FileTime::toString() const
{
    const FILETIME *const ft = reinterpret_cast<const FILETIME *>(&m_fileTime);
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    const QString result = QString::fromLatin1("%1.%2.%3 %4:%5:%6")
            .arg(stLocal.wDay, 2, 10, QLatin1Char('0')).arg(stLocal.wMonth, 2, 10, QLatin1Char('0')).arg(stLocal.wYear)
            .arg(stLocal.wHour, 2, 10, QLatin1Char('0')).arg(stLocal.wMinute, 2, 10, QLatin1Char('0')).arg(stLocal.wSecond, 2, 10, QLatin1Char('0'));
    return result;
}

} // namespace Internal
} // namespace qbs
