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

#include "stdinreader.h"

#include <tools/hostosinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtimer.h>

#include <cerrno>
#include <cstring>

#ifdef Q_OS_WIN32
#include <qt_windows.h>
#else
#include <fcntl.h>
#endif

namespace qbs {
namespace Internal {

class UnixStdinReader : public StdinReader
{
public:
    UnixStdinReader(QObject *parent) : StdinReader(parent), m_notifier(0, QSocketNotifier::Read) {}

private:
    void start() override
    {
        if (!m_stdIn.open(stdin, QIODevice::ReadOnly)) {
            emit errorOccurred(tr("Cannot read from standard input."));
            return;
        }
#ifdef Q_OS_UNIX
        const auto emitError = [this] {
            emit errorOccurred(tr("Failed to make standard input non-blocking: %1")
                               .arg(QLatin1String(std::strerror(errno))));
        };
        const int flags = fcntl(0, F_GETFL, 0);
        if (flags == -1) {
            emitError();
            return;
        }
        if (fcntl(0, F_SETFL, flags | O_NONBLOCK)) {
            emitError();
            return;
        }
#endif
        connect(&m_notifier, &QSocketNotifier::activated, this, [this] {
            emit dataAvailable(m_stdIn.readAll());
        });

        // Neither the aboutToClose() nor the readChannelFinished() signals
        // are triggering, so we need a timer to check whether the controlling
        // process disappeared.
        const auto stdinClosedChecker = new QTimer(this);
        connect(stdinClosedChecker, &QTimer::timeout, this, [this, stdinClosedChecker] {
            if (m_stdIn.atEnd()) {
                stdinClosedChecker->stop();
                emit errorOccurred(tr("Input channel closed unexpectedly."));
            }
        });
        stdinClosedChecker->start(1000);
    }

    QFile m_stdIn;
    QSocketNotifier m_notifier;
};

class WindowsStdinReader : public StdinReader
{
public:
    WindowsStdinReader(QObject *parent) : StdinReader(parent) {}

private:
    void start() override
    {
#ifdef Q_OS_WIN32
        m_stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (!m_stdinHandle) {
            emit errorOccurred(tr("Failed to create handle for standard input."));
            return;
        }

        // A timer seems slightly less awful than to block in a thread
        // (how would we abort that one?), but ideally we'd like
        // to have a signal-based approach like in the Unix variant.
        const auto timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this, timer] {
            char buf[1024];
            DWORD bytesAvail;
            if (!PeekNamedPipe(m_stdinHandle, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
                timer->stop();
                emit errorOccurred(tr("Failed to read from input channel."));
                return;
            }
            while (bytesAvail > 0) {
                DWORD bytesRead;
                if (!ReadFile(m_stdinHandle, buf, std::min<DWORD>(bytesAvail, sizeof buf),
                              &bytesRead, nullptr)) {
                    timer->stop();
                    emit errorOccurred(tr("Failed to read from input channel."));
                    return;
                }
                emit dataAvailable(QByteArray(buf, bytesRead));
                bytesAvail -= bytesRead;
            }
        });
        timer->start(10);
#endif
    }

#ifdef Q_OS_WIN32
    HANDLE m_stdinHandle;
#endif
};

StdinReader *StdinReader::create(QObject *parent)
{
    if (HostOsInfo::isWindowsHost())
        return new WindowsStdinReader(parent);
    return new UnixStdinReader(parent);
}

StdinReader::StdinReader(QObject *parent) : QObject(parent) { }

} // namespace Internal
} // namespace qbs
