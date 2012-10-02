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
#include "mainthreadcommunication.h"

#include <tools/logger.h>

#include "logmessageevent.h"
#include "processoutputevent.h"

#include <QCoreApplication>
#include <QReadLocker>
#include <QWriteLocker>

namespace Qbs {

typedef QPointer<QObject> ObjectPointer;

MainThreadCommunication::MainThreadCommunication()
{
    qbs::Logger::instance().setLevel(LoggerMaxLevel);
    setGlobalLogSink(this);
}

MainThreadCommunication::~MainThreadCommunication()
{
    cleanupGlobalLogSink();
}

MainThreadCommunication &MainThreadCommunication::instance()
{
    static MainThreadCommunication instance;
    return instance;
}

void MainThreadCommunication::addLogEventReceiver(QObject *object)
{
    QWriteLocker locker(&m_lock);
    if (!m_logReceiverList.contains(ObjectPointer(object)))
        m_logReceiverList.append(ObjectPointer(object));
}

void MainThreadCommunication::removeLogEventReceiver(QObject *object)
{
    QReadLocker locker(&m_lock);
    m_logReceiverList.removeOne(ObjectPointer(object));
}

void MainThreadCommunication::addProcessOutputEventReceiver(QObject *object)
{
    QWriteLocker locker(&m_lock);
    if (!m_processOutputReceiverList.contains(ObjectPointer(object)))
        m_processOutputReceiverList.append(ObjectPointer(object));}

void MainThreadCommunication::removeProcessOutputEventReceiver(QObject *object)
{
    QReadLocker locker(&m_lock);
    m_processOutputReceiverList.append(ObjectPointer(object));
}

void MainThreadCommunication::registerMetaType()
{
    qRegisterMetaType<Qbs::ProcessOutput>("Qbs::ProcessOutput");
    qRegisterMetaTypeStreamOperators<Qbs::ProcessOutput>("Qbs::ProcessOutput");
    qRegisterMetaType<Qbs::LoggerLevel>("Qbs::LoggerLevel");
}

void MainThreadCommunication::outputLogMessage(LoggerLevel level, const LogMessage &message)
{
    QReadLocker locker(&m_lock);
    foreach (const ObjectPointer &logReceiver, m_logReceiverList) {
        if (logReceiver.data()) {
            LogMessageEvent *logMessageEvent = new LogMessageEvent(level, QString::fromLocal8Bit(message.data));
            QCoreApplication::postEvent(logReceiver.data(), logMessageEvent);
        }
    }
}

void MainThreadCommunication::processOutput(const Qbs::ProcessOutput &processOutput)
{
    QReadLocker locker(&m_lock);
    foreach (const ObjectPointer &processOutputReceiver, m_processOutputReceiverList) {
        if (processOutputReceiver.data()) {
            ProcessOutputEvent *processOutputEvent = new ProcessOutputEvent(processOutput);
            QCoreApplication::postEvent(processOutputReceiver.data(), processOutputEvent);
        }
    }
}

} // namespace Qbs
