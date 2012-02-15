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
#ifndef QBS_MAINTHREADCOMMUNICATION_H
#define QBS_MAINTHREADCOMMUNICATION_H

#include "globals.h"
#include "processoutput.h"
#include "ilogsink.h"

#include <QtCore/QWeakPointer>
#include <QtCore/QReadWriteLock>

namespace Qbs {

class MainThreadCommunication : public ILogSink
{
public:
    MainThreadCommunication();
    ~MainThreadCommunication();

    static MainThreadCommunication &instance();

    void addLogEventReceiver(QObject *object);
    void removeLogEventReceiver(QObject *object);

    void addProcessOutputEventReceiver(QObject *object);
    void removeProcessOutputEventReceiver(QObject *object);

    static void registerMetaType();

public:
    void outputLogMessage(LoggerLevel level, const LogMessage &message);
    void processOutput(const Qbs::ProcessOutput &processOutput);

private:
    QReadWriteLock m_lock;
    QList<QWeakPointer<QObject> > m_logReceiverList;
    QList<QWeakPointer<QObject> > m_processOutputReceiverList;

};

} // namespace Qbs

#endif // QBS_MAINTHREADCOMMUNICATION_H
