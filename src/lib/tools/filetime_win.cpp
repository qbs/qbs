/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "filetime.h"

#include <QString>
#include <qt_windows.h>
#ifdef Q_CC_MSVC
#include <strsafe.h>
#endif // Q_CC_MSVC

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

QString FileTime::toString() const
{
    const FILETIME *const ft = reinterpret_cast<const FILETIME *>(&m_fileTime);
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
#ifdef Q_CC_MSVC
    WCHAR szString[512];
    HRESULT hr = StringCchPrintf(szString, sizeof(szString) / sizeof(WCHAR),
                                 L"%02d.%02d.%d %02d:%02d:%02d:%02d",
                                 stLocal.wDay, stLocal.wMonth, stLocal.wYear,
                                 stLocal.wHour, stLocal.wMinute, stLocal.wSecond,
                                 stLocal.wMilliseconds);
    return SUCCEEDED(hr) ? QString::fromWCharArray(szString) : QString();
#else // Q_CC_MSVC
    const QString result = QString("%1.%2.%3 %4:%5:%6")
            .arg(stLocal.wDay, 2, 10, QLatin1Char('0')).arg(stLocal.wMonth, 2, 10, QLatin1Char('0')).arg(stLocal.wYear)
            .arg(stLocal.wHour, 2, 10, QLatin1Char('0')).arg(stLocal.wMinute, 2, 10, QLatin1Char('0')).arg(stLocal.wSecond, 2, 10, QLatin1Char('0'));
    return result;
#endif // Q_CC_MSVC
}

} // namespace Internal
} // namespace qbs
