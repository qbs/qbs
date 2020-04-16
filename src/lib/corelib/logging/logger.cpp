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

#if defined(_MSC_VER) && _MSC_VER > 0
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "logger.h"

#include <tools/set.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qvariant.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <set>

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
            w.write(QStringLiteral(", "));
    }
    w.write(']');
    return w;
}

LogWriter operator<<(LogWriter w, const Internal::Set<QString> &strSet)
{
    bool firstLoop = true;
    w.write('(');
    for (const QString &str : strSet) {
        if (firstLoop)
            firstLoop = false;
        else
            w.write(QStringLiteral(", "));
        w.write(str);
    }
    w.write(')');
    return w;
}

LogWriter operator<<(LogWriter w, const QVariant &variant)
{
    QString str = QLatin1String(variant.typeName()) + QLatin1Char('(');
    if (variant.type() == QVariant::List) {
        bool firstLoop = true;
        const auto list = variant.toList();
        for (const QVariant &item : list) {
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
    w.write(QString::number(n));
    return w;
}

LogWriter operator<<(LogWriter w, qint64 n)
{
    w.write(QString::number(n));
    return w;
}

LogWriter operator<<(LogWriter w, bool b)
{
    w.write(QString::fromLatin1(b ? "true" : "false"));
    return w;
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

void Logger::printWarning(const ErrorInfo &warning)
{
    if (m_storeWarnings)
        m_warnings.push_back(warning);
    logSink()->printWarning(warning);
}

LogWriter Logger::qbsLog(LoggerLevel level, bool force) const
{
    return LogWriter(m_logSink, level, force);
}

} // namespace Internal
} // namespace qbs
