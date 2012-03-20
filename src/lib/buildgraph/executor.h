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

#ifndef BUILDGRAPHEXECUTOR_H
#define BUILDGRAPHEXECUTOR_H

#include "buildgraph.h"

#include <buildgraph/artifact.h>
#include <Qbs/processoutput.h>
#include <tools/settings.h>
#include <tools/scannerpluginmanager.h>
#include <buildgraph/scanresultcache.h>

#include <QtCore/QObject>
#include <QtCore/QVariant>

namespace qbs {

class AutoMoc;
class ExecutorJob;
class ScanResultCache;

class Executor : public QObject
{
    Q_OBJECT
public:
    Executor(int maxJobs = 1);
    ~Executor();

    void build(const QList<BuildProject::Ptr> projectsToBuild, const QStringList &changedFiles, const QStringList &selectedProductNames, QFutureInterface<bool> &futureInterface);

    enum ExecutorState {
        ExecutorIdle,
        ExecutorRunning,
        ExecutorError
    };

    enum BuildResult {
        SuccessfulBuild,
        FailedBuild
    };

    void setRunOnceAndForgetModeEnabled(bool enabled) { m_runOnceAndForgetMode = enabled; }
    void setDryRun(bool b);
    void setKeepGoing(bool b) { m_keepGoing = b; }
    bool isKeepGoingSet() const { return m_keepGoing; }
    ExecutorState state() const { return m_state; }
    BuildResult buildResult() const { return m_buildResult; }

    void setMaximumJobs(int numberOfJobs);
    int maximumJobs() const;

signals:
    void error();
    void finished();
    void stateChanged(ExecutorState);
    void progress(int jobsToDo, int jobCount, const QString &description);

protected slots:
    void onProcessError(QString errorString);
    void onProcessSuccess();
    void resetArtifactsToUntouched();

protected:
    void prepareBuildGraph(Artifact::BuildState buildState);
    void prepareBuildGraph_impl(Artifact *artifact, Artifact::BuildState buildState);
    void updateBuildGraph(Artifact::BuildState buildState);
    void updateBuildGraph_impl(Artifact *artifact, Artifact::BuildState buildState, QSet<Artifact *> &seenArtifacts);
    void doOutOfDateCheck();
    void doOutOfDateCheck(Artifact *root);
    void doDependencyScanTopDown();
    void doDependencyScan_impl(Artifact *artifact, QSet<Artifact *> &seenArtifacts);
    static bool isLeaf(Artifact *artifact);
    void initLeaves(const QList<Artifact *> &changedArtifacts);
    void initLeavesTopDown(Artifact *artifact, QSet<Artifact *> &seenArtifacts);
    bool run(QFutureInterface<bool> &futureInterface);
    qbs::FileTime timeStamp(Artifact *artifact);
    bool isOutOfDate(Artifact *artifact, bool &fileExists);
    void execute(Artifact *artifact);
    void finishArtifact(Artifact *artifact);
    void finish();
    void setState(ExecutorState);
    void setError(const QString &errorMessage);
    void addExecutorJobs(int jobNumber);
    void removeExecutorJobs(int jobNumber);
    bool runAutoMoc();
    void scanFileDependencies(Artifact *processedArtifact);
    void scanForFileDependencies(ScannerPlugin *scannerPlugin, const QStringList &includePaths, Artifact *processedArtifact, Artifact *inputArtifact);
    static void resolveScanResultDependencies(const QStringList &includePaths, Artifact *inputArtifact, const ScanResultCache::Result &scanResult,
                                              const QString &filePathToBeScanned, QSet<QString> *dependencies, QStringList *filePathsToScan);
    void handleDependencies(Artifact *processedArtifact, Artifact *scannedArtifact, const QSet<QString> &resolvedDependencies);
    void handleDependency(Artifact *processedArtifact, const QString &dependencyFilePath);
    void cancelJobs();

private:
    QScriptEngine *m_scriptEngine;
    bool m_runOnceAndForgetMode;    // This is true for the command line version.
    QList<ExecutorJob*> m_availableJobs;
    QHash<ExecutorJob*, Artifact *> m_processingJobs;
    ExecutorState m_state;
    BuildResult m_buildResult;
    bool m_keepGoing;
    QList<BuildProject::Ptr> m_projectsToBuild;
    QList<BuildProduct::Ptr> m_productsToBuild;
    QList<Artifact *> m_roots;
    QMap<Artifact *, QHashDummyValue> m_leaves;
    QHash<Artifact *, qbs::FileTime> m_timeStampCache;
    ScanResultCache m_scanResultCache;
    AutoMoc *m_autoMoc;

    friend class ExecutorJob;
    int m_maximumJobNumber;
    QFutureInterface<bool> *m_futureInterface; // TODO: its a hack
};

} // namespace qbs

#endif // BUILDGRAPHEXECUTOR_H
