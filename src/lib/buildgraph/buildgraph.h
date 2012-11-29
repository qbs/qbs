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
    BuildProject(BuildGraph *bg);
    ~BuildProject();

    void store() const;
    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString projectId);
    QString buildGraphFilePath() const;

    BuildGraph *buildGraph() const;
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

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
    void addBuildProduct(const BuildProductPtr &product);
    void setResolvedProject(const ResolvedProjectPtr &resolvedProject);

private:
    BuildGraph *m_buildGraph;
    ResolvedProjectPtr m_resolvedProject;
    QSet<BuildProductPtr> m_buildProducts;
    ArtifactList m_dependencyArtifacts;
    QHash<QString, QHash<QString, QList<Artifact *> > > m_artifactLookupTable;
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
    class EngineInitializer
    {
    public:
        EngineInitializer(BuildGraph *bg);
        ~EngineInitializer();

    private:
        BuildGraph *buildGraph;
    };

    BuildGraph();
    ~BuildGraph();

    void setEngine(ScriptEngine *engine);
    ScriptEngine *engine() { return m_engine; }

    void applyRules(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag);

    void setProgressObserver(ProgressObserver *observer);
    void checkCancelation() const;

    static Artifact *createArtifact(const BuildProductPtr &product,
                                    const SourceArtifactConstPtr &sourceArtifact);

    static bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path);
    static void connect(Artifact *p, Artifact *c);
    static void loggedConnect(Artifact *u, Artifact *v);
    static bool safeConnect(Artifact *u, Artifact *v);
    static void insert(BuildProductPtr target, Artifact *n);
    static void insert(BuildProduct *target, Artifact *n);
    void remove(Artifact *artifact) const;
    static void removeGeneratedArtifactFromDisk(Artifact *artifact);

    void updateNodesThatMustGetNewTransformer();
    void removeFromArtifactsThatMustGetNewTransformers(Artifact *a) {
        m_artifactsThatMustGetNewTransformers -= a;
    }
    void addToArtifactsThatMustGetNewTransformers(Artifact *a) {
        m_artifactsThatMustGetNewTransformers += a;
    }

    void createTransformerCommands(const PrepareScriptConstPtr &script, Transformer *transformer);

    static void setupScriptEngineForProduct(ScriptEngine *scriptEngine,
                                            const ResolvedProductConstPtr &product,
                                            RuleConstPtr rule, QScriptValue targetObject);
    static void disconnect(Artifact *u, Artifact *v);
    static void disconnectChildren(Artifact *u);
    static void disconnectParents(Artifact *u);
    static void disconnectAll(Artifact *u);

private:
    void initEngine();
    void cleanupEngine();
    void updateNodeThatMustGetNewTransformer(Artifact *artifact);

    ProgressObserver *m_progressObserver;
    ScriptEngine *m_engine;
    unsigned int m_initEngineCalls;
    QScriptValue m_scope;
    QScriptValue m_prepareScriptScope;
    QHash<QString, QScriptProgram> m_scriptProgramCache;
    mutable QSet<Artifact *> m_artifactsThatMustGetNewTransformers;

    friend class EngineInitializer;
};

class RulesApplicator
{
public:
    RulesApplicator(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag,
            BuildGraph *bg);
    void applyAllRules();
    void applyRule(const RuleConstPtr &rule);

private:
    void doApply(const ArtifactList &inputArtifacts);
    void setupScriptEngineForArtifact(Artifact *artifact);
    Artifact *createOutputArtifact(const RuleArtifactConstPtr &ruleArtifact,
            const ArtifactList &inputArtifacts);
    QString resolveOutPath(const QString &path) const;

    QScriptValue scope() const;
    ScriptEngine *engine() const { return m_buildGraph->engine(); }

    BuildProduct * const m_buildProduct;
    ArtifactsPerFileTagMap &m_artifactsPerFileTag;
    BuildGraph * const m_buildGraph;

    RuleConstPtr m_rule;
    TransformerPtr m_transformer;
};

class BuildProjectResolver
{
public:
    BuildProjectPtr resolveProject(const ResolvedProjectPtr &resolvedProject,
                                     BuildGraph *buildgraph, ProgressObserver *observer = 0);

private:
    BuildProductPtr resolveProduct(const ResolvedProductPtr &rProduct);

    BuildGraph *buildGraph() const { return m_project->buildGraph(); }
    ScriptEngine *engine() const { return buildGraph()->engine(); }
    QScriptValue scope() const;

    BuildProjectPtr m_project;
    ProgressObserver *m_observer;
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

    LoadResult load(const QString &projectFilePath, BuildGraph *bg, const QString &buildRoot,
                    const QVariantMap &cfg, const QStringList &loaderSearchPaths);

private:
    void onProductRemoved(const BuildProductPtr &product);
    void onProductChanged(const BuildProductPtr &product,
                          const ResolvedProductPtr &changedProduct);
    void removeArtifactAndExclusiveDependents(Artifact *artifact,
                                              QList<Artifact*> *removedArtifacts = 0);

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
