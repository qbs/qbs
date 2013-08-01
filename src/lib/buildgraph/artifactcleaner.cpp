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
#include "projectbuilddata.h"

#include <language/language.h>
#include <logging/translator.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QString>

namespace qbs {
namespace Internal {

static void printRemovalMessage(const QString &path, bool dryRun, const Logger &logger)
{
    if (dryRun)
        logger.qbsInfo() << Tr::tr("Would remove '%1'.").arg(path);
    else
        logger.qbsDebug() << "Removing '" << path << "'.";
}

static void invalidateArtifactTimestamp(Artifact *artifact)
{
    if (artifact->timestamp().isValid()) {
        artifact->clearTimestamp();
        artifact->product->topLevelProject()->buildData->isDirty = true;
    }
}

static void removeArtifactFromDisk(Artifact *artifact, bool dryRun, const Logger &logger)
{
    QFileInfo fileInfo(artifact->filePath());
    if (!fileInfo.exists()) {
        if (!dryRun)
            invalidateArtifactTimestamp(artifact);
        return;
    }
    printRemovalMessage(fileInfo.filePath(), dryRun, logger);
    if (dryRun)
        return;
    invalidateArtifactTimestamp(artifact);
    QString errorMessage;
    if (!removeFileRecursion(fileInfo, &errorMessage))
        throw ErrorInfo(errorMessage);
}

class CleanupVisitor : public ArtifactVisitor
{
public:
    CleanupVisitor(const CleanOptions &options, const Logger &logger)
        : ArtifactVisitor(Artifact::Generated)
        , m_options(options)
        , m_logger(logger)
        , m_hasError(false)
    {
    }

    void visitProduct(const ResolvedProductConstPtr &product)
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
        if (artifact->parents.isEmpty()
                && m_options.cleanType() == CleanOptions::CleanupTemporaries) {
            return;
        }
        try {
            removeArtifactFromDisk(artifact, m_options.dryRun(), m_logger);
        } catch (const ErrorInfo &error) {
            if (!m_options.keepGoing())
                throw;
            m_logger.printWarning(error);
            m_hasError = true;
        }
        m_directories << artifact->dirPath();
    }

    const CleanOptions m_options;
    Logger m_logger;
    bool m_hasError;
    ResolvedProductConstPtr m_product;
    QSet<QString> m_directories;
};

ArtifactCleaner::ArtifactCleaner(const Logger &logger, ProgressObserver *observer)
    : m_logger(logger), m_observer(observer)
{
}

void ArtifactCleaner::cleanup(const TopLevelProjectPtr &project,
        const QList<ResolvedProductPtr> &products, const CleanOptions &options)
{
    m_hasError = false;

    const QString configString = Tr::tr(" for configuration %1").arg(project->id());
    m_observer->initialize(Tr::tr("Cleaning up%1").arg(configString), products.count() + 1);

    QSet<QString> directories;
    foreach (const ResolvedProductPtr &product, products) {
        CleanupVisitor visitor(options, m_logger);
        visitor.visitProduct(product);
        directories.unite(visitor.directories());
        if (visitor.hasError())
            m_hasError = true;
        m_observer->incrementProgressValue();
    }

    // Directories created during the build are not artifacts (TODO: should they be?),
    // so we have to clean them up manually.
    foreach (const QString &dir, directories) {
        if (FileInfo(dir).exists())
            removeEmptyDirectories(dir, options);
    }
    m_observer->incrementProgressValue();

    if (m_hasError)
        throw ErrorInfo(Tr::tr("Failed to remove some files."));
    m_observer->setFinished();
}

void ArtifactCleaner::removeEmptyDirectories(const QString &rootDir, const CleanOptions &options,
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
        printRemovalMessage(rootDir, options.dryRun(), m_logger);
        if (!QDir::root().rmdir(rootDir)) {
            ErrorInfo error(Tr::tr("Failure to remove empty directory '%1'.").arg(rootDir));
            if (!options.keepGoing())
                throw error;
            m_logger.printWarning(error);
            m_hasError = true;
            subTreeIsEmpty = false;
        }
    }
    if (!subTreeIsEmpty && isEmpty)
        *isEmpty = false;
}

} // namespace Internal
} // namespace qbs
