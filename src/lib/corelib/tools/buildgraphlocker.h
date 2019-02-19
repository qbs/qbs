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

#ifndef QBS_BUILDGRAPHLOCKER_H
#define QBS_BUILDGRAPHLOCKER_H

#include <logging/logger.h>

#include <QtCore/qlockfile.h>
#include <QtCore/qstring.h>

#include <queue>

namespace qbs {
namespace Internal {

class ProgressObserver;

class DirectoryManager
{
public:
    DirectoryManager(QString dir, Logger logger);
    ~DirectoryManager();
    QString dir() const { return m_dir; }

private:
    void rememberCreatedDirectories();
    void removeEmptyCreatedDirectories();

    std::queue<QString> m_createdParentDirs;
    const QString m_dir;
    Logger m_logger;
};

class BuildGraphLocker
{
public:
    explicit BuildGraphLocker(const QString &buildGraphFilePath, const Logger &logger,
                              bool waitIndefinitely, ProgressObserver *observer);
    ~BuildGraphLocker();

private:
    QLockFile m_lockFile;
    Logger m_logger;
    DirectoryManager m_dirManager;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
