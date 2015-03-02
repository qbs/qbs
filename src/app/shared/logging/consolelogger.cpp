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

#include "consolelogger.h"

#include <tools/preferences.h>
#include <tools/settings.h>

#include <cstdio>

static QHash<QString, TextColor> setupColorTable()
{
    QHash<QString, TextColor> colorTable;
    colorTable[QLatin1String("compiler")] = TextColorDefault;
    colorTable[QLatin1String("linker")] = TextColorDarkGreen;
    colorTable[QLatin1String("codegen")] = TextColorDarkYellow;
    colorTable[QLatin1String("filegen")] = TextColorDarkYellow;
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

    FILE * const file = level == qbs::LoggerInfo ? stdout : stderr;

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
    if (m_coloredOutputEnabled)
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
