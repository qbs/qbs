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

#ifndef QBS_INPUTARTIFACTSCANNER_H
#define QBS_INPUTARTIFACTSCANNER_H

#include <buildgraph/forward_decls.h>
#include <buildgraph/rawscanresults.h>
#include <logging/logger.h>
#include <tools/set.h>

#include <deque>

namespace qbs {
namespace Internal {

class Artifact;

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
    using ResolvedDependencyCacheItem = std::optional<ResolvedDependency>;
    using ResolvedDependenciesCache
        = QHash<QString /*dirName*/, QHash<QString /*fileName*/, ResolvedDependencyCacheItem>>;

    struct ScannerKeyCacheData
    {
        QStringList searchPaths;
        ResolvedDependenciesCache resolvedDependenciesCache;
    };

    using ScannerKeyCacheItem = std::optional<ScannerKeyCacheData>;
    using ScannerKeyCache = QHash<QString /*id*/, ScannerKeyCacheItem>;

    std::unordered_map<PropertyMapConstPtr, ScannerKeyCache> cachePerProperties;

    std::unordered_map<Artifact *, ScannerKeyCache> cachePerFile;

    using DependencyScannerCacheItem = std::optional<QList<DependencyScannerPtr>>;
    QHash<ResolvedProduct*, QHash<FileTag, DependencyScannerCacheItem>> scannersCache;

    friend class InputArtifactScanner;
};

class InputArtifactScanner
{
public:
    explicit InputArtifactScanner(Logger logger, InputArtifactScannerContext *ctx);

    bool scan(const ArtifactSet &inputArtifacts);
    bool updateDependencies(Artifact *artifact);

private:
    bool scanInputArtifact(Artifact *inputArtifact);
    void updateInputArtifactDependencies(Artifact *artifact, Artifact *inputArtifact);
    Set<DependencyScanner *> scannersForArtifact(const Artifact *inputArtifact) const;
    const RawScanResults::ScanData &scanForScannerFileDependencies(
        DependencyScanner *scanner,
        Artifact *inputArtifact,
        FileResourceBase *fileToBeScanned,
        InputArtifactScannerContext::ScannerKeyCacheItem &cache);
    void resolveScanResultDependencies(
        Artifact *artifact,
        const Artifact *inputArtifact,
        const RawScanResult &scanResult,
        std::deque<FileResourceBase *> *artifactsToScan,
        InputArtifactScannerContext::ScannerKeyCacheData &cache);
    void handleDependency(Artifact *artifact, ResolvedDependency &dependency);
    void scanWithScannerPlugin(
        DependencyScanner *scanner,
        Artifact *inputArtifact,
        FileResourceBase *fileToBeScanned,
        RawScanResult *scanResult);

private:
    Logger m_logger;
    InputArtifactScannerContext *m_context;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_INPUTARTIFACTSCANNER_H
