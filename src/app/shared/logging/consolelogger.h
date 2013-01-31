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

#ifndef QBS_LOGSINK_H
#define QBS_LOGSINK_H

#include "coloredoutput.h"

#include <logging/logger.h>

namespace qbs { class Settings; }

class ConsoleLogSink : public qbs::ILogSink
{
public:
    ConsoleLogSink();

    void setColoredOutputEnabled(bool enabled) { m_coloredOutputEnabled = enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    void doPrintMessage(qbs::LoggerLevel level, const QString &message, const QString &tag);
    void fprintfWrapper(TextColor color, FILE *file, const char *str, ...);

private:
    bool m_coloredOutputEnabled;
    bool m_enabled;
};


class ConsoleLogger : public qbs::Internal::Logger
{
public:
    static ConsoleLogger &instance(qbs::Settings *settings = 0);
    ConsoleLogSink *logSink() { return &m_logSink; }

private:
    ConsoleLogger(qbs::Settings *settings);

    ConsoleLogSink m_logSink;
};

inline qbs::Internal::LogWriter qbsError() {
    return ConsoleLogger::instance().qbsLog(qbs::LoggerError);
}
inline qbs::Internal::LogWriter qbsWarning() { return ConsoleLogger::instance().qbsWarning(); }
inline qbs::Internal::LogWriter qbsInfo() { return ConsoleLogger::instance().qbsInfo(); }
inline qbs::Internal::LogWriter qbsDebug() { return ConsoleLogger::instance().qbsDebug(); }
inline qbs::Internal::LogWriter qbsTrace() { return ConsoleLogger::instance().qbsTrace(); }

#endif // QBS_LOGSINK_H
