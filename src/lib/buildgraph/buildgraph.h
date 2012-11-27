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

#ifndef BUILDGRAPH_H
#define BUILDGRAPH_H

#include "artifactlist.h"

#include <language/language.h>
#include <tools/error.h>
#include <tools/persistentobject.h>
#include <tools/weakpointer.h>

#include <QDir>
#include <QScriptValue>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace qbs {

namespace Internal {
class Artifact;
class BuildProject;
class ProgressObserver;
class Transformer;

typedef QMap<QString, ArtifactList> ArtifactsPerFileTagMap;

class BuildProduct : public PersistentObject
{
public:
    typedef QSharedPointer<BuildProduct> Ptr;
    typedef QSharedPointer<const BuildProduct> ConstPtr;

    static Ptr create() { return Ptr(new BuildProduct); }

    ~BuildProduct();

    void dump() const;
    const QList<Rule::ConstPtr> &topSortedRules() const;
    Artifact *lookupArtifact(const QString &dirPath, const QString &fileName) const;
    Artifact *lookupArtifact(const QString &filePath) const;

    WeakPointer<BuildProject> project;
    ResolvedProduct::Ptr rProduct;
    QSet<Artifact *> targetArtifacts;
    QList<BuildProduct::Ptr> dependencies;
    ArtifactList artifacts;

private:
    BuildProduct();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    mutable QList<Rule::ConstPtr> m_topSortedRules;
};

class BuildGraph;
class BuildProjectLoader;
class BuildProjectResolver;

class BuildProject : public PersistentObject
{
    friend class BuildProjectLoader;
    friend class BuildProjectResolver;
public:
    typedef QSharedPointer<BuildProject> Ptr;
    typedef QSharedPointer<const BuildProject> ConstPtr;

    BuildProject(BuildGraph *bg);
    ~BuildProject();

    void store() const;
    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString projectId);
    QString buildGraphFilePath() const;

    BuildGraph *buildGraph() const;
    ResolvedProject::Ptr resolvedProject() const;
    QSet<BuildProduct::Ptr> buildProducts() const;
    bool dirty() const;
    void markDirty();
    void insertIntoArtifactLookupTable(Artifact *artifact);
    void removeFromArtifactLookupTable(Artifact *artifact);
    QList<Artifact *> lookupArtifacts(const QString &filePath) const;
    QList<Artifact *> lookupArtifacts(const QString &dirPath, const QString &fileName) const;
    void insertFileDependency(Artifact *artifact);
    void rescueDependencies(const BuildProject::Ptr &other);

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
    void addBuildProduct(const BuildProduct::Ptr &product);
    void setResolvedProject(const ResolvedProject::Ptr & resolvedProject);

private:
    BuildGraph *m_buildGraph;
    ResolvedProject::Ptr m_resolvedProject;
    QSet<BuildProduct::Ptr> m_buildProducts;
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

    static Artifact *createArtifact(const BuildProduct::Ptr &product,
                                    const SourceArtifact::ConstPtr &sourceArtifact);

    static bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path);
    static void connect(Artifact *p, Artifact *c);
    static void loggedConnect(Artifact *u, Artifact *v);
    static bool safeConnect(Artifact *u, Artifact *v);
    static void insert(BuildProduct::Ptr target, Artifact *n);
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

    void createTransformerCommands(const PrepareScript::ConstPtr &script, Transformer *transformer);

    static void setupScriptEngineForProduct(ScriptEngine *scriptEngine,
                                            const ResolvedProduct::ConstPtr &product,
                                            Rule::ConstPtr rule, QScriptValue targetObject);
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
    void applyRule(const Rule::ConstPtr &rule);

private:
    void doApply(const ArtifactList &inputArtifacts);
    void setupScriptEngineForArtifact(Artifact *artifact);
    Artifact *createOutputArtifact(const RuleArtifact::ConstPtr &ruleArtifact,
            const ArtifactList &inputArtifacts);
    QString resolveOutPath(const QString &path) const;

    QScriptValue scope() const;
    ScriptEngine *engine() const { return m_buildGraph->engine(); }

    BuildProduct * const m_buildProduct;
    ArtifactsPerFileTagMap &m_artifactsPerFileTag;
    BuildGraph * const m_buildGraph;

    Rule::ConstPtr m_rule;
    QSharedPointer<Transformer> m_transformer;
};

class BuildProjectResolver
{
public:
    BuildProject::Ptr resolveProject(const ResolvedProject::Ptr &resolvedProject,
                                     BuildGraph *buildgraph, ProgressObserver *observer = 0);

private:
    BuildProduct::Ptr resolveProduct(const ResolvedProduct::Ptr &rProduct);

    BuildGraph *buildGraph() const { return m_project->buildGraph(); }
    ScriptEngine *engine() const { return buildGraph()->engine(); }
    QScriptValue scope() const;

    BuildProject::Ptr m_project;
    ProgressObserver *m_observer;
    QHash<ResolvedProduct::Ptr, BuildProduct::Ptr> m_productCache;
};

class BuildProjectLoader
{
public:
    struct LoadResult
    {
        LoadResult() : discardLoadedProject(false) {}

        ResolvedProject::Ptr changedResolvedProject;
        BuildProject::Ptr loadedProject;
        bool discardLoadedProject;
    };

    LoadResult load(const QString &projectFilePath, BuildGraph *bg, const QString &buildRoot,
                    const QVariantMap &cfg, const QStringList &loaderSearchPaths);

private:
    void onProductRemoved(const BuildProduct::Ptr &product);
    void onProductChanged(const BuildProduct::Ptr &product,
                          const ResolvedProduct::Ptr &changedProduct);
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

#endif // BUILDGRAPH_H
