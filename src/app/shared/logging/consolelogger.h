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
    void doPrintMessage(qbs::LoggerLevel level, const QString &message, const QString &tag) override;
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
    void setSettings(qbs::Settings *settings);

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
