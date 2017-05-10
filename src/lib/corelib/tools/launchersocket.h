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

#ifndef QBS_LAUNCHERSOCKET_H
#define QBS_LAUNCHERSOCKET_H

#include "launcherpackets.h"

#include <QtCore/qobject.h>

#include <mutex>
#include <vector>

QT_BEGIN_NAMESPACE
class QLocalSocket;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class LauncherInterface;

class LauncherSocket : public QObject
{
    Q_OBJECT
    friend class LauncherInterface;
public:
    bool isReady() const { return m_socket; }
    void sendData(const QByteArray &data);

signals:
    void ready();
    void errorOccurred(const QString &error);
    void packetArrived(qbs::Internal::LauncherPacketType type, quintptr token,
                       const QByteArray &payload);

private:
    LauncherSocket(QObject *parent);

    void setSocket(QLocalSocket *socket);
    void shutdown();

    void handleSocketError();
    void handleSocketDataAvailable();
    void handleSocketDisconnected();
    void handleError(const QString &error);
    void handleRequests();

    QLocalSocket *m_socket = nullptr;
    PacketParser m_packetParser;
    std::vector<QByteArray> m_requests;
    std::mutex m_requestsMutex;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
