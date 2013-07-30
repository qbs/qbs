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

#ifndef QBS_INPUTARTIFACTSCANNER_H
#define QBS_INPUTARTIFACTSCANNER_H

#include "scanresultcache.h"
#include <language/forward_decls.h>
#include <logging/logger.h>

#include <QHash>
#include <QStringList>

struct ScannerPlugin;

namespace qbs {
namespace Internal {

class Artifact;
class FileResourceBase;
class PropertyMapInternal;

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

    struct CacheItem
    {
        CacheItem()
            : valid(false)
        {}

        bool valid;
        QStringList includePaths;
        QHash<ScannerPlugin *, ResolvedDependenciesCache> resolvedDependenciesCache;
    };

    QHash<PropertyMapConstPtr, CacheItem> cache;

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
    void scanForFileDependencies(ScannerPlugin *scannerPlugin, const QStringList &includePaths,
            Artifact *inputArtifact, InputArtifactScannerContext::ResolvedDependenciesCache &cacheItem);
    void resolveScanResultDependencies(const QStringList &includePaths,
            const Artifact *inputArtifact, const ScanResultCache::Result &scanResult,
            const QString &filePathToBeScanned, QStringList *filePathsToScan, InputArtifactScannerContext::ResolvedDependenciesCache &cacheItem);
    void handleDependency(ResolvedDependency &dependency);

    Artifact * const m_artifact;
    InputArtifactScannerContext *const m_context;
    bool m_newDependencyAdded;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_INPUTARTIFACTSCANNER_H
