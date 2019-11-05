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

#ifndef QBS_INPUTARTIFACTSCANNER_H
#define QBS_INPUTARTIFACTSCANNER_H

#include <language/filetags.h>
#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/set.h>

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>

class ScannerPlugin;

namespace qbs {
namespace Internal {

class Artifact;
class FileResourceBase;
class RawScanResult;
class RawScanResults;
class PropertyMapInternal;

class DependencyScanner;
using DependencyScannerPtr = std::shared_ptr<DependencyScanner>;

class ResolvedDependency
{
public:
    bool isValid() const { return !filePath.isNull(); }

    QString filePath;
    FileResourceBase *file = nullptr;
};

class InputArtifactScannerContext
{
    struct ResolvedDependencyCacheItem
    {
        ResolvedDependencyCacheItem()
            : valid(false)
        {}

        bool valid;
        ResolvedDependency resolvedDependency;
    };

    using ResolvedDependenciesCache = QHash<QString, QHash<QString, ResolvedDependencyCacheItem>>;

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

    using CacheItem = QHash<const void *, ScannerResolvedDependenciesCache>;

    QHash<PropertyMapConstPtr, CacheItem> cachePerProperties;
    QHash<Artifact *, CacheItem> cachePerFile;
    QHash<ResolvedProduct*, QHash<FileTag, DependencyScannerCacheItem>> scannersCache;

    friend class InputArtifactScanner;
};

class InputArtifactScanner
{
public:
    InputArtifactScanner(Artifact *artifact, InputArtifactScannerContext *ctx,
                         Logger logger);
    void scan();
    bool newDependencyAdded() const { return m_newDependencyAdded; }

private:
    void scanForFileDependencies(Artifact *inputArtifact);
    Set<DependencyScanner *> scannersForArtifact(const Artifact *artifact) const;
    void scanForScannerFileDependencies(DependencyScanner *scanner,
            Artifact *inputArtifact, FileResourceBase *fileToBeScanned,
            QList<FileResourceBase *> *filesToScan,
            InputArtifactScannerContext::ScannerResolvedDependenciesCache &cache);
    void resolveScanResultDependencies(const Artifact *inputArtifact,
            const RawScanResult &scanResult, QList<FileResourceBase *> *artifactsToScan,
            InputArtifactScannerContext::ScannerResolvedDependenciesCache &cache);
    void handleDependency(ResolvedDependency &dependency);
    void scanWithScannerPlugin(DependencyScanner *scanner, Artifact *inputArtifact,
                               FileResourceBase *fileToBeScanned, RawScanResult *scanResult);

    Artifact * const m_artifact;
    RawScanResults &m_rawScanResults;
    InputArtifactScannerContext *const m_context;
    QByteArray m_fileTagsForScanner;
    bool m_newDependencyAdded;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_INPUTARTIFACTSCANNER_H
