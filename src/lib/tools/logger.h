/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef LOGGER_H
#define LOGGER_H

#include <Qbs/globals.h>

#include <Qbs/ilogsink.h>

#include <QByteArray>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QVariant;
QT_END_NAMESPACE

namespace qbs {

class Logger
{
public:
    static Logger &instance();

    static LoggerLevel defaultLevel() { return LoggerInfo; }

    void setLevel(int level);
    void setLevel(LoggerLevel level);
    LoggerLevel level() const { return m_level; }
    void print(LoggerLevel l, const LogMessage &message);

    ~Logger();

    void setLogSink(ILogSink *logSink);
    void sendProcessOutput(const Qbs::ProcessOutput &processOutput);

protected:
    Logger();

private:
    ILogSink *m_logSink;
    LoggerLevel m_level;
};

class LogWriter
{
public:
    LogWriter(LoggerLevel level);

    // log writer has move semantics and the last instance of
    // a << chain prints the accumulated data
    LogWriter(const LogWriter &other);
    ~LogWriter();
    const LogWriter &operator=(const LogWriter &other);

    void write(const char c);
    void write(const char *str);
    void setOutputChannel(LogOutputChannel channel) { m_logMessage.outputChannel = channel; }
    void setPrintLogLevel(bool b) { m_logMessage.printLogLevel = b; }
    void setTextColor(TextColor color) { m_logMessage.textColor = color; }
    const Qbs::LogMessage &logMessage() const { return m_logMessage; }

private:
    LoggerLevel m_level;
    mutable Qbs::LogMessage m_logMessage;
};

enum LogModifier
{
    DontPrintLogLevel
};

} // namespace qbs

inline bool qbsLogLevel(qbs::LoggerLevel l) { return qbs::Logger::instance().level() >= l; }
void qbsLog(qbs::LoggerLevel logLevel, const char *str, ...);
void qbsError(const char *str, ...);
void qbsWarning(const char *str, ...);
void qbsInfo(const char *str, ...);
void qbsDebug(const char *str, ...);
void qbsTrace(const char *str, ...);

qbs::LogWriter qbsLog(qbs::LoggerLevel level);
qbs::LogWriter qbsError();
qbs::LogWriter qbsWarning();
qbs::LogWriter qbsInfo();
qbs::LogWriter qbsDebug();
qbs::LogWriter qbsTrace();

qbs::LogWriter operator<<(qbs::LogWriter w, const char *str);
qbs::LogWriter operator<<(qbs::LogWriter w, const QByteArray &byteArray);
qbs::LogWriter operator<<(qbs::LogWriter w, const QString &str);
qbs::LogWriter operator<<(qbs::LogWriter w, const QStringList &strList);
qbs::LogWriter operator<<(qbs::LogWriter w, const QSet<QString> &strSet);
qbs::LogWriter operator<<(qbs::LogWriter w, const QVariant &variant);
qbs::LogWriter operator<<(qbs::LogWriter w, int n);
qbs::LogWriter operator<<(qbs::LogWriter w, qint64 n);
qbs::LogWriter operator<<(qbs::LogWriter w, bool b);
qbs::LogWriter operator<<(qbs::LogWriter w, qbs::LogOutputChannel);
qbs::LogWriter operator<<(qbs::LogWriter w, qbs::LogModifier);
qbs::LogWriter operator<<(qbs::LogWriter w, qbs::TextColor);

#endif // LOGGER_H
