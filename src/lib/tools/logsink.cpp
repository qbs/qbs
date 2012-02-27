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

#include "logsink.h"
#include "logger.h"
#include "coloredoutput.h"

#include <qhash.h>
#include <stdio.h>

namespace qbs {

void ConsolePrintLogSink::outputLogMessage(LoggerLevel level, const LogMessage &message)
{
    FILE *file = stdout;
    if (message.outputChannel == LogOutputStdErr)
        file = stderr;

    if (message.printLogLevel) {
        switch (level) {
        case LoggerFatal:
            fprintfWrapper(TextColorRed, file, "FATAL ERROR: ");
            break;
        case LoggerError:
            fprintfWrapper(TextColorRed, file, "ERROR: ");
            break;
        case LoggerWarning:
            fprintfWrapper(TextColorYellow, file, "WARNING: ");
            break;
        case LoggerInfo:
            fprintf(file, "INFO: ");
            break;
        case LoggerDebug:
            fprintf(file, "DEBUG: ");
            break;
        case LoggerTrace:
            fprintf(file, "TRACE: ");
            break;
        }
    }
    fprintfWrapper(message.textColor, file, "%s\n", message.data.data());
}

void ConsolePrintLogSink::fprintfWrapper(TextColor color, FILE *file, const char *str, ...)
{
    va_list vl;
    va_start(vl, str);
    if (m_coloredOutputEnabled)
        fprintfColored(color, file, str, vl);
    else
        vfprintf(file, str, vl);
    va_end(vl);
}

} // namespace qbs
