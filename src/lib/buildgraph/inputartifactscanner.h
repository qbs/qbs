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
#ifndef INPUTARTIFACTSCANNER_H
#define INPUTARTIFACTSCANNER_H

#include "scanresultcache.h"

#include <QString>

struct ScannerPlugin;

namespace qbs {
class Artifact;
struct Dependency;

class InputArtifactScanner
{
public:
    InputArtifactScanner(Artifact *artifact, ScanResultCache *scanResultCache);
    void scan();
    bool newDependencyAdded() const { return m_newDependencyAdded; }

private:
    void scanForFileDependencies(ScannerPlugin *scannerPlugin, const QStringList &includePaths,
            Artifact *inputArtifact);
    void resolveScanResultDependencies(const QStringList &includePaths,
            const Artifact *inputArtifact, const ScanResultCache::Result &scanResult,
            const QString &filePathToBeScanned, QStringList *filePathsToScan);
    void handleDependency(Dependency &dependency);

    Artifact * const m_artifact;
    ScanResultCache * const m_scanResultCache;
    bool m_newDependencyAdded;
};

} // namespace qbs

#endif // INPUTARTIFACTSCANNER_H
