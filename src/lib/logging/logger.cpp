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

#if defined(_MSC_VER) && _MSC_VER > 0
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "logger.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QSet>
#include <QVariant>

#include <cstdarg>
#include <stdio.h>

namespace qbs {

Logger &Logger::instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_logSink(0)
    , m_level(defaultLevel())
{
}

Logger::~Logger()
{
    delete m_logSink;
}

void Logger::setLogSink(ILogSink *logSink)
{
    m_logSink = logSink;
}

void Logger::setLevel(int level)
{
    m_level = static_cast<LoggerLevel>(qMin(level, int(LoggerMaxLevel)));
}

void Logger::setLevel(LoggerLevel level)
{
    m_level = level;
}

void Logger::print(LoggerLevel l, const LogMessage &message)
{
    if (!m_logSink)
        return;
    m_logSink->outputLogMessage(l, message);
}

LogWriter::LogWriter(LoggerLevel level)
    : m_level(level)
{}

LogWriter::LogWriter(const LogWriter &other)
    : m_level(other.m_level)
    , m_logMessage(other.m_logMessage)
{
    other.m_logMessage.data.clear();
}

LogWriter::~LogWriter()
{
    if (!m_logMessage.data.isEmpty())
        Logger::instance().print(m_level, m_logMessage);
}

const LogWriter &LogWriter::operator=(const LogWriter &other)
{
    m_level = other.m_level;
    m_logMessage = other.m_logMessage;
    other.m_logMessage.data.clear();

    return *this;
}

void LogWriter::write(const char c)
{
    if (Logger::instance().level() >= m_level)
        m_logMessage.data.append(c);
}

void LogWriter::write(const char *str)
{
    if (Logger::instance().level() >= m_level)
        m_logMessage.data.append(str);
}

Q_GLOBAL_STATIC(QMutex, logMutex)
Q_GLOBAL_STATIC(QByteArray, logByteArray)

static void qbsLog_impl(LoggerLevel logLevel, const char *str, va_list vl)
{
    Logger &logger = Logger::instance();
    if (logger.level() < logLevel)
        return;
    logMutex()->lock();
    if (logByteArray()->isEmpty())
        logByteArray()->resize(1024 * 1024);
    vsnprintf(logByteArray()->data(), logByteArray()->size(), str, vl);
    LogMessage msg;
    msg.data = *logByteArray();
    logMutex()->unlock();
    logger.print(logLevel, msg);
}

void qbsLog(LoggerLevel logLevel, const char *str, ...)
{
    va_list vl;
    va_start(vl, str);
    qbsLog_impl(logLevel, str, vl);
    va_end(vl);
}

#define DEFINE_QBS_LOG_FUNCTION(LogLevel) \
    void qbs##LogLevel(const char *str, ...) \
    { \
        const LoggerLevel level = Logger##LogLevel; \
        Logger &logger = Logger::instance(); \
        if (logger.level() >= level) { \
            va_list vl; \
            va_start(vl, str); \
            qbsLog_impl(level, str, vl); \
            va_end(vl); \
        } \
    } \

DEFINE_QBS_LOG_FUNCTION(Error)
DEFINE_QBS_LOG_FUNCTION(Warning)
DEFINE_QBS_LOG_FUNCTION(Info)
DEFINE_QBS_LOG_FUNCTION(Debug)
DEFINE_QBS_LOG_FUNCTION(Trace)

LogWriter qbsLog(LoggerLevel level)
{
    return LogWriter(level);
}

LogWriter qbsError()
{
    return LogWriter(LoggerError);
}

LogWriter qbsWarning()
{
    return LogWriter(LoggerWarning);
}

LogWriter qbsInfo()
{
    return LogWriter(LoggerInfo);
}

LogWriter qbsDebug()
{
    return LogWriter(LoggerDebug);
}

LogWriter qbsTrace()
{
    return LogWriter(LoggerTrace);
}

LogWriter operator<<(LogWriter w, const char *str)
{
    w.write(str);
    return w;
}

LogWriter operator<<(LogWriter w, const QByteArray &byteArray)
{
    w.write(byteArray.data());
    return w;
}

LogWriter operator<<(LogWriter w, const QString &str)
{
    w.write(qPrintable(str));
    return w;
}

LogWriter operator<<(LogWriter w, const QStringList &strList)
{
    w.write('[');
    for (int i = 0; i < strList.size(); ++i) {
        w.write(qPrintable(strList.at(i)));
        if (i != strList.size() - 1)
            w.write(", ");
    }
    w.write(']');
    return w;
}

LogWriter operator<<(LogWriter w, const QSet<QString> &strSet)
{
    bool firstLoop = true;
    w.write('(');
    foreach (const QString &str, strSet) {
        if (firstLoop)
            firstLoop = false;
        else
            w.write(", ");
        w.write(qPrintable(str));
    }
    w.write(')');
    return w;
}

LogWriter operator<<(LogWriter w, const QVariant &variant)
{
    QString str = variant.typeName() + QLatin1String("(");
    if (variant.type() == QVariant::List) {
        bool firstLoop = true;
        foreach (const QVariant &item, variant.toList()) {
            str += item.toString();
            if (firstLoop)
                firstLoop = false;
            else
                str += QLatin1String(", ");
        }
    } else {
        str += variant.toString();
    }
    str += QLatin1String(")");
    w.write(qPrintable(str));
    return w;
}

LogWriter operator<<(LogWriter w, int n)
{
    return w << QString::number(n);
}

LogWriter operator<<(LogWriter w, qint64 n)
{
    return w << QString::number(n);
}

LogWriter operator<<(LogWriter w, bool b)
{
    return w << (b ? "true" : "false");
}

LogWriter operator<<(LogWriter w, LogOutputChannel channel)
{
    w.setOutputChannel(channel);
    return w;
}

LogWriter operator<<(LogWriter w, LogModifier modifier)
{
    switch (modifier) {
    case DontPrintLogLevel:
        w.setPrintLogLevel(false);
        break;
    }
    return w;
}

LogWriter operator<<(LogWriter w, TextColor color)
{
    w.setTextColor(color);
    return w;
}

struct TimedActivityLogger::TimedActivityLoggerPrivate
{
    QString prefix;
    QString activity;
    LoggerLevel logLevel;
    QElapsedTimer timer;
};

TimedActivityLogger::TimedActivityLogger(const QString &activity, const QString &prefix,
        LoggerLevel logLevel)
    : d(0)
{
    if (!qbsLogLevel(logLevel))
        return;
    d = new TimedActivityLoggerPrivate;
    d->prefix = prefix;
    d->activity = activity;
    d->logLevel = logLevel;
    LogWriter(d->logLevel) << QString::fromLocal8Bit("%1Starting activity '%2'.")
            .arg(d->prefix, d->activity);
    d->timer.start();
}

void TimedActivityLogger::finishActivity()
{
    if (!d)
        return;
    qint64 ms = d->timer.elapsed();
    qint64 s = ms/1000;
    ms -= s*1000;
    qint64 m = s/60;
    s -= m*60;
    const qint64 h = m/60;
    m -= h*60;
    QString timeString = QString::fromLocal8Bit("%1ms").arg(ms);
    if (h || m || s)
        timeString.prepend(QString::fromLocal8Bit("%1s, ").arg(s));
    if (h || m)
        timeString.prepend(QString::fromLocal8Bit("%1m, ").arg(m));
    if (h)
        timeString.prepend(QString::fromLocal8Bit("%1h, ").arg(h));
    LogWriter(d->logLevel) << QString::fromLocal8Bit("%1Activity '%2' took %3.")
            .arg(d->prefix, d->activity, timeString);
    delete d;
    d = 0;
}

TimedActivityLogger::~TimedActivityLogger()
{
    finishActivity();
}

} // namespace qbs
