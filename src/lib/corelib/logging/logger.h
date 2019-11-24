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

#ifndef QBS_LOGGER_H
#define QBS_LOGGER_H

#include "ilogsink.h"

#include <tools/error.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

QT_BEGIN_NAMESPACE
class QVariant;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

template<typename T> class Set;

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
    const LogWriter &operator=(const LogWriter &other); // NOLINT

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
    explicit MessageTag(QString tag) : m_tag(std::move(tag)) {}

    const QString &tag() const { return m_tag; }

private:
    QString m_tag;
};

QBS_EXPORT LogWriter operator<<(LogWriter w, const char *str);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QByteArray &byteArray);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QString &str);
QBS_EXPORT LogWriter operator<<(LogWriter w, const QStringList &strList);
QBS_EXPORT LogWriter operator<<(LogWriter w, const Internal::Set<QString> &strSet);
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

    void printWarning(const ErrorInfo &warning);
    QList<ErrorInfo> warnings() const { return m_warnings; }
    void clearWarnings() { m_warnings.clear(); }
    void storeWarnings() { m_storeWarnings = true; }

    LogWriter qbsLog(LoggerLevel level, bool force = false) const;
    LogWriter qbsWarning() const { return qbsLog(LoggerWarning); }
    LogWriter qbsInfo() const { return qbsLog(LoggerInfo); }
    LogWriter qbsDebug() const { return qbsLog(LoggerDebug); }
    LogWriter qbsTrace() const { return qbsLog(LoggerTrace); }

private:
    ILogSink *m_logSink;
    QList<ErrorInfo> m_warnings;
    bool m_storeWarnings = false;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_LOGGER_H
