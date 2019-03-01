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

#include "consolelogger.h"

#include <tools/preferences.h>
#include <tools/settings.h>

#include <cstdio>

static QHash<QString, TextColor> setupColorTable()
{
    QHash<QString, TextColor> colorTable;
    colorTable[QStringLiteral("compiler")] = TextColorDefault;
    colorTable[QStringLiteral("linker")] = TextColorDarkGreen;
    colorTable[QStringLiteral("codegen")] = TextColorDarkYellow;
    colorTable[QStringLiteral("filegen")] = TextColorDarkYellow;
    return colorTable;
}

ConsoleLogSink::ConsoleLogSink() : m_coloredOutputEnabled(true), m_enabled(true)
{
}

void ConsoleLogSink::doPrintMessage(qbs::LoggerLevel level, const QString &message,
                                    const QString &tag)
{
    if (!m_enabled)
        return;

    FILE * const file = level == qbs::LoggerInfo && tag != QStringLiteral("stdErr")
            ? stdout : stderr;

    const QString levelTag = logLevelTag(level);
    TextColor color = TextColorDefault;
    switch (level) {
    case qbs::LoggerError:
        color = TextColorRed;
        break;
    case qbs::LoggerWarning:
        color = TextColorYellow;
        break;
    default:
        break;
    }

    fprintfWrapper(color, file, levelTag.toLocal8Bit().constData());
    static QHash<QString, TextColor> colorTable = setupColorTable();
    fprintfWrapper(colorTable.value(tag, TextColorDefault), file, "%s\n",
                   message.toLocal8Bit().constData());
    fflush(file);
}

void ConsoleLogSink::fprintfWrapper(TextColor color, FILE *file, const char *str, ...)
{
    va_list vl;
    va_start(vl, str);
    if (m_coloredOutputEnabled && terminalSupportsColor())
        fprintfColored(color, file, str, vl);
    else
        vfprintf(file, str, vl);
    va_end(vl);
}


ConsoleLogger &ConsoleLogger::instance(qbs::Settings *settings)
{
    static ConsoleLogger logger(settings);
    return logger;
}

void ConsoleLogger::setSettings(qbs::Settings *settings)
{
    if (settings)
        m_logSink.setColoredOutputEnabled(qbs::Preferences(settings).useColoredOutput());
}

ConsoleLogger::ConsoleLogger(qbs::Settings *settings) : Logger(&m_logSink)
{
    setSettings(settings);
}
