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

#include "inputartifactscanner.h"

#include "artifact.h"
#include "buildgraph.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "transformer.h"
#include "depscanner.h"
#include "rulesevaluationcontext.h"

#include <language/language.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>
#include <tools/qbsassert.h>
#include <tools/error.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

static void resolveDepencency(const RawScannedDependency &dependency,
                              const ResolvedProduct *product, ResolvedDependency *result,
                              const QString &baseDir = QString())
{
    QString absDirPath = baseDir.isEmpty()
            ? dependency.dirPath()
            : dependency.dirPath().isEmpty()
              ? baseDir : FileInfo::resolvePath(baseDir, dependency.dirPath());
    if (!dependency.isClean())
        absDirPath = QDir::cleanPath(absDirPath);

    ResolvedProject *project = product->project.get();
    FileDependency *fileDependencyArtifact = 0;
    Artifact *dependencyInProduct = 0;
    Artifact *dependencyInOtherProduct = 0;
    for (FileResourceBase *lookupResult : project->topLevelProject()
             ->buildData->lookupFiles(absDirPath, dependency.fileName())) {
        if ((fileDependencyArtifact = dynamic_cast<FileDependency *>(lookupResult)))
            continue;
        Artifact * const foundArtifact = dynamic_cast<Artifact *>(lookupResult);
        if (foundArtifact->product == product) {
            dependencyInProduct = foundArtifact;
            break;
        }
        dependencyInOtherProduct = foundArtifact;
    }

    // prioritize found artifacts
    if ((result->file = dependencyInProduct)
        || (result->file = dependencyInOtherProduct)
        || (result->file = fileDependencyArtifact)) {
        result->filePath = result->file->filePath();
        return;
    }

    const QString &absFilePath = baseDir.isEmpty()
            ? dependency.filePath()
            : absDirPath + QLatin1Char('/') + dependency.fileName();

    // TODO: We probably need a flag that tells us whether directories are allowed.
    const FileInfo fi(absFilePath);
    if (fi.exists(absFilePath) && !fi.isDir())
        result->filePath = absFilePath;
}

InputArtifactScanner::InputArtifactScanner(Artifact *artifact, InputArtifactScannerContext *ctx,
                                           const Logger &logger)
    : m_artifact(artifact),
      m_rawScanResults(artifact->product->topLevelProject()->buildData->rawScanResults),
      m_context(ctx),
      m_newDependencyAdded(false),
      m_logger(logger)
{
}

void InputArtifactScanner::scan()
{
    if (m_artifact->inputsScanned)
        return;

    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace()
                << QString::fromLatin1("[DEPSCAN] inputs for %1 [%2] in product '%3'")
                   .arg(m_artifact->filePath(),
                        m_artifact->fileTags().toStringList().join(QLatin1String(", ")),
                        m_artifact->product->name);
    }

    m_artifact->inputsScanned = true;

    // clear file dependencies; they will be regenerated
    m_artifact->fileDependencies.clear();

    // Remove all connections to children that were added by the dependency scanner.
    // They will be regenerated.
    const Set<Artifact *> childrenAddedByScanner = m_artifact->childrenAddedByScanner;
    m_artifact->childrenAddedByScanner.clear();
    for (Artifact * const dependency : childrenAddedByScanner)
        disconnect(m_artifact, dependency, m_logger);

    for (Artifact * const inputArtifact : qAsConst(m_artifact->transformer->inputs))
        scanForFileDependencies(inputArtifact);
}

void InputArtifactScanner::scanForFileDependencies(Artifact *inputArtifact)
{
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace()
                << QString::fromLatin1("[DEPSCAN] input artifact %1 [%2]")
                   .arg(inputArtifact->filePath(),
                        inputArtifact->fileTags().toStringList().join(QLatin1String(", ")));
    }

    InputArtifactScannerContext::CacheItem &cacheItem = m_context->cache[inputArtifact->properties];
    Set<QString> visitedFilePaths;
    QList<FileResourceBase *> filesToScan;
    filesToScan.append(inputArtifact);
    const Set<DependencyScanner *> scanners = scannersForArtifact(inputArtifact);
    if (scanners.isEmpty())
        return;
    m_fileTagsForScanner
            = inputArtifact->fileTags().toStringList().join(QLatin1Char(',')).toLatin1();
    while (!filesToScan.isEmpty()) {
        FileResourceBase *fileToBeScanned = filesToScan.takeFirst();
        const QString &filePathToBeScanned = fileToBeScanned->filePath();
        if (!visitedFilePaths.insert(filePathToBeScanned).second)
            continue;

        for (DependencyScanner * const scanner : scanners) {
            scanForScannerFileDependencies(scanner, inputArtifact, fileToBeScanned,
                scanner->recursive() ? &filesToScan : 0, cacheItem[scanner->key()]);
        }
    }
}

Set<DependencyScanner *> InputArtifactScanner::scannersForArtifact(const Artifact *artifact) const
{
    Set<DependencyScanner *> scanners;
    ResolvedProduct *product = artifact->product.get();
    ScriptEngine *engine = product->topLevelProject()->buildData->evaluationContext->engine();
    QHash<FileTag, InputArtifactScannerContext::DependencyScannerCacheItem> &scannerCache
            = m_context->scannersCache[product];
    for (const FileTag &fileTag : artifact->fileTags()) {
        InputArtifactScannerContext::DependencyScannerCacheItem &cache = scannerCache[fileTag];
        if (!cache.valid) {
            cache.valid = true;
            for (ScannerPlugin *scanner : ScannerPluginManager::scannersForFileTag(fileTag)) {
                PluginDependencyScanner *pluginScanner = new PluginDependencyScanner(scanner);
                cache.scanners += DependencyScannerPtr(pluginScanner);
            }
            for (const ResolvedScannerConstPtr &scanner : qAsConst(product->scanners)) {
                if (scanner->inputs.contains(fileTag)) {
                    cache.scanners += DependencyScannerPtr(
                                new UserDependencyScanner(scanner, m_logger, engine));
                    break;
                }
            }
        }
        for (const DependencyScannerPtr &scanner : qAsConst(cache.scanners))
            scanners += scanner.get();
    }
    return scanners;
}

void InputArtifactScanner::scanForScannerFileDependencies(DependencyScanner *scanner,
        Artifact *inputArtifact, FileResourceBase *fileToBeScanned,
        QList<FileResourceBase *> *filesToScan,
        InputArtifactScannerContext::ScannerResolvedDependenciesCache &cache)
{
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << QString::fromLatin1("[DEPSCAN] file %1")
                .arg(fileToBeScanned->filePath());
    }

    const bool cacheHit = cache.valid;
    if (!cacheHit) {
        cache.valid = true;
        cache.searchPaths = scanner->collectSearchPaths(inputArtifact);
    }
    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace()
                << "[DEPSCAN] include paths (cache " << (cacheHit ? "hit)" : "miss)");
        for (const QString &s : qAsConst(cache.searchPaths))
            m_logger.qbsTrace() << "    " << s;
    }

    const QString &filePathToBeScanned = fileToBeScanned->filePath();
    RawScanResults::ScanData &scanData = m_rawScanResults.findScanData(fileToBeScanned, scanner,
                                                                       m_artifact->properties);
    if (scanData.lastScanTime < fileToBeScanned->timestamp()) {
        try {
            if (m_logger.traceEnabled())
                m_logger.qbsTrace() << "scanning " << FileInfo::fileName(filePathToBeScanned);
            scanWithScannerPlugin(scanner, fileToBeScanned, &scanData.rawScanResult);
            scanData.lastScanTime = FileTime::currentTime();
        } catch (const ErrorInfo &error) {
            m_logger.printWarning(error);
            return;
        }
    }

    resolveScanResultDependencies(inputArtifact, scanData.rawScanResult, filesToScan, cache);
}

void InputArtifactScanner::resolveScanResultDependencies(const Artifact *inputArtifact,
        const RawScanResult &scanResult, QList<FileResourceBase *> *artifactsToScan,
        InputArtifactScannerContext::ScannerResolvedDependenciesCache &cache)
{
    for (const RawScannedDependency &dependency : scanResult.deps) {
        const QString &dependencyFilePath = dependency.filePath();
        InputArtifactScannerContext::ResolvedDependencyCacheItem &cachedResolvedDependencyItem
                = cache.resolvedDependenciesCache[dependency.dirPath()][dependency.fileName()];
        ResolvedDependency &resolvedDependency = cachedResolvedDependencyItem.resolvedDependency;
        if (cachedResolvedDependencyItem.valid) {
            if (resolvedDependency.filePath.isEmpty())
                goto unresolved;
            goto resolved;
        }
        cachedResolvedDependencyItem.valid = true;

        if (FileInfo::isAbsolute(dependencyFilePath)) {
            resolveDepencency(dependency, inputArtifact->product.get(), &resolvedDependency);
            goto resolved;
        }

        // try include paths
        for (const QString &includePath : cache.searchPaths) {
            resolveDepencency(dependency, inputArtifact->product.get(),
                              &resolvedDependency, includePath);
            if (resolvedDependency.isValid())
                goto resolved;
        }

unresolved:
        if (m_logger.traceEnabled())
            m_logger.qbsWarning() << "[DEPSCAN] unresolved dependency " << dependencyFilePath;
        continue;

resolved:
        handleDependency(resolvedDependency);
        if (artifactsToScan && resolvedDependency.file) {
            if (Artifact *artifactDependency = dynamic_cast<Artifact *>(resolvedDependency.file)) {
                // Do not scan an artifact that is not built yet: Its contents might still change.
                if (artifactDependency->artifactType == Artifact::SourceFile
                        || artifactDependency->buildState == BuildGraphNode::Built) {
                    artifactsToScan->append(artifactDependency);
                }
            } else {
                // Add file dependency to the next round of scanning.
                artifactsToScan->append(resolvedDependency.file);
            }
        }
    }
}

void InputArtifactScanner::handleDependency(ResolvedDependency &dependency)
{
    const ResolvedProductPtr product = m_artifact->product.lock();
    QBS_CHECK(m_artifact->artifactType == Artifact::Generated);
    QBS_CHECK(product);

    Artifact *artifactDependency = dynamic_cast<Artifact *>(dependency.file);
    FileDependency *fileDependency
            = artifactDependency ? 0 : dynamic_cast<FileDependency *>(dependency.file);
    QBS_CHECK(!dependency.file || artifactDependency || fileDependency);

    if (!dependency.file) {
        // The dependency is an existing file but does not exist in the build graph.
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "[DEPSCAN] add new file dependency " << dependency.filePath;

        fileDependency = new FileDependency();
        dependency.file = fileDependency;
        fileDependency->setFilePath(dependency.filePath);
        product->topLevelProject()->buildData->insertFileDependency(fileDependency);
    } else if (fileDependency) {
        // The dependency exists in the project's list of file dependencies.
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << "[DEPSCAN] add existing file dependency "
                                << dependency.filePath;
        }
    } else if (artifactDependency->product == product) {
        // The dependency is in our product.
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << "[DEPSCAN] add artifact dependency " << dependency.filePath
                                << " (from this product)";
        }
    } else {
        // The dependency is in some other product.
        ResolvedProduct * const otherProduct = artifactDependency->product;
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << "[DEPSCAN] add artifact dependency " << dependency.filePath
                                << " (from product " << otherProduct->uniqueName() << ')';
        }
    }

    if (m_artifact == dependency.file)
        return;

    if (fileDependency) {
        m_artifact->fileDependencies << fileDependency;
        if (!fileDependency->timestamp().isValid())
            fileDependency->setTimestamp(FileInfo(fileDependency->filePath()).lastModified());
    } else {
        if (m_artifact->children.contains(artifactDependency))
            return;
        safeConnect(m_artifact, artifactDependency, m_logger);
        m_artifact->childrenAddedByScanner += artifactDependency;
        m_newDependencyAdded = true;
    }
}

void InputArtifactScanner::scanWithScannerPlugin(DependencyScanner *scanner,
                                                 FileResourceBase *fileToBeScanned,
                                                 RawScanResult *scanResult)
{
    scanResult->deps.clear();
    const QStringList &dependencies
            = scanner->collectDependencies(fileToBeScanned, m_fileTagsForScanner.constData());
    for (const QString &s : dependencies)
        scanResult->deps.push_back(RawScannedDependency(s));
}

InputArtifactScannerContext::DependencyScannerCacheItem::DependencyScannerCacheItem() : valid(false)
{
}

InputArtifactScannerContext::DependencyScannerCacheItem::~DependencyScannerCacheItem()
{
}

} // namespace Internal
} // namespace qbs
