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
#include <buildgraph/scanresultcache.h>

#include <QtCore/QObject>
#include <QtCore/QVariant>

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
    Executor(int maxJobs = 1);
    ~Executor();

    void build(const QList<BuildProject::Ptr> projectsToBuild, const QStringList &changedFiles, const QStringList &selectedProductNames);
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

    void setProgressObserver(ProgressObserver *observer) { m_progressObserver = observer; }
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
    QScriptEngine *m_scriptEngine;
    ProgressObserver *m_progressObserver;
    bool m_runOnceAndForgetMode;    // This is true for the command line version.
    QList<ExecutorJob*> m_availableJobs;
    QHash<ExecutorJob*, Artifact *> m_processingJobs;
    ExecutorState m_state;
    BuildResult m_buildResult;
    bool m_keepGoing;
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
    int m_maximumJobNumber;
};

} // namespace qbs

#endif // BUILDGRAPHEXECUTOR_H
