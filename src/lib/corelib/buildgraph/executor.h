/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef QBS_BUILDGRAPHEXECUTOR_H
#define QBS_BUILDGRAPHEXECUTOR_H

#include "forward_decls.h"
#include "buildgraphvisitor.h"
#include <buildgraph/artifact.h>
#include <buildgraph/scanresultcache.h>
#include <language/forward_decls.h>

#include <logging/logger.h>
#include <tools/buildoptions.h>
#include <tools/error.h>

#include <QObject>
#include <queue>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace qbs {
class ProcessResult;

namespace Internal {
class ExecutorJob;
class FileTime;
class InputArtifactScannerContext;
class ProductInstaller;
class ProgressObserver;
class RuleNode;

class Executor : public QObject, private BuildGraphVisitor
{
    Q_OBJECT

public slots:
    void build();

public:
    Executor(const Logger &logger, QObject *parent = 0);
    ~Executor();

    void setProject(const TopLevelProjectPtr &project);
    void setProducts(const QList<ResolvedProductPtr> &productsToBuild);
    void setBuildOptions(const BuildOptions &buildOptions);
    void setProgressObserver(ProgressObserver *observer) { m_progressObserver = observer; }

    ErrorInfo error() const { return m_error; }

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);

    void finished();

private slots:
    void onJobFinished(const qbs::ErrorInfo &err);
    void finish();
    void checkForCancellation();

private:
    // BuildGraphVisitor implementation
    bool visit(Artifact *artifact);
    bool visit(RuleNode *ruleNode);

    enum ExecutorState { ExecutorIdle, ExecutorRunning, ExecutorCanceling };

    struct ComparePriority
    {
        bool operator() (const BuildGraphNode *x, const BuildGraphNode *y) const;
    };

    typedef std::priority_queue<Artifact *, std::vector<BuildGraphNode *>, ComparePriority> Leaves;

    void doBuild();
    void prepareAllNodes();
    void prepareArtifact(Artifact *artifact);
    void prepareReachableNodes();
    void prepareReachableNodes_impl(BuildGraphNode *node);
    void prepareProducts();
    void setupRootNodes();
    void initLeaves();
    void updateLeaves(const NodeSet &nodes);
    void updateLeaves(BuildGraphNode *node, NodeSet &seenNodes);
    bool scheduleJobs();
    void buildArtifact(Artifact *artifact);
    void executeRuleNode(RuleNode *ruleNode);
    void finishJob(ExecutorJob *job, bool success);
    void finishNode(BuildGraphNode *leaf);
    void finishArtifact(Artifact *artifact);
    void setState(ExecutorState);
    void addExecutorJobs();
    void cancelJobs();
    void setupProgressObserver();
    void doSanityChecks();
    void handleError(const ErrorInfo &error);
    void rescueOldBuildData(Artifact *artifact, bool *childrenAdded);
    bool checkForUnbuiltDependencies(Artifact *artifact);
    void potentiallyRunTransformer(const TransformerPtr &transformer);
    void runTransformer(const TransformerPtr &transformer);
    void finishTransformer(const TransformerPtr &transformer);
    void possiblyInstallArtifact(const Artifact *artifact);

    bool mustExecuteTransformer(const TransformerPtr &transformer) const;
    bool isUpToDate(Artifact *artifact) const;
    void retrieveSourceFileTimestamp(Artifact *artifact) const;
    FileTime recursiveFileTime(const QString &filePath) const;
    QString configString() const;
    bool transformerHasMatchingOutputTags(const TransformerConstPtr &transformer) const;
    bool transformerHasMatchingInputFiles(const TransformerConstPtr &transformer) const;

    typedef QHash<ExecutorJob *, TransformerPtr> JobMap;
    JobMap m_processingJobs;

    ProductInstaller *m_productInstaller;
    RulesEvaluationContextPtr m_evalContext;
    BuildOptions m_buildOptions;
    Logger m_logger;
    ProgressObserver *m_progressObserver;
    QList<ExecutorJob*> m_availableJobs;
    ExecutorState m_state;
    TopLevelProjectPtr m_project;
    QList<ResolvedProductPtr> m_productsToBuild;
    NodeSet m_roots;
    Leaves m_leaves;
    QList<Artifact *> m_changedSourceArtifacts;
    ScanResultCache m_scanResultCache;
    InputArtifactScannerContext *m_inputArtifactScanContext;
    ErrorInfo m_error;
    bool m_explicitlyCanceled;
    FileTags m_activeFileTags;
    QTimer * const m_cancelationTimer;
    QStringList m_artifactsRemovedFromDisk;
    const bool m_doTrace;
    const bool m_doDebug;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPHEXECUTOR_H
