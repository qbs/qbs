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
#include "emptydirectoriesremover.h"

#include "artifact.h"

#include <language/language.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

namespace qbs {
namespace Internal {

EmptyDirectoriesRemover::EmptyDirectoriesRemover(const TopLevelProject *project,
                                                 Logger logger)
    : m_project(project), m_logger(std::move(logger))
{
}

void EmptyDirectoriesRemover::removeEmptyParentDirectories(const QStringList &artifactFilePaths)
{
    m_dirsToRemove.clear();
    m_handledDirs.clear();
    for (const QString &filePath : artifactFilePaths)
        insertSorted(QFileInfo(filePath).absolutePath());
    while (!m_dirsToRemove.empty())
        removeDirIfEmpty();
}

void EmptyDirectoriesRemover::removeEmptyParentDirectories(const ArtifactSet &artifacts)
{
    QStringList filePaths;
    for (const Artifact * const a : artifacts) {
        if (a->artifactType == Artifact::Generated)
            filePaths << a->filePath();
    }
    removeEmptyParentDirectories(filePaths);
}

// List is sorted so that "deeper" directories come first.
void EmptyDirectoriesRemover::insertSorted(const QString &dirPath)
{
    int i;
    for (i = 0; i < m_dirsToRemove.size(); ++i) {
        const QString &cur = m_dirsToRemove.at(i);
        if (dirPath == cur)
            return;
        if (dirPath.count(QLatin1Char('/')) > cur.count(QLatin1Char('/')))
            break;
    }
    m_dirsToRemove.insert(i, dirPath);
}

void EmptyDirectoriesRemover::removeDirIfEmpty()
{
    const QString dirPath = m_dirsToRemove.takeFirst();
    m_handledDirs.insert(dirPath);
    QFileInfo fi(dirPath);
    if (fi.isSymLink() || !fi.exists() || !dirPath.startsWith(m_project->buildDirectory)
            || fi.filePath() == m_project->buildDirectory) {
        return;
    }
    QDir dir(dirPath);
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    if (dir.count() != 0)
        return;
    dir.cdUp();
    if (!dir.rmdir(fi.fileName())) {
        m_logger.qbsWarning() << QStringLiteral("Cannot remove empty directory '%1'.")
                                 .arg(dirPath);
        return;
    }
    const QString parentDir = dir.path();
    if (!m_handledDirs.contains(parentDir))
        insertSorted(parentDir);
}

} // namespace Internal
} // namespace qbs
