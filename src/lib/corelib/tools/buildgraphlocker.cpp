/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "buildgraphlocker.h"

#include "error.h"

#include <logging/translator.h>

#include <QDir>
#include <QFileInfo>
#include <QString>

namespace qbs {
namespace Internal {

BuildGraphLocker::BuildGraphLocker(const QString &buildGraphFilePath)
    : m_lockFile(buildGraphFilePath + QLatin1String(".lock"))
{
    const QString buildDir = QFileInfo(buildGraphFilePath).absolutePath();
    if (!QDir::root().mkpath(buildDir)) {
        throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1': Failed to create directory.")
                        .arg(buildGraphFilePath));
    }
    m_lockFile.setStaleLockTime(0);
    int attemptsToGetInfo = 0;
    do {
        if (m_lockFile.tryLock(250))
            return;
        switch (m_lockFile.error()) {
        case QLockFile::LockFailedError: {
            qint64 pid;
            QString hostName;
            QString appName;
            if (m_lockFile.getLockInfo(&pid, &hostName, &appName)) {
                throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1': "
                        "Already locked by '%2' (PID %3).")
                                .arg(buildGraphFilePath, appName).arg(pid));
            }
            break;
        }
        case QLockFile::PermissionError:
            throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1': Permission denied.")
                            .arg(buildGraphFilePath));
        case QLockFile::UnknownError:
        case QLockFile::NoError:
            throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1' (reason unknown).")
                            .arg(buildGraphFilePath));
        }
    } while (++attemptsToGetInfo < 10);

    // This very unlikely case arises if tryLock() repeatedly returns LockFailedError
    // with the subsequent getLockInfo() failing as well.
    throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1' (reason unknown).")
                    .arg(buildGraphFilePath));
}

BuildGraphLocker::~BuildGraphLocker()
{
    m_lockFile.unlock();
}

} // namespace Internal
} // namespace qbs
