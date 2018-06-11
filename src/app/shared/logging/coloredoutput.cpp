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

#include "coloredoutput.h"
#include <QtCore/qbytearray.h>
#ifdef Q_OS_WIN32
# include <QtCore/qt_windows.h>
#endif

#include <cstdarg>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

void printfColored(TextColor color, const char *str, va_list vl)
{
    fprintfColored(color, stdout, str, vl);
}

void printfColored(TextColor color, const char *str, ...)
{
    va_list vl;
    va_start(vl, str);
    printfColored(color, str, vl);
    va_end(vl);
}

void fprintfColored(TextColor color, FILE *file, const char *str, va_list vl)
{
#if defined(Q_OS_UNIX)
    if (color != TextColorDefault && isatty(fileno(file))) {
        unsigned char bright = (color & TextColorBright) >> 3;
        fprintf(file, "\033[%d;%dm", bright, 30 + (color & ~TextColorBright));
        vfprintf(file, str, vl);
        fprintf(stdout, "\033[0m");
        fprintf(stderr, "\033[0m");
    } else
#elif defined(Q_OS_WIN32)
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    if (color != TextColorDefault
        && hStdout != INVALID_HANDLE_VALUE
        && GetConsoleScreenBufferInfo(hStdout, &csbiInfo))
    {
        WORD bgrColor = ((color & 1) << 2) | (color & 2) | ((color & 4) >> 2);    // BGR instead of RGB.
        if (color & TextColorBright)
            bgrColor += FOREGROUND_INTENSITY;
        SetConsoleTextAttribute(hStdout, (csbiInfo.wAttributes & 0xf0) | bgrColor);
        vfprintf(file, str, vl);
        SetConsoleTextAttribute(hStdout, csbiInfo.wAttributes);
    } else
#endif
    {
        vfprintf(file, str, vl);
    }
}

void fprintfColored(TextColor color, FILE *file, const char *str, ...)
{
    va_list vl;
    va_start(vl, str);
    fprintfColored(color, file, str, vl);
    va_end(vl);
}

bool terminalSupportsColor()
{
#if defined(Q_OS_UNIX)
    const QByteArray &term = qgetenv("TERM");
    return !term.isEmpty() && term != "dumb";
#else
    return true;
#endif
}
