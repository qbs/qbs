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
#include "buildproduct.h"
#include "buildproject.h"
#include "rulesevaluationcontext.h"
#include "transformer.h"

#include <language/language.h>
#include <logging/logger.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>

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

static ResolvedDependency resolveWithIncludePath(const QString &includePath, const ScanResultCache::Dependency &dependency, const BuildProduct *buildProduct)
{
    QString absDirPath = dependency.dirPath().isEmpty() ? includePath : FileInfo::resolvePath(includePath, dependency.dirPath());
    if (!dependency.isClean())
        absDirPath = QDir::cleanPath(absDirPath);

    ResolvedDependency result;
    BuildProject *project = buildProduct->project;
    Artifact *fileDependencyArtifact = 0;
    Artifact *dependencyInProduct = 0;
    Artifact *dependencyInOtherProduct = 0;
    foreach (Artifact *artifact, project->lookupArtifacts(absDirPath, dependency.fileName())) {
        if (artifact->artifactType == Artifact::FileDependency)
            fileDependencyArtifact = artifact;
        else if (artifact->product == buildProduct)
            dependencyInProduct = artifact;
        else
            dependencyInOtherProduct = artifact;
    }

    // prioritize found artifacts
    if ((result.artifact = dependencyInProduct)
        || (result.artifact = dependencyInOtherProduct)
        || (result.artifact = fileDependencyArtifact))
    {
        result.filePath = result.artifact->filePath();
        return result;
    }

    QString absFilePath = absDirPath + QLatin1Char('/') + dependency.fileName();
    if (FileInfo::exists(absFilePath))
        result.filePath = absFilePath;

    return result;
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


InputArtifactScanner::InputArtifactScanner(Artifact *artifact, InputArtifactScannerContext *ctx)
    : m_artifact(artifact), m_context(ctx), m_newDependencyAdded(false)
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
        disconnect(m_artifact, dependency);
    }

    ArtifactList::const_iterator it = m_artifact->transformer->inputs.begin();
    for (; it != m_artifact->transformer->inputs.end(); ++it) {
        Artifact *inputArtifact = *it;
        QStringList includePaths;
        bool mustCollectIncludePaths = false;

        QSet<ScannerPlugin *> scanners;
        foreach (const QString &fileTag, inputArtifact->fileTags) {
            foreach (ScannerPlugin *scanner, ScannerPluginManager::scannersForFileTag(fileTag)) {
                scanners += scanner;
                if (scanner->usesCppIncludePaths)
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
                                    scanner->usesCppIncludePaths ? includePaths : emptyIncludePaths,
                                    inputArtifact,
                                    cacheItem.resolvedDependenciesCache[scanner]);
        }
    }
}

void InputArtifactScanner::scanForFileDependencies(ScannerPlugin *scannerPlugin,
        const QStringList &includePaths, Artifact *inputArtifact, InputArtifactScannerContext::ResolvedDependenciesCache &resolvedDependenciesCache)
{
    if (qbsLogLevel(LoggerDebug)) {
        qbsDebug("scanning %s [%s]", qPrintable(inputArtifact->filePath()), scannerPlugin->fileTag);
        qbsDebug("    from %s", qPrintable(m_artifact->filePath()));
    }

    QSet<QString> visitedFilePaths;
    QStringList filePathsToScan;
    filePathsToScan.append(inputArtifact->filePath());

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
                                      &filePathsToScan, resolvedDependenciesCache);
    }
}

void InputArtifactScanner::resolveScanResultDependencies(const QStringList &includePaths,
        const Artifact *inputArtifact, const ScanResultCache::Result &scanResult,
        const QString &filePathToBeScanned, QStringList *filePathsToScan, InputArtifactScannerContext::ResolvedDependenciesCache &resolvedDependenciesCache)
{
    QString baseDirOfInFilePath;
    foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
        const QString &dependencyFilePath = dependency.filePath();
        ResolvedDependency resolvedDependency;
        InputArtifactScannerContext::ResolvedDependencyCacheItem *cachedResolvedDependencyItem = 0;

        if (FileInfo::isAbsolute(dependencyFilePath)) {
            resolvedDependency.filePath = dependencyFilePath;
            goto resolved;
        }

        if (dependency.isLocal()) {
            // try base directory of source file
            if (baseDirOfInFilePath.isNull())
                baseDirOfInFilePath = FileInfo::path(filePathToBeScanned);
            resolvedDependency = resolveWithIncludePath(baseDirOfInFilePath, dependency, inputArtifact->product);
            if (resolvedDependency.isValid())
                goto resolved;
        }

        cachedResolvedDependencyItem = &resolvedDependenciesCache[dependency.fileName()][dependency.dirPath()];
        if (cachedResolvedDependencyItem->valid) {
//            qDebug() << "RESCACHE HIT" << dependency.filePath();
            resolvedDependency = cachedResolvedDependencyItem->resolvedDependency;
            if (resolvedDependency.filePath.isEmpty())
                goto unresolved;
            goto resolved;
        }
//        qDebug() << "RESCACHE MISS";

        // try include paths
        foreach (const QString &includePath, includePaths) {
            resolvedDependency = resolveWithIncludePath(includePath, dependency, inputArtifact->product);
            if (resolvedDependency.isValid()) {
                cachedResolvedDependencyItem->valid = true;
                cachedResolvedDependencyItem->resolvedDependency = resolvedDependency;
                goto resolved;
            }
        }

        // we could not resolve the include
        cachedResolvedDependencyItem->valid = true;
        cachedResolvedDependencyItem->resolvedDependency = resolvedDependency;
unresolved:
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN] unresolved '%s'", qPrintable(dependencyFilePath));
        continue;

resolved:
        // Do not scan artifacts that are being built. Otherwise we might read an incomplete
        // file or conflict with the writing process.
        if (!resolvedDependency.artifact
                || resolvedDependency.artifact->buildState != Artifact::Building) {
            filePathsToScan->append(resolvedDependency.filePath);
        }
        handleDependency(resolvedDependency);
    }
}

void InputArtifactScanner::handleDependency(ResolvedDependency &dependency)
{
    BuildProduct *product = m_artifact->product;
    bool insertIntoProduct = true;
    Q_ASSERT(m_artifact->artifactType == Artifact::Generated);
    Q_ASSERT(m_artifact->product);

    if (!dependency.artifact) {
        // The dependency is an existing file but does not exist in the build graph.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]   + '%s'", qPrintable(dependency.filePath));
        dependency.artifact = new Artifact(m_artifact->project);
        dependency.artifact->artifactType = Artifact::FileDependency;
        dependency.artifact->properties = m_artifact->properties;
        dependency.artifact->setFilePath(dependency.filePath);
        m_artifact->project->insertFileDependency(dependency.artifact);
    } else if (dependency.artifact->artifactType == Artifact::FileDependency) {
        // The dependency exists in the project's list of file dependencies.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]  ok in deps '%s'", qPrintable(dependency.filePath));
    } else if (dependency.artifact->product == product) {
        // The dependency is in our product.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]  ok in product '%s'", qPrintable(dependency.filePath));
        insertIntoProduct = false;
    } else {
        // The dependency is in some other product.
        BuildProduct *otherProduct = dependency.artifact->product;
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]  found in product '%s': '%s'", qPrintable(otherProduct->rProduct->name), qPrintable(dependency.filePath));
        insertIntoProduct = false;
    }

    if (m_artifact == dependency.artifact)
        return;

    if (dependency.artifact->artifactType == Artifact::FileDependency) {
        m_artifact->fileDependencies.insert(dependency.artifact);
    } else {
        if (m_artifact->children.contains(dependency.artifact))
            return;
        if (insertIntoProduct && !product->artifacts.contains(dependency.artifact))
            product->insertArtifact(dependency.artifact);
        safeConnect(m_artifact, dependency.artifact);
        m_newDependencyAdded = true;
    }
}

} // namespace Internal
} // namespace qbs
