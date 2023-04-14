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
#include <QtCore/qthread.h>
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
#ifdef Q_OS_WIN32
    class FileReaderThread : public QThread
    {
    public:
        FileReaderThread(WindowsStdinReader &parent, HANDLE stdInHandle, HANDLE exitEventHandle)
            : QThread(&parent), m_stdIn{stdInHandle}, m_exitEvent{exitEventHandle} { }
        ~FileReaderThread()
        {
            wait();
            CloseHandle(m_exitEvent);
        }

        void run() override
        {
            WindowsStdinReader *r = static_cast<WindowsStdinReader *>(parent());

            char buf[1024];
            while (true) {
                DWORD bytesRead = 0;
                if (!ReadFile(m_stdIn, buf, sizeof buf, &bytesRead, nullptr)) {
                    emit r->errorOccurred(tr("Failed to read from input channel."));
                    break;
                }
                if (!bytesRead)
                    break;
                emit r->dataAvailable(QByteArray(buf, bytesRead));
            }
        }
    private:
        HANDLE m_stdIn;
        HANDLE m_exitEvent;
    };

    class ConsoleReaderThread : public QThread
    {
    public:
        ConsoleReaderThread(WindowsStdinReader &parent, HANDLE stdInHandle, HANDLE exitEventHandle)
            : QThread(&parent), m_stdIn{stdInHandle}, m_exitEvent{exitEventHandle} { }
        virtual ~ConsoleReaderThread() override
        {
            SetEvent(m_exitEvent);
            wait();
            CloseHandle(m_exitEvent);
        }

        void run() override
        {
            WindowsStdinReader *r = static_cast<WindowsStdinReader *>(parent());

            DWORD origConsoleMode;
            GetConsoleMode(m_stdIn, &origConsoleMode);
            DWORD consoleMode = ENABLE_PROCESSED_INPUT;
            SetConsoleMode(m_stdIn, consoleMode);

            HANDLE handles[2] = {m_exitEvent, m_stdIn};
            char buf[1024];
            while (true) {
                auto result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                if (result == WAIT_OBJECT_0)
                    break;
                INPUT_RECORD consoleInput;
                DWORD inputsRead = 0;
                if (!PeekConsoleInputA(m_stdIn, &consoleInput, 1, &inputsRead)) {
                    emit r->errorOccurred(tr("Failed to read from input channel."));
                    break;
                }
                if (inputsRead) {
                    if (consoleInput.EventType != KEY_EVENT
                            || !consoleInput.Event.KeyEvent.bKeyDown
                            || !consoleInput.Event.KeyEvent.uChar.AsciiChar) {
                        if (!ReadConsoleInputA(m_stdIn, &consoleInput, 1, &inputsRead)) {
                            emit r->errorOccurred(tr("Failed to read console input."));
                            break;
                        }
                    } else {
                        DWORD bytesRead = 0;
                        if (!ReadConsoleA(m_stdIn, buf, sizeof buf, &bytesRead, nullptr)) {
                            emit r->errorOccurred(tr("Failed to read console."));
                            break;
                        }
                        emit r->dataAvailable(QByteArray(buf, bytesRead));
                    }
                }
            }
            SetConsoleMode(m_stdIn, origConsoleMode);
        }
    private:
        HANDLE m_stdIn;
        HANDLE m_exitEvent;
    };

    class PipeReaderThread : public QThread
    {
    public:
        PipeReaderThread(WindowsStdinReader &parent, HANDLE stdInHandle, HANDLE exitEventHandle)
            : QThread(&parent), m_stdIn{stdInHandle}, m_exitEvent{exitEventHandle} { }
        virtual ~PipeReaderThread() override
        {
            SetEvent(m_exitEvent);
            wait();
            CloseHandle(m_exitEvent);
        }

        void run() override
        {
            WindowsStdinReader *r = static_cast<WindowsStdinReader *>(parent());

            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEventA(NULL, TRUE, TRUE, NULL);
            if (!overlapped.hEvent) {
                emit r->errorOccurred(StdinReader::tr("Failed to create handle for overlapped event."));
                return;
            }

            char buf[1024];
            DWORD bytesRead;
            HANDLE handles[2] = {m_exitEvent, overlapped.hEvent};
            while (true) {
                bytesRead = 0;
                auto readResult = ReadFile(m_stdIn, buf, sizeof buf, NULL, &overlapped);
                if (!readResult) {
                    if (GetLastError() != ERROR_IO_PENDING) {
                        emit r->errorOccurred(StdinReader::tr("ReadFile Failed."));
                        break;
                    }

                    auto result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                    if (result == WAIT_OBJECT_0)
                        break;
                }
                if (!GetOverlappedResult(m_stdIn, &overlapped, &bytesRead, FALSE)) {
                    if (GetLastError() != ERROR_HANDLE_EOF)
                        emit r->errorOccurred(StdinReader::tr("Error GetOverlappedResult."));
                    break;
                }
                emit r->dataAvailable(QByteArray(buf, bytesRead));
            }
            CancelIo(m_stdIn);
            CloseHandle(overlapped.hEvent);
        }
    private:
        HANDLE m_stdIn;
        HANDLE m_exitEvent;
    };
#endif

    void start() override
    {
#ifdef Q_OS_WIN32
        HANDLE stdInHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (!stdInHandle) {
            emit errorOccurred(StdinReader::tr("Failed to create handle for standard input."));
            return;
        }
        HANDLE exitEventHandle = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (!exitEventHandle) {
            emit errorOccurred(StdinReader::tr("Failed to create handle for exit event."));
            return;
        }

        auto result = GetFileType(stdInHandle);
        switch (result) {
        case FILE_TYPE_CHAR:
            (new ConsoleReaderThread(*this, stdInHandle, exitEventHandle))->start();
            return;
        case FILE_TYPE_PIPE:
            (new PipeReaderThread(*this, stdInHandle, exitEventHandle))->start();
            return;
        case FILE_TYPE_DISK:
            (new FileReaderThread(*this, stdInHandle, exitEventHandle))->start();
            return;
        default:
            emit errorOccurred(StdinReader::tr("Unable to handle unknown input type"));
            return;
        }
#endif
    }
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
