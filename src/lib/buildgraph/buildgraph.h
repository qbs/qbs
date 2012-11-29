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

#ifndef QBS_BUILDGRAPH_H
#define QBS_BUILDGRAPH_H

#include "artifactlist.h"
#include "forward_decls.h"

#include <language/forward_decls.h>
#include <tools/error.h>
#include <tools/persistentobject.h>
#include <tools/weakpointer.h>

#include <QDir>
#include <QScriptProgram>
#include <QScriptValue>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace qbs {
namespace Internal {
class ProgressObserver;
class RulesEvaluationContext;
class ScriptEngine;
class Transformer;

typedef QMap<QString, ArtifactList> ArtifactsPerFileTagMap;

class BuildProduct : public PersistentObject
{
public:
    static BuildProductPtr create() { return BuildProductPtr(new BuildProduct); }

    ~BuildProduct();

    void dump() const;
    const QList<RuleConstPtr> &topSortedRules() const;
    Artifact *lookupArtifact(const QString &dirPath, const QString &fileName) const;
    Artifact *lookupArtifact(const QString &filePath) const;
    Artifact *createArtifact(const SourceArtifactConstPtr &sourceArtifact);
    void insertArtifact(Artifact *n);
    void applyRules(ArtifactsPerFileTagMap &artifactsPerFileTag);

    WeakPointer<BuildProject> project;
    ResolvedProductPtr rProduct;
    QSet<Artifact *> targetArtifacts;
    QList<BuildProductPtr> dependencies;
    ArtifactList artifacts;

private:
    BuildProduct();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    mutable QList<RuleConstPtr> m_topSortedRules;
};

class BuildGraph;
class BuildProjectLoader;
class BuildProjectResolver;

class BuildProject : public PersistentObject
{
    friend class BuildProjectLoader;
    friend class BuildProjectResolver;
public:
    BuildProject();
    ~BuildProject();

    void store() const;
    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString projectId);
    QString buildGraphFilePath() const;

    void setEvaluationContext(RulesEvaluationContext *evalContext);
    RulesEvaluationContext *evaluationContext() const { return m_evalContext; }

    ResolvedProjectPtr resolvedProject() const;
    QSet<BuildProductPtr> buildProducts() const;
    bool dirty() const;
    void markDirty();
    void insertIntoArtifactLookupTable(Artifact *artifact);
    void removeFromArtifactLookupTable(Artifact *artifact);
    QList<Artifact *> lookupArtifacts(const QString &filePath) const;
    QList<Artifact *> lookupArtifacts(const QString &dirPath, const QString &fileName) const;
    void insertFileDependency(Artifact *artifact);
    void rescueDependencies(const BuildProjectPtr &other);
    void removeArtifact(Artifact *artifact);
    void updateNodesThatMustGetNewTransformer();
    void removeFromArtifactsThatMustGetNewTransformers(Artifact *a) {
        m_artifactsThatMustGetNewTransformers -= a;
    }
    void addToArtifactsThatMustGetNewTransformers(Artifact *a) {
        m_artifactsThatMustGetNewTransformers += a;
    }

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
    void addBuildProduct(const BuildProductPtr &product);
    void setResolvedProject(const ResolvedProjectPtr &resolvedProject);
    void updateNodeThatMustGetNewTransformer(Artifact *artifact);

private:
    RulesEvaluationContext *m_evalContext;
    ResolvedProjectPtr m_resolvedProject;
    QSet<BuildProductPtr> m_buildProducts;
    ArtifactList m_dependencyArtifacts;
    QHash<QString, QHash<QString, QList<Artifact *> > > m_artifactLookupTable;
    QSet<Artifact *> m_artifactsThatMustGetNewTransformers;
    mutable bool m_dirty;
};

/**
 * N artifact, T transformer, parent -> child
 * parent depends on child, child is a dependency of parent,
 * parent is a dependent of child.
 *
 * N a.out -> N main.o -> N main.cpp
 *
 * Every artifact can point to a transformer which contains the commands.
 * Multiple artifacts can point to the same transformer.
 */
class BuildGraph
{
public:
    static bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path);
    static void connect(Artifact *p, Artifact *c);
    static void loggedConnect(Artifact *u, Artifact *v);
    static bool safeConnect(Artifact *u, Artifact *v);
    static void removeGeneratedArtifactFromDisk(Artifact *artifact);
    static void disconnect(Artifact *u, Artifact *v);
    static void setupScriptEngineForProduct(ScriptEngine *engine,
            const ResolvedProductConstPtr &product, const RuleConstPtr &rule,
            QScriptValue targetObject);

private:
    BuildGraph();
};

class RulesApplicator
{
public:
    RulesApplicator(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag);
    void applyAllRules();
    void applyRule(const RuleConstPtr &rule);

private:
    void doApply(const ArtifactList &inputArtifacts);
    void setupScriptEngineForArtifact(Artifact *artifact);
    Artifact *createOutputArtifact(const RuleArtifactConstPtr &ruleArtifact,
            const ArtifactList &inputArtifacts);
    QString resolveOutPath(const QString &path) const;
    RulesEvaluationContext *evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    BuildProduct * const m_buildProduct;
    ArtifactsPerFileTagMap &m_artifactsPerFileTag;

    RuleConstPtr m_rule;
    TransformerPtr m_transformer;
};

class BuildProjectResolver
{
public:
    BuildProjectPtr resolveProject(const ResolvedProjectPtr &resolvedProject,
                                   RulesEvaluationContext *evalContext);

private:
    BuildProductPtr resolveProduct(const ResolvedProductPtr &rProduct);

    RulesEvaluationContext *evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    BuildProjectPtr m_project;
    QHash<ResolvedProductPtr, BuildProductPtr> m_productCache;
};

class BuildProjectLoader
{
public:
    struct LoadResult
    {
        LoadResult() : discardLoadedProject(false) {}

        ResolvedProjectPtr changedResolvedProject;
        BuildProjectPtr loadedProject;
        bool discardLoadedProject;
    };

    LoadResult load(const QString &projectFilePath, RulesEvaluationContext *evalContext,
                    const QString &buildRoot, const QVariantMap &cfg,
                    const QStringList &loaderSearchPaths);

private:
    void onProductRemoved(const BuildProductPtr &product);
    void onProductChanged(const BuildProductPtr &product,
                          const ResolvedProductPtr &changedProduct);
    void removeArtifactAndExclusiveDependents(Artifact *artifact,
                                              QList<Artifact*> *removedArtifacts = 0);

    RulesEvaluationContext *m_evalContext;
    LoadResult m_result;
};

// debugging helper
QString fileName(Artifact *n);

// debugging helper
template <typename T>
static QStringList toStringList(const T &artifactContainer)
{
    QStringList l;
    foreach (Artifact *n, artifactContainer)
        l.append(fileName(n));
    return l;
}

char **createCFileTags(const QSet<QString> &fileTags);
void freeCFileTags(char **cFileTags, int numFileTags);

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPH_H
