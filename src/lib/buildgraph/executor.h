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

#ifndef QBS_BUILDGRAPHEXECUTOR_H
#define QBS_BUILDGRAPHEXECUTOR_H

#include "forward_decls.h"
#include <buildgraph/artifact.h>
#include <buildgraph/scanresultcache.h>
#include <tools/buildoptions.h>
#include <tools/error.h>
#include <tools/settings.h>

#include <QObject>
#include <QProcessEnvironment>
#include <QVariant>

namespace qbs {
class ProcessResult;

namespace Internal {
class AutoMoc;
class ExecutorJob;
class InputArtifactScannerContext;
class ProgressObserver;
class ScanResultCache;

class Executor : public QObject
{
    Q_OBJECT
public:
    Executor(QObject *parent = 0);
    ~Executor();

    void build(const QList<BuildProductPtr> &productsToBuild);

    void setBuildOptions(const BuildOptions &buildOptions);
    void setProgressObserver(ProgressObserver *observer) { m_progressObserver = observer; }
    void setBaseEnvironment(const QProcessEnvironment &env) { m_baseEnvironment = env; }

    Error error() const { return m_error; }
    bool hasError() const { return !error().entries().isEmpty(); }

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);
    void reportWarning(const qbs::Error &warning);

    void finished();

private slots:
    void onProcessError(const qbs::Error &err);
    void onProcessSuccess();
    void finish();

private:
    enum ExecutorState { ExecutorIdle, ExecutorRunning, ExecutorCanceling };

    void doBuild(const QList<BuildProductPtr> &productsToBuild);
    void prepareBuildGraph(const Artifact::BuildState buildState, bool *sourceFilesChanged);
    void prepareBuildGraph_impl(Artifact *artifact, const Artifact::BuildState buildState, bool *sourceFilesChanged);
    void updateBuildGraph(Artifact::BuildState buildState);
    void updateBuildGraph_impl(Artifact *artifact, Artifact::BuildState buildState, QSet<Artifact *> &seenArtifacts);
    void initLeaves(const QList<Artifact *> &changedArtifacts);
    void initLeavesTopDown(Artifact *artifact, QSet<Artifact *> &seenArtifacts);
    bool scheduleJobs();
    void buildArtifact(Artifact *artifact);
    void finishJob(ExecutorJob *job, bool success);
    void finishArtifact(Artifact *artifact);
    void initializeArtifactsState();
    void setState(ExecutorState);
    void addExecutorJobs(int jobNumber);
    void runAutoMoc();
    void insertLeavesAfterAddingDependencies(QVector<Artifact *> dependencies);
    void cancelJobs();
    void setupProgressObserver(bool mocWillRun);
    void doSanityChecks();

    RulesEvaluationContextPtr m_evalContext;
    BuildOptions m_buildOptions;
    ProgressObserver *m_progressObserver;
    QProcessEnvironment m_baseEnvironment;
    QList<ExecutorJob*> m_availableJobs;
    QHash<ExecutorJob*, Artifact *> m_processingJobs;
    ExecutorState m_state;
    QList<BuildProductPtr> m_productsToBuild;
    QList<Artifact *> m_roots;
    QList<Artifact *> m_leaves;
    ScanResultCache m_scanResultCache;
    InputArtifactScannerContext *m_inputArtifactScanContext;
    AutoMoc *m_autoMoc;
    int m_mocEffort;
    Error m_error;
    bool m_explicitlyCanceled;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPHEXECUTOR_H
