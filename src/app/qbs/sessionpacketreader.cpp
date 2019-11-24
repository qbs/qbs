/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "sessionpacketreader.h"

#include "sessionpacket.h"
#include "stdinreader.h"

namespace qbs {
namespace Internal {

class SessionPacketReader::Private
{
public:
    QByteArray incomingData;
    SessionPacket currentPacket;
};

SessionPacketReader::SessionPacketReader(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{ }

SessionPacketReader::~SessionPacketReader() = default;

void SessionPacketReader::start()
{
    StdinReader * const stdinReader = StdinReader::create(this);
    connect(stdinReader, &StdinReader::errorOccurred, this, &SessionPacketReader::errorOccurred);
    connect(stdinReader, &StdinReader::dataAvailable, this, [this](const QByteArray &data) {
        d->incomingData += data;
        while (!d->incomingData.isEmpty()) {
            switch (d->currentPacket.parseInput(d->incomingData)) {
            case SessionPacket::Status::Invalid:
                emit errorOccurred(tr("Received invalid input."));
                return;
            case SessionPacket::Status::Complete:
                emit packetReceived(d->currentPacket.retrievePacket());
                break;
            case SessionPacket::Status::Incomplete:
                return;
            }
        }
    });
    stdinReader->start();
}

} // namespace Internal
} // namespace qbs
