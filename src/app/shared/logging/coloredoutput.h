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

#ifndef QBS_COLOREDOUTPUT_H
#define QBS_COLOREDOUTPUT_H

#include <cstdio>
#include <cstdarg>

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

#endif // QBS_COLOREDOUTPUT_H
