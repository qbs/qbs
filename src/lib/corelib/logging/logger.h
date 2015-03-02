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

#ifndef QBS_LOGGER_H
#define QBS_LOGGER_H

#include "ilogsink.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QVariant;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

// Note that while these classes are not part of the API, we export some stuff for use by
// our command line tools for the sake of a uniform logging approach.

class QBS_EXPORT LogWriter
{
public:
    LogWriter(ILogSink *logSink, LoggerLevel level, bool force = false);

    // log writer has move semantics and the last instance of
    // a << chain prints the accumulated data
    LogWriter(const LogWriter &other);
    ~LogWriter();
    const LogWriter &operator=(const LogWriter &other);

    void write(char c);
    void write(const char *str);
    void write(const QChar &c);
    void write(const QString &message);

    void setMessageTag(const QString &tag);

private:
    ILogSink *m_logSink;
    LoggerLevel m_level;
    mutable QString m_message;
    QString m_tag;
    bool m_force;
};

class QBS_EXPORT MessageTag
{
public:
    explicit MessageTag(const QString &tag) : m_tag(tag) {}

    const QString &tag() const { return m_tag; }

private:
    QString m_tag;
};

QBS_EXPORT LogWriter operator<<(LogWriter w, const char *str);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QByteArray &byteArray);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QString &str);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QStringList &strList);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QSet<QString> &strSet);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QVariant &variant);
QBS_EXPORT LogWriter operator<<(LogWriter w, int n);
QBS_EXPORT LogWriter operator<<(LogWriter w, qint64 n);
QBS_EXPORT LogWriter operator<<(LogWriter w, bool b);
QBS_EXPORT LogWriter operator<<(LogWriter w, const MessageTag &tag);

class QBS_EXPORT Logger
{
public:
    Logger(ILogSink *logSink = 0);

    ILogSink *logSink() const { return m_logSink; }

    bool debugEnabled() const;
    bool traceEnabled() const;

    void printWarning(const ErrorInfo &warning) { logSink()->printWarning(warning); }

    LogWriter qbsLog(LoggerLevel level, bool force = false) const;
    LogWriter qbsWarning() const { return qbsLog(LoggerWarning); }
    LogWriter qbsInfo() const { return qbsLog(LoggerInfo); }
    LogWriter qbsDebug() const { return qbsLog(LoggerDebug); }
    LogWriter qbsTrace() const { return qbsLog(LoggerTrace); }

private:
    ILogSink *m_logSink;
};


class TimedActivityLogger
{
public:
    TimedActivityLogger(const Logger &logger, const QString &activity,
                        const QString &prefix = QString(), LoggerLevel logLevel = LoggerDebug,
                        bool alwaysLog = false);
    void finishActivity();
    ~TimedActivityLogger();

private:
    class TimedActivityLoggerPrivate;
    TimedActivityLoggerPrivate *d;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_LOGGER_H
