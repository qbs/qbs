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

#include "profiling.h"

#include <logging/logger.h>
#include <logging/translator.h>

#include <QtCore/qstring.h>

namespace qbs {
namespace Internal {

class TimedActivityLogger::TimedActivityLoggerPrivate
{
public:
    Logger logger;
    QString activity;
    QElapsedTimer timer;
};

TimedActivityLogger::TimedActivityLogger(const Logger &logger, const QString &activity,
        bool enabled)
    : d(nullptr)
{
    if (!enabled)
        return;
    d = new TimedActivityLoggerPrivate;
    d->logger = logger;
    d->activity = activity;
    d->logger.qbsLog(LoggerInfo, true) << Tr::tr("Starting activity '%2'.").arg(activity);
    d->timer.start();
}

void TimedActivityLogger::finishActivity()
{
    if (!d)
        return;
    const QString timeString = elapsedTimeString(d->timer.elapsed());
    d->logger.qbsLog(LoggerInfo, true)
            << Tr::tr("Activity '%2' took %3.").arg(d->activity, timeString);
    delete d;
    d = nullptr;
}

TimedActivityLogger::~TimedActivityLogger()
{
    finishActivity();
}

AccumulatingTimer::AccumulatingTimer(qint64 *elapsedTime) : m_elapsedTime(elapsedTime)
{
    if (elapsedTime)
        m_timer.start();
}

AccumulatingTimer::~AccumulatingTimer()
{
    stop();
}

void AccumulatingTimer::stop()
{
    if (!m_timer.isValid())
        return;
    *m_elapsedTime += m_timer.elapsed();
    m_timer.invalidate();
}

QString elapsedTimeString(qint64 elapsedTimeInMs)
{
    qint64 ms = elapsedTimeInMs;
    qint64 s = ms/1000;
    ms -= s*1000;
    qint64 m = s/60;
    s -= m*60;
    const qint64 h = m/60;
    m -= h*60;
    QString timeString = QStringLiteral("%1ms").arg(ms);
    if (h || m || s)
        timeString.prepend(QStringLiteral("%1s, ").arg(s));
    if (h || m)
        timeString.prepend(QStringLiteral("%1m, ").arg(m));
    if (h)
        timeString.prepend(QStringLiteral("%1h, ").arg(h));
    return timeString;
}

} // namespace Internal
} // namespace qbs
