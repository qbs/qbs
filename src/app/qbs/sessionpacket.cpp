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

#include "sessionpacket.h"

#include <tools/qbsassert.h>
#include <tools/stringconstants.h>
#include <tools/version.h>

#include <QtCore/qdebug.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qstring.h>

namespace qbs {
namespace Internal {

const QByteArray packetStart = "qbsmsg:";

SessionPacket::Status SessionPacket::parseInput(QByteArray &input)
{
    //qDebug() << m_expectedPayloadLength << m_payload << input;
    if (m_expectedPayloadLength == -1) {
        const int packetStartOffset = input.indexOf(packetStart);
        if (packetStartOffset == -1)
            return Status::Incomplete;
        const int numberOffset = packetStartOffset + packetStart.length();
        const int newLineOffset = input.indexOf('\n', numberOffset);
        if (newLineOffset == -1)
            return Status::Incomplete;
        const QByteArray sizeString = input.mid(numberOffset, newLineOffset - numberOffset);
        bool isNumber;
        const int payloadLen = sizeString.toInt(&isNumber);
        if (!isNumber || payloadLen < 0)
            return Status::Invalid;
        m_expectedPayloadLength = payloadLen;
        input.remove(0, newLineOffset + 1);
    }
    const int bytesToAdd = m_expectedPayloadLength - m_payload.length();
    QBS_ASSERT(bytesToAdd >= 0, return Status::Invalid);
    m_payload += input.left(bytesToAdd);
    input.remove(0, bytesToAdd);
    return isComplete() ? Status::Complete : Status::Incomplete;
}

QJsonObject SessionPacket::retrievePacket()
{
    QBS_ASSERT(isComplete(), return QJsonObject());
    const auto packet = QJsonDocument::fromJson(QByteArray::fromBase64(m_payload)).object();
    m_payload.clear();
    m_expectedPayloadLength = -1;
    return packet;
}

QByteArray SessionPacket::createPacket(const QJsonObject &packet)
{
    const QByteArray jsonData = QJsonDocument(packet).toJson(QJsonDocument::Compact).toBase64();
    return QByteArray(packetStart).append(QByteArray::number(jsonData.length())).append('\n')
            .append(jsonData);
}

QJsonObject SessionPacket::helloMessage()
{
    return QJsonObject{
        {StringConstants::type(), QLatin1String("hello")},
        {QLatin1String("api-level"), 2},
        {QLatin1String("api-compat-level"), 2}
    };
}

bool SessionPacket::isComplete() const
{
    return m_payload.length() == m_expectedPayloadLength;
}

} // namespace Internal
} // namespace qbs
