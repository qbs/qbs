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

#ifndef COLOREDOUTPUT_H
#define COLOREDOUTPUT_H

#include <cstdio>

namespace qbs {

// http://en.wikipedia.org/wiki/ANSI_escape_code#Colors
enum TextColor {
    TextColorDefault = -1,
    TextColorBlack = 0,
    TextColorDarkRed = 1,
    TextColorDarkGreen = 2,
    TextColorDarkBlue = 4,
    TextColorDarkCyan = TextColorDarkGreen | TextColorDarkBlue,
    TextColorDarkMagenta = TextColorDarkRed | TextColorDarkBlue,
    TextColorDarkYellow = TextColorDarkRed | TextColorDarkGreen,
    TextColorGray = 7,
    TextColorBright = 8,
    TextColorRed = TextColorDarkRed | TextColorBright,
    TextColorGreen = TextColorDarkGreen | TextColorBright,
    TextColorBlue = TextColorDarkBlue | TextColorBright,
    TextColorCyan = TextColorDarkCyan | TextColorBright,
    TextColorMagenta = TextColorDarkMagenta | TextColorBright,
    TextColorYellow = TextColorDarkYellow | TextColorBright,
    TextColorWhite = TextColorGray | TextColorBright
};

void printfColored(TextColor color, const char *str, va_list vl);
void printfColored(TextColor color, const char *str, ...);
void fprintfColored(TextColor color, FILE *file, const char *str, va_list vl);
void fprintfColored(TextColor color, FILE *file, const char *str, ...);

} // namespace qbs

#endif // COLOREDOUTPUT_H
