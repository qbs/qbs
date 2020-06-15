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

#include "launchersocket.h"

#include "qbsassert.h"
#include "qttools.h"

#include <logging/translator.h>

#include <QtCore/qtimer.h>
#include <QtNetwork/qlocalsocket.h>

namespace qbs {
namespace Internal {

LauncherSocket::LauncherSocket(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<qbs::Internal::LauncherPacketType>();
    qRegisterMetaType<quintptr>("quintptr");
}

void LauncherSocket::sendData(const QByteArray &data)
{
    if (!isReady())
        return;
    std::lock_guard<std::mutex> locker(m_requestsMutex);
    m_requests.push_back(data);
    if (m_requests.size() == 1)
        QTimer::singleShot(0, this, &LauncherSocket::handleRequests);
}

void LauncherSocket::shutdown()
{
    QBS_ASSERT(m_socket, return);
    m_socket->disconnect();
    m_socket->write(ShutdownPacket().serialize());
    m_socket->waitForBytesWritten(1000);
    m_socket->deleteLater();
    m_socket = nullptr;
}

void LauncherSocket::setSocket(QLocalSocket *socket)
{
    QBS_ASSERT(!m_socket, return);
    m_socket = socket;
    m_packetParser.setDevice(m_socket);
    connect(m_socket,
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
            static_cast<void(QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error),
#else
            &QLocalSocket::errorOccurred,
#endif
            this, &LauncherSocket::handleSocketError);
    connect(m_socket, &QLocalSocket::readyRead,
            this, &LauncherSocket::handleSocketDataAvailable);
    connect(m_socket, &QLocalSocket::disconnected,
            this, &LauncherSocket::handleSocketDisconnected);
    emit ready();
}

void LauncherSocket::handleSocketError()
{
    if (m_socket->error() != QLocalSocket::PeerClosedError)
        handleError(Tr::tr("Socket error: %1").arg(m_socket->errorString()));
}

void LauncherSocket::handleSocketDataAvailable()
{
    try {
        if (!m_packetParser.parse())
            return;
    } catch (const PacketParser::InvalidPacketSizeException &e) {
        handleError(Tr::tr("Internal protocol error: invalid packet size %1.").arg(e.size));
        return;
    }
    switch (m_packetParser.type()) {
    case LauncherPacketType::ProcessError:
    case LauncherPacketType::ProcessFinished:
        emit packetArrived(m_packetParser.type(), m_packetParser.token(),
                           m_packetParser.packetData());
        break;
    default:
        handleError(Tr::tr("Internal protocol error: invalid packet type %1.")
                    .arg(static_cast<int>(m_packetParser.type())));
        return;
    }
    handleSocketDataAvailable();
}

void LauncherSocket::handleSocketDisconnected()
{
    handleError(Tr::tr("Launcher socket closed unexpectedly"));
}

void LauncherSocket::handleError(const QString &error)
{
    m_socket->disconnect();
    m_socket->deleteLater();
    m_socket = nullptr;
    emit errorOccurred(error);
}

void LauncherSocket::handleRequests()
{
    QBS_ASSERT(isReady(), return);
    std::lock_guard<std::mutex> locker(m_requestsMutex);
    for (const QByteArray &request : qAsConst(m_requests))
        m_socket->write(request);
    m_requests.clear();
}

} // namespace Internal
} // namespace qbs
