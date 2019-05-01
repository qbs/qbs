/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QBS_QBSPROCESS_H
#define QBS_QBSPROCESS_H

#include "launcherpackets.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>

namespace qbs {
namespace Internal {

class QbsProcess : public QObject
{
    Q_OBJECT
public:
    explicit QbsProcess(QObject *parent = nullptr);

    QProcess::ProcessState state() const { return m_state; }
    void setProcessEnvironment(const QProcessEnvironment &env) { m_environment = env; }
    void setWorkingDirectory(const QString &workingDir) { m_workingDirectory = workingDir; }
    QString workingDirectory() const { return m_workingDirectory; }
    void start(const QString &command, const QStringList &arguments);
    void cancel();
    QByteArray readAllStandardOutput();
    QByteArray readAllStandardError();
    int exitCode() const { return m_exitCode; }
    QProcess::ProcessError error() const { return m_error; }
    QString errorString() const { return m_errorString; }

signals:
    void error(QProcess::ProcessError error);
    void finished(int exitCode);

private:
    void doStart();
    void sendPacket(const LauncherPacket &packet);
    QByteArray readAndClear(QByteArray &data);

    void handleSocketError(const QString &message);
    void handlePacket(qbs::Internal::LauncherPacketType type, quintptr token,
                      const QByteArray &payload);
    void handleErrorPacket(const QByteArray &packetData);
    void handleFinishedPacket(const QByteArray &packetData);
    void handleSocketReady();

    quintptr token() const { return reinterpret_cast<quintptr>(this); }

    QString m_command;
    QStringList m_arguments;
    QProcessEnvironment m_environment;
    QString m_workingDirectory;
    QByteArray m_stdout;
    QByteArray m_stderr;
    QString m_errorString;
    QProcess::ProcessError m_error = QProcess::UnknownError;
    QProcess::ProcessState m_state = QProcess::NotRunning;
    int m_exitCode = 0;
    int m_connectionAttempts = 0;
    bool m_socketError = false;
};

} // namespace Internal
} // namespace qbs

#endif // QBSPROCESS_H
