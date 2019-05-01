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
#include "ilogsink.h"

#include <tools/error.h>

#include <QtCore/qbytearray.h>

#include <mutex>

namespace qbs {

QString logLevelTag(LoggerLevel level)
{
    if (level == LoggerInfo)
        return {};
    QString str = logLevelName(level).toUpper();
    if (!str.isEmpty())
        str.append(QLatin1String(": "));
    return str;
}

QString logLevelName(LoggerLevel level)
{
    switch (level) {
    case qbs::LoggerError:
        return QStringLiteral("error");
    case qbs::LoggerWarning:
        return QStringLiteral("warning");
    case qbs::LoggerInfo:
        return QStringLiteral("info");
    case qbs::LoggerDebug:
        return QStringLiteral("debug");
    case qbs::LoggerTrace:
        return QStringLiteral("trace");
    default:
        break;
    }
    return {};
}

class ILogSink::ILogSinkPrivate
{
public:
    LoggerLevel logLevel = defaultLogLevel();
    std::mutex mutex;
};

ILogSink::ILogSink() : d(new ILogSinkPrivate)
{
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
