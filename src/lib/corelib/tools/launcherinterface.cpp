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

#include "launcherinterface.h"

#include "launcherpackets.h"
#include "launchersocket.h"
#include "qbsassert.h"
#include <logging/logger.h>
#include <logging/translator.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtNetwork/qlocalserver.h>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qbs {
namespace Internal {

class LauncherProcess : public QProcess
{
public:
    LauncherProcess(QObject *parent) : QProcess(parent) { }

private:
    void setupChildProcess() override
    {
#ifdef Q_OS_UNIX
        const auto pid = static_cast<pid_t>(processId());
        setpgid(pid, pid);
#endif
    }
};

static QString launcherSocketName()
{
    return QStringLiteral("qbs_processlauncher-%1")
            .arg(QString::number(qApp->applicationPid()));
}

LauncherInterface::LauncherInterface()
    : m_server(new QLocalServer(this)), m_socket(new LauncherSocket(this))
{
    QObject::connect(m_server, &QLocalServer::newConnection,
                     this, &LauncherInterface::handleNewConnection);
}

LauncherInterface &LauncherInterface::instance()
{
    static LauncherInterface p;
    return p;
}

LauncherInterface::~LauncherInterface()
{
    m_server->disconnect();
}

void LauncherInterface::doStart()
{
    if (++m_startRequests > 1)
        return;
    const QString &socketName = launcherSocketName();
    QLocalServer::removeServer(socketName);
    if (!m_server->listen(socketName)) {
        emit errorOccurred(ErrorInfo(m_server->errorString()));
        return;
    }
    m_process = new LauncherProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, &LauncherInterface::handleProcessError);
    connect(m_process,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &LauncherInterface::handleProcessFinished);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &LauncherInterface::handleProcessStderr);
    m_process->start(qApp->applicationDirPath() + QLatin1Char('/')
                            + QLatin1String(QBS_RELATIVE_LIBEXEC_PATH)
                            + QLatin1String("/qbs_processlauncher"),
                           QStringList(m_server->fullServerName()));
}

void LauncherInterface::doStop()
{
    if (--m_startRequests > 0)
        return;
    m_server->close();
    if (!m_process)
        return;
    m_process->disconnect();
    if (m_socket->isReady())
        m_socket->shutdown();
    m_process->waitForFinished(3000);
    m_process->deleteLater();
    m_process = nullptr;
}

void LauncherInterface::handleNewConnection()
{
    QLocalSocket * const socket = m_server->nextPendingConnection();
    if (!socket)
        return;
    m_server->close();
    m_socket->setSocket(socket);
}

void LauncherInterface::handleProcessError()
{
    if (m_process->error() == QProcess::FailedToStart) {
        const QString launcherPathForUser
                = QDir::toNativeSeparators(QDir::cleanPath(m_process->program()));
        emit errorOccurred(ErrorInfo(Tr::tr("Failed to start process launcher at '%1': %2")
                                     .arg(launcherPathForUser, m_process->errorString())));
    }
}

void LauncherInterface::handleProcessFinished()
{
    emit errorOccurred(ErrorInfo(Tr::tr("Process launcher closed unexpectedly: %1")
                                 .arg(m_process->errorString())));
}

void LauncherInterface::handleProcessStderr()
{
    qDebug() << "[launcher]" << m_process->readAllStandardError();
}

} // namespace Internal
} // namespace qbs
