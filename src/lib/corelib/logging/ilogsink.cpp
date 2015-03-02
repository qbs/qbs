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
#include "ilogsink.h"

#include <tools/error.h>

#include <QByteArray>
#include <QMutex>

namespace qbs {

QString logLevelTag(LoggerLevel level)
{
    if (level == LoggerInfo)
        return QString();
    QString str = logLevelName(level).toUpper();
    if (!str.isEmpty())
        str.append(QLatin1String(": "));
    return str;
}

QString logLevelName(LoggerLevel level)
{
    switch (level) {
    case qbs::LoggerError:
        return QLatin1String("error");
    case qbs::LoggerWarning:
        return QLatin1String("warning");
    case qbs::LoggerInfo:
        return QLatin1String("info");
    case qbs::LoggerDebug:
        return QLatin1String("debug");
    case qbs::LoggerTrace:
        return QLatin1String("trace");
    default:
        break;
    }
    return QString();
}

class ILogSink::ILogSinkPrivate
{
public:
    LoggerLevel logLevel;
    QMutex mutex;
};

ILogSink::ILogSink() : d(new ILogSinkPrivate)
{
    d->logLevel = defaultLogLevel();
}

ILogSink::~ILogSink()
{
    delete d;
}

void ILogSink::setLogLevel(LoggerLevel level)
{
    d->logLevel = level;
}

LoggerLevel ILogSink::logLevel() const
{
    return d->logLevel;
}

void ILogSink::printWarning(const ErrorInfo &warning)
{
    if (willPrint(LoggerWarning)) {
        d->mutex.lock();
        doPrintWarning(warning);
        d->mutex.unlock();
    }
}

void ILogSink::printMessage(LoggerLevel level, const QString &message, const QString &tag,
                            bool force)
{
    if (force || willPrint(level)) {
        d->mutex.lock();
        doPrintMessage(level, message, tag);
        d->mutex.unlock();
    }
}

void ILogSink::doPrintWarning(const ErrorInfo &warning)
{
    doPrintMessage(LoggerWarning, warning.toString(), QString());
}

} // namespace qbs
