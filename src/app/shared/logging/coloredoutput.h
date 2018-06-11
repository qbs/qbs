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
bool terminalSupportsColor();

#endif // QBS_COLOREDOUTPUT_H
