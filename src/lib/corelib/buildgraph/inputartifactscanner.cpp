/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com).
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
#include "depscanner.h"
#include "projectbuilddata.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <language/language.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/stringconstants.h>

#include <QtCore/QDir>

namespace qbs {
namespace Internal {

static void resolveDepencency(
    const RawScannedDependency &dependency,
    const ResolvedProduct *product,
    ResolvedDependency *result,
    const QString &baseDir = QString())
{
    QString absDirPath = baseDir.isEmpty() ? dependency.dirPath()
                         : dependency.dirPath().isEmpty()
                             ? baseDir
                             : FileInfo::resolvePath(baseDir, dependency.dirPath());
    if (!dependency.isClean())
        absDirPath = QDir::cleanPath(absDirPath);

    ResolvedProject *project = product->project.get();
    FileDependency *fileDependencyArtifact = nullptr;
    Artifact *dependencyInProduct = nullptr;
    Artifact *dependencyInOtherProduct = nullptr;
    bool productOfDependencyIsDependency = false;
    const auto files = project->topLevelProject()->buildData->lookupFiles(
        absDirPath, dependency.fileName());
    for (FileResourceBase *lookupResult : files) {
        switch (lookupResult->fileType()) {
        case FileResourceBase::FileTypeDependency:
            fileDependencyArtifact = static_cast<FileDependency *>(lookupResult);
            break;
        case FileResourceBase::FileTypeArtifact: {
            auto const foundArtifact = static_cast<Artifact *>(lookupResult);
            if (foundArtifact->product == product) {
                dependencyInProduct = foundArtifact;
            } else if (!productOfDependencyIsDependency) {
                dependencyInOtherProduct = foundArtifact;
                productOfDependencyIsDependency = product->hasDependency(
                    dependencyInOtherProduct->product.lock());
            }
            break;
        }
        }
        if (dependencyInProduct)
            break;
    }

    // prioritize found artifacts
    if (dependencyInProduct)
        result->file = dependencyInProduct;
    else if (dependencyInOtherProduct)
        result->file = dependencyInOtherProduct;
    else
        result->file = fileDependencyArtifact;

    if (result->file) {
        result->filePath = result->file->filePath();

        if (result->file == dependencyInOtherProduct && !productOfDependencyIsDependency) {
            qCDebug(lcDepScan) << "product" << dependencyInOtherProduct->product->fullDisplayName()
                                 << "of scanned dependency" << result->filePath
                                 << "is not a dependency of product" << product->fullDisplayName()
                                 << ". The file dependency might get lost during change tracking.";
        }

        return;
    }

    // Must be built from absDirPath, which is what we looked the file up by. Otherwise, an
    // unclean path would be stored and never found again, creating a new FileDependency on
    // every scan.
    const QString absFilePath = absDirPath + QLatin1Char('/') + dependency.fileName();

    // TODO: We probably need a flag that tells us whether directories are allowed.
    const FileInfo fi(absFilePath);
    if (fi.exists(absFilePath) && !fi.isDir())
        result->filePath = absFilePath;
}

InputArtifactScanner::InputArtifactScanner(
    Logger logger, InputArtifactScannerContext *ctx, Set<QString> excludedScanners)
    : m_logger(logger)
    , m_context(ctx)
    , m_excludedScanners(std::move(excludedScanners))
{}

/*
    Scans the dependencies for the given \a inputArtifacts.
    Returns true if at least one artifact had dependency scanners run.
*/
bool InputArtifactScanner::scan(const ArtifactSet &inputArtifacts)
{
    bool wasScanned = false;
    for (Artifact * const artifact : inputArtifacts) {
        qCDebug(lcDepScan) << "scanning" << artifact->filePath() << artifact->fileTags()
                           << "in product" << artifact->product->name;
        wasScanned = scanInputArtifact(artifact) || wasScanned;
    }
    return wasScanned;
}

/*!
    Rebuilds the dependencies of a generated \a artifact.

    Existing scanner-added edges and file dependencies are removed, new dependencies are
    discovered and connected, and the artifact's timestamp is cleared if its file
    dependency set changed.
*/
bool InputArtifactScanner::updateDependencies(Artifact *artifact)
{
    // clear file dependencies; they will be regenerated
    const auto oldFileDependencies = artifact->fileDependencies;
    artifact->fileDependencies.clear();

    // Remove all connections to children that were added by the dependency scanner.
    // They will be regenerated.
    const Set<Artifact *> childrenAddedByScanner = artifact->childrenAddedByScanner;
    artifact->childrenAddedByScanner.clear();
    for (Artifact * const dependency : childrenAddedByScanner)
        disconnect(artifact, dependency);

    for (Artifact * const inputArtifact : std::as_const(artifact->transformer->inputs)) {
        updateInputArtifactDependencies(artifact, inputArtifact);
    }

    // If file dependencies changed, invalidate the artifact's timestamp to force a rebuild.
    // This handles cases where a dependency moves to a different location (e.g., a header
    // file moves from one include directory to another).
    if (oldFileDependencies != artifact->fileDependencies) {
        artifact->clearTimestamp();
    }

    if (!artifact->childrenAddedByScanner.empty())
        return true;

    return false;
}

bool InputArtifactScanner::scanInputArtifact(Artifact *inputArtifact)
{
    qCInfo(lcDepScan) << "input artifact" << inputArtifact->filePath() << inputArtifact->fileTags();

    const auto scanners = scannersForArtifact(inputArtifact);
    if (scanners.empty())
        return false;

    const QStringList fileTags = inputArtifact->fileTags().toStringList();
    const InputArtifactScannerContext::PropertiesCacheKey propsKey{
        inputArtifact->properties, fileTags};
    InputArtifactScannerContext::ScannerKeyCache *lastPerFileCacheItem = nullptr;
    InputArtifactScannerContext::ScannerKeyCache *lastPerPropsCacheItem = nullptr;

    for (DependencyScanner * const scanner : scanners) {
        InputArtifactScannerContext::ScannerKeyCache *cacheItem;
        if (scanner->cacheIsPerFile()) {
            if (!lastPerFileCacheItem)
                lastPerFileCacheItem = &m_context->cachePerFile[inputArtifact];
            cacheItem = lastPerFileCacheItem;
        } else {
            if (!lastPerPropsCacheItem)
                lastPerPropsCacheItem = &m_context->cachePerProperties[propsKey];
            cacheItem = lastPerPropsCacheItem;
        }
        scanForScannerFileDependencies(
            scanner, inputArtifact, inputArtifact, (*cacheItem)[scanner->id()]);
    }
    return true;
}

void InputArtifactScanner::updateInputArtifactDependencies(
    Artifact *artifact, Artifact *inputArtifact)
{
    QBS_ASSERT(artifact, return);
    QBS_ASSERT(inputArtifact, return);

    const QStringList fileTags = inputArtifact->fileTags().toStringList();
    const InputArtifactScannerContext::PropertiesCacheKey propsKey{
        inputArtifact->properties, fileTags};
    const auto scanners = scannersForArtifact(inputArtifact);
    for (const auto &scanner : scanners) {
        InputArtifactScannerContext::ScannerKeyCache *cacheItem;
        if (scanner->cacheIsPerFile()) {
            cacheItem = &m_context->cachePerFile[inputArtifact];
        } else {
            cacheItem = &m_context->cachePerProperties[propsKey];
        }
        Set<QString> visitedFilePaths;
        std::deque<FileResourceBase *> filesToScan;
        filesToScan.emplace_back(inputArtifact);
        while (!filesToScan.empty()) {
            FileResourceBase *file = filesToScan.front();
            QBS_ASSERT(file, break);
            filesToScan.pop_front();

            const QString &filePathToBeScanned = file->filePath();
            if (!visitedFilePaths.insert(filePathToBeScanned).second)
                continue;

            auto &scannerCacheItem = (*cacheItem)[scanner->id()];
            const auto &scanData = scanForScannerFileDependencies(
                scanner, inputArtifact, file, scannerCacheItem);

            resolveScanResultDependencies(
                artifact,
                inputArtifact,
                scanData.rawScanResult,
                scanner->recursive() ? &filesToScan : nullptr,
                *scannerCacheItem);
        }
    }
}

Set<DependencyScanner *> InputArtifactScanner::scannersForArtifact(const Artifact *artifact) const
{
    Set<DependencyScanner *> scanners;
    ResolvedProduct *product = artifact->product.get();
    ScriptEngine *engine = product->topLevelProject()->buildData->evaluationContext->engine();
    auto &scannerCache = m_context->scannersCache[product];
    for (const FileTag &fileTag : artifact->fileTags()) {
        InputArtifactScannerContext::DependencyScannerCacheItem &cache = scannerCache[fileTag];
        if (!cache) {
            QList<DependencyScannerPtr> cacheScanners;
            for (const ResolvedScannerPtr &scanner : product->scanners) {
                if (scanner->inputs.contains(fileTag)) {
                    ScannerPlugin *plugin = nullptr;
                    if (!scanner->pluginName.isEmpty()) {
                        plugin = ScannerPluginManager::scannerByName(scanner->pluginName);
                        if (!plugin) {
                            throw ErrorInfo(
                                Tr::tr("Scanner plugin '%1' not found").arg(scanner->pluginName),
                                scanner->location);
                        }
                    }
                    cacheScanners.push_back(
                        std::make_shared<DependencyScanner>(scanner, engine, plugin));
                }
            }
            cache = std::move(cacheScanners);
        }
        for (const DependencyScannerPtr &scanner : std::as_const(*cache)) {
            if (!m_excludedScanners.contains(scanner->id()))
                scanners += scanner.get();
        }
    }
    return scanners;
}

const RawScanResults::ScanData &InputArtifactScanner::scanForScannerFileDependencies(
    DependencyScanner *scanner,
    Artifact *inputArtifact,
    FileResourceBase *fileToBeScanned,
    InputArtifactScannerContext::ScannerKeyCacheItem &cache)
{
    qCDebug(lcDepScan) << "processing file" << fileToBeScanned->filePath();

    const bool cacheHit = !!cache;
    if (!cacheHit) {
        cache.emplace();
        cache->searchPaths = scanner->collectSearchPaths(inputArtifact);
    }
    qCDebug(lcDepScan) << "include paths (cache" << (cacheHit ? "hit)" : "miss)");
    for (const QString &s : std::as_const(cache->searchPaths))
        qCDebug(lcDepScan) << "    " << s;

    const QString &filePathToBeScanned = fileToBeScanned->filePath();
    RawScanResults &rawScanResults
        = inputArtifact->product->topLevelProject()->buildData->rawScanResults;
    RawScanResults::ScanData &scanData = rawScanResults.findScanData(
        fileToBeScanned, scanner, inputArtifact->properties);
    if (scanData.lastScanTime < fileToBeScanned->timestamp()) {
        qCDebug(lcDepScan) << "scanning" << FileInfo::fileName(filePathToBeScanned);
        scanWithScannerPlugin(scanner, inputArtifact, fileToBeScanned, &scanData.rawScanResult);
        scanData.lastScanTime = FileTime::currentTime();
    }
    return scanData;
}

void InputArtifactScanner::resolveScanResultDependencies(
    Artifact *artifact,
    const Artifact *inputArtifact,
    const RawScanResult &scanResult,
    std::deque<FileResourceBase *> *artifactsToScan,
    InputArtifactScannerContext::ScannerKeyCacheData &cache)
{
    auto getResolvedDependency =
        [inputArtifact, &cache](const RawScannedDependency &dependency) -> ResolvedDependency * {
        const QString &dependencyFilePath = dependency.filePath();
        auto &dirCache = cache.resolvedDependenciesCache[dependency.dirPath()];
        auto &cachedResolvedDependencyItem = dirCache[dependency.fileName()];
        if (cachedResolvedDependencyItem) {
            ResolvedDependency &resolvedDependency = *cachedResolvedDependencyItem;
            if (!resolvedDependency.filePath.isEmpty())
                return &resolvedDependency;
        }
        ResolvedDependency &resolvedDependency = cachedResolvedDependencyItem.emplace();

        if (FileInfo::isAbsolute(dependencyFilePath)) {
            resolveDepencency(dependency, inputArtifact->product.get(), &resolvedDependency);
            if (resolvedDependency.filePath.isEmpty())
                return nullptr;
            return &resolvedDependency;
        }

        // try include paths
        for (const QString &includePath : std::as_const(cache.searchPaths)) {
            resolveDepencency(
                dependency, inputArtifact->product.get(), &resolvedDependency, includePath);
            if (resolvedDependency.isValid())
                return &resolvedDependency;
        }
        return nullptr;
    };

    for (const RawScannedDependency &dependency : scanResult.deps) {
        const auto maybeResolvedDependency = getResolvedDependency(dependency);
        if (!maybeResolvedDependency) {
            qCWarning(lcDepScan) << "unresolved dependency " << dependency.filePath();
            continue;
        }
        auto &resolvedDependency = *maybeResolvedDependency;

        handleDependency(artifact, resolvedDependency);

        if (artifactsToScan && resolvedDependency.file) {
            if (resolvedDependency.file->fileType() == FileResourceBase::FileTypeArtifact) {
                // Do not scan an artifact that is not built yet: Its contents might still change.
                auto const artifactDependency = static_cast<Artifact *>(resolvedDependency.file);
                if (artifactDependency->artifactType == Artifact::SourceFile
                        || artifactDependency->buildState == BuildGraphNode::Built) {
                    artifactsToScan->push_back(artifactDependency);
                }
            } else {
                // Add file dependency to the next round of scanning.
                artifactsToScan->push_back(resolvedDependency.file);
            }
        }
    }
}

void InputArtifactScanner::handleDependency(Artifact *artifact, ResolvedDependency &dependency)
{
    const ResolvedProductPtr product = artifact->product.lock();
    QBS_CHECK(artifact->artifactType == Artifact::Generated);
    QBS_CHECK(product);

    Artifact *artifactDependency = nullptr;
    FileDependency *fileDependency = nullptr;
    if (dependency.file) {
        switch (dependency.file->fileType()) {
        case FileResourceBase::FileTypeArtifact:
            artifactDependency = static_cast<Artifact *>(dependency.file);
            break;
        case FileResourceBase::FileTypeDependency:
            fileDependency = static_cast<FileDependency *>(dependency.file);
            break;
        }
    }
    QBS_CHECK(!dependency.file || artifactDependency || fileDependency);

    if (!dependency.file) {
        // The dependency is an existing file but does not exist in the build graph.
        qCDebug(lcDepScan) << "add new file dependency" << dependency.filePath;

        fileDependency = new FileDependency();
        dependency.file = fileDependency;
        fileDependency->setFilePath(dependency.filePath);
        product->topLevelProject()->buildData->insertFileDependency(fileDependency);
    } else if (fileDependency) {
        // The dependency exists in the project's list of file dependencies.
        qCDebug(lcDepScan) << "add existing file dependency" << dependency.filePath;
    } else if (artifactDependency->product == product) {
        // The dependency is in our product.
        qCDebug(lcDepScan) << "add artifact dependency" << dependency.filePath
                           << "(from this product)";
    } else {
        // The dependency is in some other product.
        ResolvedProduct * const otherProduct = artifactDependency->product;
        qCDebug(lcDepScan) << "add artifact dependency" << dependency.filePath << " (from product"
                           << otherProduct->uniqueName() << ')';
    }

    if (artifact == dependency.file)
        return;
    if (artifactDependency && artifactDependency->transformer == artifact->transformer)
        return;

    if (fileDependency) {
        artifact->fileDependencies << fileDependency;
        if (!fileDependency->timestamp().isValid())
            fileDependency->setTimestamp(FileInfo(fileDependency->filePath()).lastModified());
    } else {
        if (artifact->children.contains(artifactDependency))
            return;
        if (safeConnect(artifact, artifactDependency))
            artifact->childrenAddedByScanner += artifactDependency;
    }
}

void InputArtifactScanner::scanWithScannerPlugin(
    DependencyScanner *scanner,
    Artifact *inputArtifact,
    FileResourceBase *fileToBeScanned,
    RawScanResult *scanResult)
{
    scanResult->deps.clear();
    scanResult->scannerProperties.clear();
    const auto fileTagsForScanner
        = inputArtifact->fileTags().toStringList().join(QLatin1Char(',')).toLatin1();
    // It would be nice to return searchPaths from the scan() too, but due to different caching
    // rules we cannot use it here, at least for now. The problem is that for the cpp scanner,
    // we cache searchPaths per properties, but the scan() is run when the artifact is changed,
    // so searchPaths won't be requested if file is not changed. Maybe simply running scan script
    // on properties change (!cacheHit in the method above) will fix that.
    const auto scannerResult = scanner->collectScanResult(
        inputArtifact, fileToBeScanned, fileTagsForScanner.constData());
    scanResult->scannerProperties = scannerResult.scannerProperties;
    for (const QString &s : scannerResult.dependencies)
        scanResult->deps.emplace_back(s);
}

} // namespace Internal
} // namespace qbs
