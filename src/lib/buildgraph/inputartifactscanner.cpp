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

#include "inputartifactscanner.h"

#include "artifact.h"
#include "buildgraph.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "transformer.h"

#include <language/language.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

namespace qbs {
namespace Internal {

InputArtifactScannerContext::InputArtifactScannerContext(ScanResultCache *scanResultCache)
    : scanResultCache(scanResultCache)
{
}

InputArtifactScannerContext::~InputArtifactScannerContext()
{
}

static void collectIncludePaths(const QVariantMap &modules, QSet<QString> *collectedPaths)
{
    QMapIterator<QString, QVariant> iterator(modules);
    while (iterator.hasNext()) {
        iterator.next();
        if (iterator.key() == "cpp") {
            QVariant includePathsVariant = iterator .value().toMap().value("includePaths");
            if (includePathsVariant.isValid())
                collectedPaths->unite(QSet<QString>::fromList(includePathsVariant.toStringList()));
        } else {
            collectIncludePaths(iterator.value().toMap().value("modules").toMap(), collectedPaths);
        }
    }
}

static QStringList collectIncludePaths(const QVariantMap &modules)
{
    QSet<QString> collectedPaths;

    collectIncludePaths(modules, &collectedPaths);
    return QStringList(collectedPaths.toList());
}

static void resolveWithIncludePath(const QString &includePath,
        const ScanResultCache::Dependency &dependency, const ResolvedProduct *product,
        ResolvedDependency *result)
{
    QString absDirPath = dependency.dirPath().isEmpty() ? includePath : FileInfo::resolvePath(includePath, dependency.dirPath());
    if (!dependency.isClean())
        absDirPath = QDir::cleanPath(absDirPath);

    ResolvedProject *project = product->project.data();
    Artifact *fileDependencyArtifact = 0;
    Artifact *dependencyInProduct = 0;
    Artifact *dependencyInOtherProduct = 0;
    foreach (Artifact *artifact, project->topLevelProject()
             ->buildData->lookupArtifacts(absDirPath, dependency.fileName())) {
        if (artifact->artifactType == Artifact::FileDependency)
            fileDependencyArtifact = artifact;
        else if (artifact->product == product)
            dependencyInProduct = artifact;
        else
            dependencyInOtherProduct = artifact;
    }

    // prioritize found artifacts
    if ((result->artifact = dependencyInProduct)
        || (result->artifact = dependencyInOtherProduct)
        || (result->artifact = fileDependencyArtifact))
    {
        result->filePath = result->artifact->filePath();
        return;
    }

    QString absFilePath = absDirPath + QLatin1Char('/') + dependency.fileName();
    if (FileInfo::exists(absFilePath))
        result->filePath = absFilePath;
}

static bool scanWithScannerPlugin(ScannerPlugin *scannerPlugin,
                                  const QString &filePathToBeScanned,
                                  ScanResultCache::Result *scanResult)
{
    void *scannerHandle = scannerPlugin->open(filePathToBeScanned.utf16(), 0, 0);
    if (!scannerHandle)
        return false;
    while (true) {
        int flags = 0;
        int length = 0;
        const char *szOutFilePath = scannerPlugin->next(scannerHandle, &length, &flags);
        if (szOutFilePath == 0)
            break;
        QString outFilePath = QString::fromLocal8Bit(szOutFilePath, length);
        if (outFilePath.isEmpty())
            continue;
        bool isLocalInclude = (flags & SC_LOCAL_INCLUDE_FLAG);
        scanResult->deps += ScanResultCache::Dependency(outFilePath, isLocalInclude);
    }
    scannerPlugin->close(scannerHandle);
    scanResult->valid = true;
    return true;
}


InputArtifactScanner::InputArtifactScanner(Artifact *artifact, InputArtifactScannerContext *ctx,
                                           const Logger &logger)
    : m_artifact(artifact), m_context(ctx), m_newDependencyAdded(false), m_logger(logger)
{
}

void InputArtifactScanner::scan()
{
    if (m_artifact->inputsScanned)
        return;

    m_artifact->inputsScanned = true;

    // clear file dependencies; they will be regenerated
    m_artifact->fileDependencies.clear();

    // Remove all connections to children that do not belong to our transformer.
    // They will be regenerated.
    foreach (Artifact *dependency, m_artifact->children) {
        if (m_artifact->transformer->inputs.contains(dependency))
            continue;
        disconnect(m_artifact, dependency, m_logger);
    }

    ArtifactList::const_iterator it = m_artifact->transformer->inputs.begin();
    for (; it != m_artifact->transformer->inputs.end(); ++it) {
        Artifact *inputArtifact = *it;
        QStringList includePaths;
        bool mustCollectIncludePaths = false;

        QSet<ScannerPlugin *> scanners;
        foreach (const FileTag &fileTag, inputArtifact->fileTags) {
            foreach (ScannerPlugin *scanner, ScannerPluginManager::scannersForFileTag(fileTag)) {
                scanners += scanner;
                if (scanner->flags & ScannerUsesCppIncludePaths)
                    mustCollectIncludePaths = true;
            }
        }

        InputArtifactScannerContext::CacheItem &cacheItem = m_context->cache[inputArtifact->properties];
        if (mustCollectIncludePaths) {
            if (cacheItem.valid) {
                //qDebug() << "CACHE HIT";
                includePaths = cacheItem.includePaths;
            } else {
                //qDebug() << "CACHE MISS";
                includePaths = collectIncludePaths(inputArtifact->properties->value().value("modules").toMap());
                cacheItem.includePaths = includePaths;
                cacheItem.valid = true;
            }
        }

        const QStringList emptyIncludePaths;
        foreach (ScannerPlugin *scanner, scanners) {
            scanForFileDependencies(scanner,
                                    (scanner->flags & ScannerUsesCppIncludePaths)
                                        ? includePaths : emptyIncludePaths,
                                    inputArtifact,
                                    cacheItem.resolvedDependenciesCache[scanner]);
        }
    }
}

void InputArtifactScanner::scanForFileDependencies(ScannerPlugin *scannerPlugin,
        const QStringList &includePaths, Artifact *inputArtifact, InputArtifactScannerContext::ResolvedDependenciesCache &resolvedDependenciesCache)
{
    if (m_logger.debugEnabled()) {
        m_logger.qbsDebug() << QString::fromLocal8Bit("scanning %1 [%2]\n    from %3")
                .arg(inputArtifact->filePath()).arg(scannerPlugin->fileTag)
                .arg(m_artifact->filePath());
    }

    QSet<QString> visitedFilePaths;
    QStringList filePathsToScan;
    filePathsToScan.append(inputArtifact->filePath());
    QStringList * const filePathsToScanPtr =
            (scannerPlugin->flags & ScannerRecursiveDependencies) ? &filePathsToScan : 0;

    while (!filePathsToScan.isEmpty()) {
        const QString filePathToBeScanned = filePathsToScan.takeFirst();
        if (visitedFilePaths.contains(filePathToBeScanned))
            continue;
        visitedFilePaths.insert(filePathToBeScanned);

        ScanResultCache::Result scanResult = m_context->scanResultCache->value(filePathToBeScanned);
        if (!scanResult.valid) {
            bool successfulScan = scanWithScannerPlugin(scannerPlugin, filePathToBeScanned, &scanResult);
            if (!successfulScan)
                continue;
            m_context->scanResultCache->insert(filePathToBeScanned, scanResult);
        }

        resolveScanResultDependencies(includePaths, inputArtifact, scanResult, filePathToBeScanned,
                                      filePathsToScanPtr, resolvedDependenciesCache);
    }
}

void InputArtifactScanner::resolveScanResultDependencies(const QStringList &includePaths,
        const Artifact *inputArtifact, const ScanResultCache::Result &scanResult,
        const QString &filePathToBeScanned, QStringList *filePathsToScan, InputArtifactScannerContext::ResolvedDependenciesCache &resolvedDependenciesCache)
{
    QString baseDirOfInFilePath;
    foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
        const QString &dependencyFilePath = dependency.filePath();
        ResolvedDependency pristineResolvedDependency;
        ResolvedDependency *resolvedDependency = &pristineResolvedDependency;
        InputArtifactScannerContext::ResolvedDependencyCacheItem *cachedResolvedDependencyItem = 0;

        if (FileInfo::isAbsolute(dependencyFilePath)) {
            resolvedDependency->filePath = dependencyFilePath;
            goto resolved;
        }

        if (dependency.isLocal()) {
            // try base directory of source file
            if (baseDirOfInFilePath.isNull())
                baseDirOfInFilePath = FileInfo::path(filePathToBeScanned);
            resolveWithIncludePath(baseDirOfInFilePath, dependency, inputArtifact->product.data(),
                                   resolvedDependency);
            if (resolvedDependency->isValid())
                goto resolved;
        }

        cachedResolvedDependencyItem = &resolvedDependenciesCache[dependency.fileName()][dependency.dirPath()];
        resolvedDependency = &cachedResolvedDependencyItem->resolvedDependency;
        if (cachedResolvedDependencyItem->valid) {
//            qDebug() << "RESCACHE HIT" << dependency.filePath();
            if (resolvedDependency->filePath.isEmpty())
                goto unresolved;
            goto resolved;
        }
//        qDebug() << "RESCACHE MISS";
        cachedResolvedDependencyItem->valid = true;

        // try include paths
        foreach (const QString &includePath, includePaths) {
            resolveWithIncludePath(includePath, dependency, inputArtifact->product.data(),
                                   resolvedDependency);
            if (resolvedDependency->isValid())
                goto resolved;
        }

unresolved:
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << QString::fromLocal8Bit("[DEPSCAN] unresolved '%1'")
                                   .arg(dependencyFilePath);
        }
        continue;

resolved:
        // Do not scan artifacts that are being built. Otherwise we might read an incomplete
        // file or conflict with the writing process.
        if (filePathsToScan
                && (!resolvedDependency->artifact
                    || resolvedDependency->artifact->buildState != Artifact::Building)) {
            filePathsToScan->append(resolvedDependency->filePath);
        }
        handleDependency(*resolvedDependency);
    }
}

void InputArtifactScanner::handleDependency(ResolvedDependency &dependency)
{
    const ResolvedProductPtr product = m_artifact->product;
    bool insertIntoProduct = true;
    QBS_CHECK(m_artifact->artifactType == Artifact::Generated);
    QBS_CHECK(product);

    if (!dependency.artifact) {
        // The dependency is an existing file but does not exist in the build graph.
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << QString::fromLocal8Bit("[DEPSCAN]   + '%1'")
                                   .arg(dependency.filePath);
        }
        dependency.artifact = new Artifact(m_artifact->project);
        dependency.artifact->artifactType = Artifact::FileDependency;
        dependency.artifact->properties = m_artifact->properties;
        dependency.artifact->setFilePath(dependency.filePath);
        m_artifact->topLevelProject()->buildData->insertFileDependency(dependency.artifact);
    } else if (dependency.artifact->artifactType == Artifact::FileDependency) {
        // The dependency exists in the project's list of file dependencies.
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << QString::fromLocal8Bit("[DEPSCAN]  ok in deps '%1'")
                                   .arg(dependency.filePath);
        }
    } else if (dependency.artifact->product == product) {
        // The dependency is in our product.
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << QString::fromLocal8Bit("[DEPSCAN]  ok in product '%1'")
                                   .arg(dependency.filePath);
        }
        insertIntoProduct = false;
    } else {
        // The dependency is in some other product.
        ResolvedProduct * const otherProduct = dependency.artifact->product;
        if (m_logger.traceEnabled()) {
            m_logger.qbsTrace() << QString::fromLocal8Bit("[DEPSCAN]  found in product '%1': '%2'")
                                   .arg(otherProduct->name, dependency.filePath);
        }
        insertIntoProduct = false;
    }

    if (m_artifact == dependency.artifact)
        return;

    if (dependency.artifact->artifactType == Artifact::FileDependency) {
        m_artifact->fileDependencies.insert(dependency.artifact);
    } else {
        if (m_artifact->children.contains(dependency.artifact))
            return;
        if (insertIntoProduct && !product->buildData->artifacts.contains(dependency.artifact))
            insertArtifact(product, dependency.artifact, m_logger);
        safeConnect(m_artifact, dependency.artifact, m_logger);
        m_newDependencyAdded = true;
    }
}

} // namespace Internal
} // namespace qbs
