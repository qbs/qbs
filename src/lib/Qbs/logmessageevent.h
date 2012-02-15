/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/
#ifndef QBS_QBSLOGMESSAGEEVENT_H
#define QBS_QBSLOGMESSAGEEVENT_H

#include "globals.h"

#include <QtCore/QEvent>
#include <QtCore/QString>

namespace Qbs {

class LogMessageEvent : public QEvent
{
public:
    LogMessageEvent(LoggerLevel level, const QString &message);

    static LogMessageEvent *toLogMessageEvent(QEvent *event);
    static bool isLogMessageEvent(QEvent *event);

    LoggerLevel level() const;
    QString message() const;


private:
    static int s_eventType;
    LoggerLevel m_level;
    QString m_message;
};

} // namespace Qbs

#endif // QBS_QBSLOGMESSAGEEVENT_H
