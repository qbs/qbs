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

#include "buildgraphlocker.h"

#include "error.h"
#include "hostosinfo.h"
#include "processutils.h"
#include "progressobserver.h"
#include "stringconstants.h"

#include <logging/translator.h>

#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstring.h>

namespace qbs {
namespace Internal {

DirectoryManager::DirectoryManager(QString dir, Logger logger)
    : m_dir(std::move(dir)), m_logger(std::move(logger))
{
    rememberCreatedDirectories();
}

DirectoryManager::~DirectoryManager()
{
    removeEmptyCreatedDirectories();
}

void DirectoryManager::rememberCreatedDirectories()
{
    QString parentDir = m_dir;
    while (!QFileInfo::exists(parentDir)) {
        m_createdParentDirs.push(parentDir);
        parentDir = QDir::cleanPath(parentDir + StringConstants::slashDotDot());
    }
}

void DirectoryManager::removeEmptyCreatedDirectories()
{
    QDir root = QDir::root();
    while (!m_createdParentDirs.empty()) {
        const QString parentDir = m_createdParentDirs.front();
        m_createdParentDirs.pop();
        QDirIterator it(parentDir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System,
                        QDirIterator::Subdirectories);
        if (it.hasNext())
            break;
        if (!root.rmdir(parentDir) && m_logger.logSink()) {
            m_logger.printWarning(ErrorInfo(Tr::tr("Failed to remove empty directory '%1'.")
                                             .arg(parentDir)));
        }
    }
}

static void tryCreateBuildDirectory(const QString &buildDir, const QString &buildGraphFilePath)
{
    if (!QDir::root().mkpath(buildDir)) {
        throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1': Failed to create directory.")
                        .arg(buildGraphFilePath));
    }
}

static bool appNamesAreEqual(const QString &app1, const QString &app2)
{
    return QString::compare(app1, app2, HostOsInfo::fileNameCaseSensitivity()) == 0;
}

BuildGraphLocker::BuildGraphLocker(const QString &buildGraphFilePath, const Logger &logger,
                                   bool waitIndefinitely, ProgressObserver *observer)
    : m_lockFile(buildGraphFilePath + QStringLiteral(".lock"))
    , m_logger(logger)
    , m_dirManager(QFileInfo(buildGraphFilePath).absolutePath(), logger)
{
    if (waitIndefinitely)
        m_logger.qbsDebug() << "Waiting to acquire lock file...";
    m_lockFile.setStaleLockTime(0);
    int attemptsToGetInfo = 0;
    do {
        if (observer && observer->canceled())
            break;
        tryCreateBuildDirectory(m_dirManager.dir(), buildGraphFilePath);
        if (m_lockFile.tryLock(250))
            return;
        switch (m_lockFile.error()) {
        case QLockFile::LockFailedError: {
            if (waitIndefinitely)
                continue;
            qint64 pid;
            QString hostName;
            QString appName;
            if (m_lockFile.getLockInfo(&pid, &hostName, &appName)) {
                if (appNamesAreEqual(appName, processNameByPid(pid))) {
                    throw ErrorInfo(Tr::tr("Cannot lock build graph file '%1': "
                                           "Already locked by '%2' (PID %3).")
                                    .arg(buildGraphFilePath, appName).arg(pid));
                }

                // The process id was reused by some other process.
                m_logger.qbsInfo() << Tr::tr("Removing stale lock file.");
                m_lockFile.removeStaleLockFile();
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
    } while (++attemptsToGetInfo < 10 || waitIndefinitely);

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
