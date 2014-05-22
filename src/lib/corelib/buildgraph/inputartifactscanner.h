/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QBS_INPUTARTIFACTSCANNER_H
#define QBS_INPUTARTIFACTSCANNER_H

#include "scanresultcache.h"
#include <language/forward_decls.h>
#include <logging/logger.h>

#include <QHash>
#include <QStringList>

class ScannerPlugin;

namespace qbs {
namespace Internal {

class Artifact;
class FileResourceBase;
class PropertyMapInternal;

class DependencyScanner;
typedef QSharedPointer<DependencyScanner> DependencyScannerPtr;

class ResolvedDependency
{
public:
    ResolvedDependency()
        : file(0)
    {}

    bool isValid() const { return !filePath.isNull(); }

    QString filePath;
    FileResourceBase *file;
};

class InputArtifactScannerContext
{
public:
    InputArtifactScannerContext(ScanResultCache *scanResultCache);
    ~InputArtifactScannerContext();

private:
    ScanResultCache *scanResultCache;

    struct ResolvedDependencyCacheItem
    {
        ResolvedDependencyCacheItem()
            : valid(false)
        {}

        bool valid;
        ResolvedDependency resolvedDependency;
    };

    typedef QHash<QString, QHash<QString, ResolvedDependencyCacheItem> > ResolvedDependenciesCache;

    struct ScannerResolvedDependenciesCache
    {
        ScannerResolvedDependenciesCache() :
            valid(false)
        {}

        bool valid;
        QStringList searchPaths;
        ResolvedDependenciesCache resolvedDependenciesCache;
    };

    struct DependencyScannerCacheItem
    {
        DependencyScannerCacheItem();
        ~DependencyScannerCacheItem();

        bool valid;
        QList<DependencyScannerPtr> scanners;
    };

    typedef QHash<void *, ScannerResolvedDependenciesCache> CacheItem;

    QHash<PropertyMapConstPtr, CacheItem> cache;
    QHash<ResolvedProduct*, QHash<FileTag, DependencyScannerCacheItem> > scannersCache;

    friend class InputArtifactScanner;
};

class InputArtifactScanner
{
public:
    InputArtifactScanner(Artifact *artifact, InputArtifactScannerContext *ctx,
                         const Logger &logger);
    void scan();
    bool newDependencyAdded() const { return m_newDependencyAdded; }

private:
    void scanForFileDependencies(Artifact *inputArtifact);
    QSet<DependencyScanner *> scannersForArtifact(const Artifact *artifact) const;
    void scanForScannerFileDependencies(DependencyScanner *scanner,
            Artifact *inputArtifact, FileResourceBase *fileToBeScanned,
            QList<FileResourceBase *> *filesToScan,
            InputArtifactScannerContext::ScannerResolvedDependenciesCache &cache);
    void resolveScanResultDependencies(const Artifact *inputArtifact,
            const ScanResultCache::Result &scanResult, QList<FileResourceBase *> *artifactsToScan,
            InputArtifactScannerContext::ScannerResolvedDependenciesCache &cache);
    void handleDependency(ResolvedDependency &dependency);

    Artifact * const m_artifact;
    InputArtifactScannerContext *const m_context;
    bool m_newDependencyAdded;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_INPUTARTIFACTSCANNER_H
