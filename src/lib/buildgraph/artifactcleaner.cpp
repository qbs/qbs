/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "artifactcleaner.h"

#include "artifact.h"
#include "artifactvisitor.h"
#include "buildproduct.h"
#include "buildproject.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <language/language.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/buildoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QString>

namespace qbs {
namespace Internal {

static void printRemovalMessage(const QString &path, bool dryRun)
{
    if (dryRun)
        qbsInfo() << Tr::tr("Would remove '%1'.").arg(path);
    else
        qbsDebug() << "Removing '" << path << "'.";
}

static void invalidateArtifactTimestamp(Artifact *artifact)
{
    if (artifact->timestamp.isValid()) {
        artifact->timestamp.clear();
        artifact->project->markDirty();
    }
}

static void removeArtifactFromDisk(Artifact *artifact, bool dryRun)
{
    QFileInfo fileInfo(artifact->filePath());
    if (!fileInfo.exists()) {
        if (!dryRun)
            invalidateArtifactTimestamp(artifact);
        return;
    }
    printRemovalMessage(fileInfo.filePath(), dryRun);
    if (dryRun)
        return;
    invalidateArtifactTimestamp(artifact);
    QString errorMessage;
    if (!removeFileRecursion(fileInfo, &errorMessage))
        throw Error(errorMessage);
}

class CleanupVisitor : public ArtifactVisitor
{
public:
    CleanupVisitor(bool stopOnError, bool dryRun, bool removeAll)
        : ArtifactVisitor(Artifact::Generated)
        , m_stopOnError(stopOnError)
        , m_dryRun(dryRun)
        , m_removeAll(removeAll)
        , m_hasError(false)
    {
    }

    void visitProduct(const BuildProductConstPtr &product)
    {
        m_product = product;
        ArtifactVisitor::visitProduct(product);
    }

    const QSet<QString> &directories() const { return m_directories; }
    bool hasError() const { return m_hasError; }

private:
    void doVisit(Artifact *artifact)
    {
        if (artifact->product != m_product)
            return;
        if (artifact->parents.isEmpty() && !m_removeAll)
            return;
        try {
            removeArtifactFromDisk(artifact, m_dryRun);
        } catch (const Error &error) {
            if (m_stopOnError)
                throw;
            qbsWarning() << error.toString();
            m_hasError = true;
        }
        m_directories << artifact->dirPath();
    }

    const bool m_stopOnError;
    const bool m_dryRun;
    const bool m_removeAll;
    bool m_hasError;
    BuildProductConstPtr m_product;
    QSet<QString> m_directories;
};

void ArtifactCleaner::cleanup(const QList<BuildProductPtr> &products, bool removeAll,
                              const BuildOptions &buildOptions)
{
    m_hasError = false;
    TimedActivityLogger logger(QLatin1String("Cleaning up"));

    QSet<QString> directories;
    foreach (const BuildProductConstPtr &product, products) {
        CleanupVisitor visitor(!buildOptions.keepGoing, buildOptions.dryRun, removeAll);
        visitor.visitProduct(product);
        directories.unite(visitor.directories());
        if (visitor.hasError())
            m_hasError = true;
    }

    // Directories created during the build are not artifacts (TODO: should they be?),
    // so we have to clean them up manually.
    foreach (const QString &dir, directories) {
        if (FileInfo(dir).exists())
            removeEmptyDirectories(dir, buildOptions);
    }

    if (m_hasError)
        throw Error(Tr::tr("Failed to remove some files."));
}

void ArtifactCleaner::removeEmptyDirectories(const QString &rootDir, const BuildOptions &options,
                                             bool *isEmpty)
{
    bool subTreeIsEmpty = true;
    QDirIterator it(rootDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        if (!it.fileInfo().isSymLink() && it.fileInfo().isDir())
            removeEmptyDirectories(it.filePath(), options, &subTreeIsEmpty);
        else
            subTreeIsEmpty = false;
    }
    if (subTreeIsEmpty) {
        printRemovalMessage(rootDir, options.dryRun);
        if (!QDir::root().rmdir(rootDir)) {
            Error error(Tr::tr("Failure to remove empty directory '%1'.").arg(rootDir));
            if (!options.keepGoing)
                throw error;
            qbsWarning() << error.toString();
            m_hasError = true;
            subTreeIsEmpty = false;
        }
    }
    if (!subTreeIsEmpty && isEmpty)
        *isEmpty = false;
}

} // namespace Internal
} // namespace qbs
