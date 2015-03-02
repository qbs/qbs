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
#ifndef QBS_ILOGSINK_H
#define QBS_ILOGSINK_H

#include "../tools/qbs_export.h"

#include <QString>

namespace qbs {
class ErrorInfo;

enum LoggerLevel
{
    LoggerMinLevel,
    LoggerError = LoggerMinLevel,
    LoggerWarning,
    LoggerInfo,
    LoggerDebug,
    LoggerTrace,
    LoggerMaxLevel = LoggerTrace
};

inline LoggerLevel defaultLogLevel() { return LoggerInfo; }
QBS_EXPORT QString logLevelTag(LoggerLevel level);
QBS_EXPORT QString logLevelName(LoggerLevel level);

class QBS_EXPORT ILogSink
{
    Q_DISABLE_COPY(ILogSink)
public:
    ILogSink();
    virtual ~ILogSink();

    void setLogLevel(LoggerLevel level);
    LoggerLevel logLevel() const;

    bool willPrint(LoggerLevel level) const { return level <= logLevel(); }

    void printWarning(const ErrorInfo &warning);
    void printMessage(LoggerLevel level, const QString &message,
                      const QString &tag = QString(), bool force = false);

private:
    virtual void doPrintWarning(const ErrorInfo &warning);
    virtual void doPrintMessage(LoggerLevel level, const QString &message,
                                const QString &tag) = 0;

    class ILogSinkPrivate;
    ILogSinkPrivate * const d;
};

} // namespace qbs

#endif // Include guard
