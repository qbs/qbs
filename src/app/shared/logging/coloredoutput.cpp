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

#include "coloredoutput.h"
#include <QtGlobal>
#ifdef Q_OS_WIN32
# include <qt_windows.h>
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
