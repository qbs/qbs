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
#include "emptydirectoriesremover.h"

#include "artifact.h"

#include <language/language.h>

#include <QDir>
#include <QFileInfo>

namespace qbs {
namespace Internal {

EmptyDirectoriesRemover::EmptyDirectoriesRemover(const TopLevelProject *project,
                                                 const Logger &logger)
    : m_project(project), m_logger(logger)
{
}

void EmptyDirectoriesRemover::removeEmptyParentDirectories(const QStringList &artifactFilePaths)
{
    m_dirsToRemove.clear();
    m_handledDirs.clear();
    foreach (const QString &filePath, artifactFilePaths)
        insertSorted(QFileInfo(filePath).absolutePath());
    while (!m_dirsToRemove.isEmpty())
        removeDirIfEmpty();
}

void EmptyDirectoriesRemover::removeEmptyParentDirectories(const ArtifactSet &artifacts)
{
    QStringList filePaths;
    foreach (const Artifact * const a, artifacts) {
        if (a->artifactType == Artifact::Generated)
            filePaths << a->filePath();
    }
    removeEmptyParentDirectories(filePaths);
}

// List is sorted so that "deeper" directories come first.
void EmptyDirectoriesRemover::insertSorted(const QString &dirPath)
{
    int i;
    for (i = 0; i < m_dirsToRemove.count(); ++i) {
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
        m_logger.qbsWarning() << QString::fromLocal8Bit("Cannot remove empty directory '%1'.")
                                 .arg(dirPath);
        return;
    }
    const QString parentDir = dir.path();
    if (!m_handledDirs.contains(parentDir))
        insertSorted(parentDir);
}

} // namespace Internal
} // namespace qbs
