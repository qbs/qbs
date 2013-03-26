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
namespace Internal {

LogWriter::LogWriter(ILogSink *logSink, LoggerLevel level, bool force)
    : m_logSink(logSink), m_level(level), m_force(force)
{}

LogWriter::LogWriter(const LogWriter &other)
    : m_logSink(other.m_logSink)
    , m_level(other.m_level)
    , m_message(other.m_message)
    , m_tag(other.m_tag)
    , m_force(other.m_force)
{
    other.m_message.clear();
}

LogWriter::~LogWriter()
{
    if (!m_message.isEmpty())
        m_logSink->printMessage(m_level, m_message, m_tag, m_force);
}

const LogWriter &LogWriter::operator=(const LogWriter &other)
{
    m_logSink = other.m_logSink;
    m_level = other.m_level;
    m_message = other.m_message;
    m_tag = other.m_tag;
    m_force = other.m_force;
    other.m_message.clear();
    return *this;
}

void LogWriter::write(char c)
{
    write(QLatin1Char(c));
}

void LogWriter::write(const char *str)
{
    write(QLatin1String(str));
}

void LogWriter::write(const QChar &c)
{
    if (m_force || m_logSink->logLevel() >= m_level)
        m_message.append(c);
}

void LogWriter::write(const QString &message)
{
    if (m_force || m_logSink->logLevel() >= m_level)
        m_message += message;
}

void LogWriter::setMessageTag(const QString &tag)
{
    m_tag = tag;
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
    w.write(str);
    return w;
}

LogWriter operator<<(LogWriter w, const QStringList &strList)
{
    w.write('[');
    for (int i = 0; i < strList.size(); ++i) {
        w.write(strList.at(i));
        if (i != strList.size() - 1)
            w.write(QLatin1String(", "));
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
            w.write(QLatin1String(", "));
        w.write(str);
    }
    w.write(')');
    return w;
}

LogWriter operator<<(LogWriter w, const QVariant &variant)
{
    QString str = variant.typeName() + QLatin1Char('(');
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
    str += QLatin1Char(')');
    w.write(str);
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
    return w << QString(QLatin1String(b ? "true" : "false"));
}

LogWriter operator<<(LogWriter w, const MessageTag &tag)
{
    w.setMessageTag(tag.tag());
    return w;
}

Logger::Logger(ILogSink *logger) : m_logSink(logger)
{
}

bool Logger::debugEnabled() const
{
    return m_logSink->willPrint(LoggerDebug);
}

bool Logger::traceEnabled() const
{
    return m_logSink->willPrint(LoggerTrace);
}

LogWriter Logger::qbsLog(LoggerLevel level, bool force) const
{
    return LogWriter(m_logSink, level, force);
}


class TimedActivityLogger::TimedActivityLoggerPrivate
{
public:
    Logger logger;
    QString prefix;
    QString activity;
    LoggerLevel logLevel;
    QElapsedTimer timer;
    bool alwaysLog;
};

TimedActivityLogger::TimedActivityLogger(const Logger &logger, const QString &activity,
        const QString &prefix, LoggerLevel logLevel, bool alwaysLog)
    : d(0)
{
    if (!alwaysLog && !logger.logSink()->willPrint(logLevel))
        return;
    d = new TimedActivityLoggerPrivate;
    d->logger = logger;
    d->prefix = prefix;
    d->activity = activity;
    d->logLevel = logLevel;
    d->alwaysLog = alwaysLog;
    d->logger.qbsLog(logLevel, alwaysLog) << QString::fromLocal8Bit("%1Starting activity '%2'.")
            .arg(prefix, activity);
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
    d->logger.qbsLog(d->logLevel, d->alwaysLog)
            << QString::fromLocal8Bit("%1Activity '%2' took %3.")
               .arg(d->prefix, d->activity, timeString);
    delete d;
    d = 0;
}

TimedActivityLogger::~TimedActivityLogger()
{
    finishActivity();
}

} // namespace Internal
} // namespace qbs
