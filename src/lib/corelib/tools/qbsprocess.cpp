/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qbsprocess.h"

#include "launcherinterface.h"
#include "launchersocket.h"
#include "qbsassert.h"

#include <logging/translator.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qtimer.h>

namespace qbs {
namespace Internal {

QbsProcess::QbsProcess(QObject *parent) : QObject(parent)
{
    connect(LauncherInterface::socket(), &LauncherSocket::ready,
            this, &QbsProcess::handleSocketReady);
    connect(LauncherInterface::socket(), &LauncherSocket::errorOccurred,
            this, &QbsProcess::handleSocketError);
    connect(LauncherInterface::socket(), &LauncherSocket::packetArrived,
            this, &QbsProcess::handlePacket);
}

void QbsProcess::start(const QString &command, const QStringList &arguments)
{
    if (m_socketError) {
        m_error = QProcess::FailedToStart;
        emit error(m_error);
        return;
    }
    m_command = command;
    m_arguments = arguments;
    m_state = QProcess::Starting;
    if (LauncherInterface::socket()->isReady())
        doStart();
}

void QbsProcess::doStart()
{
    m_state = QProcess::Running;
    StartProcessPacket p(token());
    p.command = m_command;
    p.arguments = m_arguments;
    p.env = m_environment.toStringList();
    p.workingDir = m_workingDirectory;
    sendPacket(p);
}

void QbsProcess::cancel()
{
    switch (m_state) {
    case QProcess::NotRunning:
        break;
    case QProcess::Starting:
        m_errorString = Tr::tr("Process canceled before it was started.");
        m_error = QProcess::FailedToStart;
        m_state = QProcess::NotRunning;
        emit error(m_error);
        break;
    case QProcess::Running:
        sendPacket(StopProcessPacket(token()));
        break;
    }
}

QByteArray QbsProcess::readAllStandardOutput()
{
    return readAndClear(m_stdout);
}

QByteArray QbsProcess::readAllStandardError()
{
    return readAndClear(m_stderr);
}

void QbsProcess::sendPacket(const LauncherPacket &packet)
{
    LauncherInterface::socket()->sendData(packet.serialize());
}

QByteArray QbsProcess::readAndClear(QByteArray &data)
{
    const QByteArray tmp = data;
    data.clear();
    return tmp;
}

void QbsProcess::handlePacket(LauncherPacketType type, quintptr token, const QByteArray &payload)
{
    if (token != this->token())
        return;
    switch (type) {
    case LauncherPacketType::ProcessError:
        handleErrorPacket(payload);
        break;
    case LauncherPacketType::ProcessFinished:
        handleFinishedPacket(payload);
        break;
    default:
        QBS_ASSERT(false, break);
    }
}

void QbsProcess::handleSocketReady()
{
    m_socketError = false;
    if (m_state == QProcess::Starting)
        doStart();
}

void QbsProcess::handleSocketError(const QString &message)
{
    m_socketError = true;
    m_errorString = Tr::tr("Internal socket error: %1").arg(message);
    if (m_state != QProcess::NotRunning) {
        m_state = QProcess::NotRunning;
        m_error = QProcess::FailedToStart;
        emit error(m_error);
    }
}

void QbsProcess::handleErrorPacket(const QByteArray &packetData)
{
    QBS_ASSERT(m_state != QProcess::NotRunning, return);
    const auto packet = LauncherPacket::extractPacket<ProcessErrorPacket>(token(), packetData);
    m_error = packet.error;
    m_errorString = packet.errorString;
    m_state = QProcess::NotRunning;
    emit error(m_error);
}

void QbsProcess::handleFinishedPacket(const QByteArray &packetData)
{
    QBS_ASSERT(m_state == QProcess::Running, return);
    m_state = QProcess::NotRunning;
    const auto packet = LauncherPacket::extractPacket<ProcessFinishedPacket>(token(), packetData);
    m_exitCode = packet.exitCode;
    m_stdout = packet.stdOut;
    m_stderr = packet.stdErr;
    m_errorString = packet.errorString;
    emit finished(m_exitCode);
}

} // namespace Internal
} // namespace qbs
