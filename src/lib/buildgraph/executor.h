/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef BUILDGRAPHEXECUTOR_H
#define BUILDGRAPHEXECUTOR_H

#include "buildgraph.h"

#include <buildgraph/artifact.h>
#include <Qbs/processoutput.h>
#include <tools/buildoptions.h>
#include <tools/settings.h>
#include <buildgraph/scanresultcache.h>

#include <QObject>
#include <QVariant>

namespace qbs {

class AutoMoc;
class ExecutorJob;
class InputArtifactScannerContext;
class ProgressObserver;
class ScanResultCache;

class Executor : public QObject
{
    Q_OBJECT
public:
    Executor();
    ~Executor();

    void build(const QList<BuildProject::Ptr> projectsToBuild);
    void cancelBuild();

    enum ExecutorState {
        ExecutorIdle,
        ExecutorRunning,
        ExecutorCanceled,
        ExecutorError
    };

    enum BuildResult {
        SuccessfulBuild,
        FailedBuild
    };

    void setEngine(QbsEngine *engine);
    void setBuildOptions(const BuildOptions &buildOptions);
    void setProgressObserver(ProgressObserver *observer) { m_progressObserver = observer; }
    ExecutorState state() const { return m_state; }
    BuildResult buildResult() const { return m_buildResult; }

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
    static bool isLeaf(Artifact *artifact);
    void initLeaves(const QList<Artifact *> &changedArtifacts);
    void initLeavesTopDown(Artifact *artifact, QSet<Artifact *> &seenArtifacts);
    bool run();
    qbs::FileTime timeStamp(Artifact *artifact);
    void execute(Artifact *artifact);
    void finishArtifact(Artifact *artifact);
    void finish();
    void setState(ExecutorState);
    void setError(const QString &errorMessage);
    void addExecutorJobs(int jobNumber);
    void removeExecutorJobs(int jobNumber);
    bool runAutoMoc();
    void printScanningMessageOnce();
    void insertLeavesAfterAddingDependencies(QVector<Artifact *> dependencies);
    void cancelJobs();

private:
    QbsEngine *m_engine;
    BuildOptions m_buildOptions;
    ProgressObserver *m_progressObserver;
    QList<ExecutorJob*> m_availableJobs;
    QHash<ExecutorJob*, Artifact *> m_processingJobs;
    ExecutorState m_state;
    BuildResult m_buildResult;
    bool m_printScanningMessage;
    QList<BuildProject::Ptr> m_projectsToBuild;
    QList<BuildProduct::Ptr> m_productsToBuild;
    QList<Artifact *> m_roots;
    QMap<Artifact *, QHashDummyValue> m_leaves;
    QHash<Artifact *, qbs::FileTime> m_timeStampCache;
    ScanResultCache m_scanResultCache;
    InputArtifactScannerContext *m_inputArtifactScanContext;
    AutoMoc *m_autoMoc;

    friend class ExecutorJob;
};

} // namespace qbs

#endif // BUILDGRAPHEXECUTOR_H
