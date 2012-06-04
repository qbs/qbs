/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/
#include "inputartifactscanner.h"

#include "artifact.h"
#include "buildgraph.h"
#include "transformer.h"

#include <tools/logger.h>
#include <tools/scannerpluginmanager.h>

#include <QSet>
#include <QStringList>
#include <QVariantMap>

namespace qbs {

struct Dependency
{
    Dependency() : artifact(0) {}

    bool isValid() const { return !filePath.isNull(); }

    QString filePath;
    Artifact *artifact;
};

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
            collectIncludePaths(iterator .value().toMap().value("modules").toMap(), collectedPaths);
        }
    }
}

static QStringList collectIncludePaths(const QVariantMap &modules)
{
    QSet<QString> collectedPaths;

    collectIncludePaths(modules, &collectedPaths);
    return QStringList(collectedPaths.toList());
}

static Dependency resolveWithIncludePath(const QString &includePath, const QString &dependencyDirPath,
        bool isCleanPath, const QString &dependencyFileName, const BuildProduct *buildProduct)
{
    QString absDirPath = dependencyDirPath.isEmpty() ? includePath : FileInfo::resolvePath(includePath, dependencyDirPath);
    if (!isCleanPath)
        absDirPath = QDir::cleanPath(absDirPath);

    Dependency result;
    BuildProject *project = buildProduct->project;
    Artifact *fileDependencyArtifact = 0;
    Artifact *dependencyInProduct = 0;
    Artifact *dependencyInOtherProduct = 0;
    foreach (Artifact *artifact, project->lookupArtifacts(absDirPath, dependencyFileName)) {
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

    QString absFilePath = absDirPath + QLatin1Char('/') + dependencyFileName;
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


InputArtifactScanner::InputArtifactScanner(Artifact *artifact, ScanResultCache *scanResultCache)
    : m_artifact(artifact), m_scanResultCache(scanResultCache), m_newDependencyAdded(false)
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
        BuildGraph::disconnect(m_artifact, dependency);
    }

    QSet<Artifact*>::const_iterator it = m_artifact->transformer->inputs.begin();
    for (; it != m_artifact->transformer->inputs.end(); ++it) {
        Artifact *inputArtifact = *it;
        QStringList includePaths;

        foreach (const QString &fileTag, inputArtifact->fileTags) {
            QList<ScannerPlugin *> scanners = ScannerPluginManager::scannersForFileTag(fileTag);
            foreach (ScannerPlugin *scanner, scanners) {
                if (includePaths.isEmpty())
                    includePaths = collectIncludePaths(inputArtifact->configuration->value().value("modules").toMap());
                scanForFileDependencies(scanner, includePaths, inputArtifact);
            }
        }
    }
}

void InputArtifactScanner::scanForFileDependencies(ScannerPlugin *scannerPlugin,
        const QStringList &includePaths, Artifact *inputArtifact)
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

        ScanResultCache::Result scanResult = m_scanResultCache->value(filePathToBeScanned);
        if (!scanResult.valid) {
            bool successfulScan = scanWithScannerPlugin(scannerPlugin, filePathToBeScanned, &scanResult);
            if (!successfulScan)
                continue;
            m_scanResultCache->insert(filePathToBeScanned, scanResult);
        }

        resolveScanResultDependencies(includePaths, inputArtifact, scanResult, filePathToBeScanned,
                                      &filePathsToScan);
    }
}

void InputArtifactScanner::resolveScanResultDependencies(const QStringList &includePaths,
        const Artifact *inputArtifact, const ScanResultCache::Result &scanResult,
        const QString &filePathToBeScanned, QStringList *filePathsToScan)
{
    QString dependencyDirPath;
    QString dependencyFileName;
    QString baseDirOfInFilePath;
    foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
        const QString &dependencyFilePath = dependency.filePath;
        const bool isLocalInclude = dependency.isLocal;
        Dependency resolvedDependency;
        if (FileInfo::isAbsolute(dependencyFilePath)) {
            resolvedDependency.filePath = dependencyFilePath;
            goto resolved;
        }

        FileInfo::splitIntoDirectoryAndFileName(dependencyFilePath, &dependencyDirPath, &dependencyFileName);

        if (isLocalInclude) {
            // try base directory of source file
            if (baseDirOfInFilePath.isNull())
                baseDirOfInFilePath = FileInfo::path(filePathToBeScanned);
            resolvedDependency = resolveWithIncludePath(baseDirOfInFilePath, dependencyDirPath, dependency.isClean, dependencyFileName, inputArtifact->product);
            if (resolvedDependency.isValid())
                goto resolved;
        }

        // try include paths
        foreach (const QString &includePath, includePaths) {
            resolvedDependency = resolveWithIncludePath(includePath, dependencyDirPath, dependency.isClean, dependencyFileName, inputArtifact->product);
            if (resolvedDependency.isValid())
                goto resolved;
        }

        // we could not resolve the include
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN] unresolved '%s'", qPrintable(dependencyFilePath));
        continue;

resolved:
        filePathsToScan->append(resolvedDependency.filePath);
        handleDependency(resolvedDependency);
    }

}

void InputArtifactScanner::handleDependency(Dependency &dependency)
{
    BuildProduct *product = m_artifact->product;
    bool insertIntoProduct = true;
    Q_ASSERT(m_artifact->artifactType == Artifact::Generated);
    Q_CHECK_PTR(m_artifact->product);

    if (!dependency.artifact) {
        // The dependency is an existing file but does not exist in the build graph.
        if (qbsLogLevel(LoggerTrace))
            qbsTrace("[DEPSCAN]   + '%s'", qPrintable(dependency.filePath));
        dependency.artifact = new Artifact(m_artifact->project);
        dependency.artifact->artifactType = Artifact::FileDependency;
        dependency.artifact->configuration = m_artifact->configuration;
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
        BuildGraph *buildGraph = product->project->buildGraph();
        if (insertIntoProduct && !product->artifacts.contains(dependency.artifact))
            buildGraph->insert(product, dependency.artifact);
        buildGraph->safeConnect(m_artifact, dependency.artifact);
        m_newDependencyAdded = true;
    }
}

} // namespace qbs
