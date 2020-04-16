#include <utility>

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
#include "artifactcleaner.h"

#include "artifact.h"
#include "artifactvisitor.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "transformer.h"

#include <language/language.h>
#include <logging/translator.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstring.h>

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
        artifact->product->topLevelProject()->buildData->setDirty();
    }
}

static void removeArtifactFromDisk(Artifact *artifact, bool dryRun, const Logger &logger)
{
    QFileInfo fileInfo(artifact->filePath());
    if (!FileInfo::fileExists(fileInfo)) {
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
    CleanupVisitor(CleanOptions options, const ProgressObserver *observer,
                   Logger logger)
        : ArtifactVisitor(Artifact::Generated)
        , m_options(std::move(options))
        , m_observer(observer)
        , m_logger(std::move(logger))
        , m_hasError(false)
    {
    }

    void visitProduct(const ResolvedProductPtr &product)
    {
        m_product = product;
        ArtifactVisitor::visitProduct(product);
        const AllRescuableArtifactData rescuableArtifactData
                = product->buildData->rescuableArtifactData();
        for (auto it = rescuableArtifactData.begin(); it != rescuableArtifactData.end(); ++it) {
            Artifact tmp;
            tmp.product = product;
            tmp.setFilePath(it.key());
            tmp.setTimestamp(it.value().timeStamp);
            removeArtifactFromDisk(&tmp, m_options.dryRun(), m_logger);
            product->buildData->removeFromRescuableArtifactData(it.key());
        }
    }

    const Set<QString> &directories() const { return m_directories; }
    bool hasError() const { return m_hasError; }

private:
    void doVisit(Artifact *artifact) override
    {
        if (m_observer->canceled())
            throw ErrorInfo(Tr::tr("Cleaning up was canceled."));

        if (artifact->product != m_product)
            return;
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
    const ProgressObserver * const m_observer;
    Logger m_logger;
    bool m_hasError;
    ResolvedProductConstPtr m_product;
    Set<QString> m_directories;
};

ArtifactCleaner::ArtifactCleaner(Logger logger, ProgressObserver *observer)
    : m_logger(std::move(logger)), m_observer(observer)
{
}

void ArtifactCleaner::cleanup(const TopLevelProjectPtr &project,
        const QVector<ResolvedProductPtr> &products, const CleanOptions &options)
{
    m_hasError = false;

    const QString configString = Tr::tr(" for configuration %1").arg(project->id());
    m_observer->initialize(Tr::tr("Cleaning up%1").arg(configString), products.size() + 1);

    Set<QString> directories;
    for (const ResolvedProductPtr &product : products) {
        CleanupVisitor visitor(options, m_observer, m_logger);
        visitor.visitProduct(product);
        directories.unite(visitor.directories());
        if (visitor.hasError())
            m_hasError = true;
        m_observer->incrementProgressValue();
    }

    // Directories created during the build are not artifacts (TODO: should they be?),
    // so we have to clean them up manually.
    QList<QString> dirList = directories.toList();
    for (int i = 0; i < dirList.size(); ++i) {
        const QString &dir = dirList.at(i);
        if (!dir.startsWith(project->buildDirectory))
            continue;
        if (FileInfo(dir).exists())
            removeEmptyDirectories(dir, options);
        if (dir != project->buildDirectory) {
            const QString parentDir = QDir::cleanPath(dir + StringConstants::slashDotDot());
            if (parentDir != project->buildDirectory && !dirList.contains(parentDir))
                dirList.push_back(parentDir);
        }
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
    QDirIterator it(rootDir, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
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
