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

#include "coloredoutput.h"
#include <qglobal.h>
#ifdef Q_OS_WIN32
# include <qt_windows.h>
#endif

#include <cstdarg>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qbs {

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
    if (color != TextColorDefault && isatty(fileno(stdout))) {
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

} // namespace qbs
