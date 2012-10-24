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
#include <tools/persistence.h>

#include <QCoreApplication>
#include <QDir>
#include <QScriptValue>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace qbs {

class Artifact;
class Transformer;
class BuildProject;

typedef QMap<QString, QSet<Artifact *> > ArtifactsPerFileTagMap;

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

    BuildProject *project;
    ResolvedProduct::Ptr rProduct;
    QSet<Artifact *> targetArtifacts;
    QList<BuildProduct *> usings;
    ArtifactList artifacts;

private:
    BuildProduct();

    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;

private:
    mutable QList<Rule::ConstPtr> m_topSortedRules;
};

class BuildGraph;

class BuildProject : public PersistentObject
{
    friend class BuildGraph;
public:
    typedef QSharedPointer<BuildProject> Ptr;
    typedef QSharedPointer<const BuildProject> ConstPtr;

    BuildProject(BuildGraph *bg);
    ~BuildProject();

    struct LoadResult
    {
        ResolvedProject::Ptr changedResolvedProject;
        BuildProject::Ptr loadedProject;
        bool discardLoadedProject;
    };

    static LoadResult load(BuildGraph *bg,
                           const FileTime &minTimeStamp,
                           const QVariantMap &cfg,
                           const QStringList &loaderSearchPaths);
    void store() const;


    BuildGraph *buildGraph() const;
    ResolvedProject::Ptr resolvedProject() const;
    QSet<BuildProduct::Ptr> buildProducts() const;
    bool dirty() const;
    void insertIntoArtifactLookupTable(Artifact *artifact);
    void removeFromArtifactLookupTable(Artifact *artifact);
    QList<Artifact *> lookupArtifacts(const QString &filePath) const;
    QList<Artifact *> lookupArtifacts(const QString &dirPath, const QString &fileName) const;
    void insertFileDependency(Artifact *artifact);
    void rescueDependencies(const BuildProject::Ptr &other);

    void onProductRemoved(const BuildProduct::Ptr &product);

private:
    static void restoreBuildGraph(BuildGraph *buildGraph,
                                  const FileTime &minTimeStamp,
                                  const QVariantMap &configuration,
                                  const QStringList &loaderSearchPaths,
                                  LoadResult *loadResult);
    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;
    void markDirty();
    void addBuildProduct(const BuildProduct::Ptr &product);
    void setResolvedProject(const ResolvedProject::Ptr & resolvedProject);

private:
    BuildGraph *m_buildGraph;
    ResolvedProject::Ptr m_resolvedProject;
    QSet<BuildProduct::Ptr> m_buildProducts;
    ArtifactList m_dependencyArtifacts;
    QHash<QString, QHash<QString, QList<Artifact *> > > m_artifactLookupTable;
    bool m_dirty;
};

class ProgressObserver;

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
    Q_DECLARE_TR_FUNCTIONS(BuildGraph)
public:
    BuildGraph(QbsEngine *engine);
    ~BuildGraph();

    QbsEngine *engine() { return m_engine; }

    BuildProject::Ptr resolveProject(ResolvedProject::Ptr);

    void applyRules(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag);
    static void detectCycle(BuildProject *project);
    static void detectCycle(Artifact *a);

    void setProgressObserver(ProgressObserver *observer);
    void setOutputDirectoryRoot(const QString &buildDirectoryRoot) { m_outputDirectoryRoot = buildDirectoryRoot; }
    const QString &outputDirectoryRoot() const { return m_outputDirectoryRoot; }
    QString buildDirectory(const QString &projectId) const;
    QString buildGraphFilePath(const QString &projectId) const;

    static void connect(Artifact *p, Artifact *c);
    static void loggedConnect(Artifact *u, Artifact *v);
    static bool safeConnect(Artifact *u, Artifact *v);
    void insert(BuildProduct::Ptr target, Artifact *n) const;
    void insert(BuildProduct *target, Artifact *n) const;
    void remove(Artifact *artifact) const;
    void removeArtifactAndExclusiveDependents(Artifact *artifact, QList<Artifact*> *removedArtifacts = 0);
    static void removeGeneratedArtifactFromDisk(Artifact *artifact);

    void onProductChanged(BuildProduct::Ptr product, ResolvedProduct::Ptr changedProduct, bool *discardStoredProject);
    void updateNodesThatMustGetNewTransformer();
    void removeFromArtifactsThatMustGetNewTransformers(Artifact *a) {
        m_artifactsThatMustGetNewTransformers -= a;
    }
    void createTransformerCommands(const RuleScript::ConstPtr &script, Transformer *transformer);

    static void setupScriptEngineForProduct(QbsEngine *scriptEngine,
                                            const ResolvedProduct::ConstPtr &product,
                                            Rule::ConstPtr rule, QScriptValue targetObject);
    static void disconnect(Artifact *u, Artifact *v);
    static void disconnectChildren(Artifact *u);
    static void disconnectParents(Artifact *u);
    static void disconnectAll(Artifact *u);

private:
    void initEngine();
    void cleanupEngine();
    BuildProduct::Ptr resolveProduct(BuildProject *, ResolvedProduct::Ptr);
    Artifact *createArtifact(BuildProduct::Ptr product, SourceArtifact::ConstPtr sourceArtifact);
    void updateNodeThatMustGetNewTransformer(Artifact *artifact);
    static void detectCycle(Artifact *v, QSet<Artifact *> &done, QSet<Artifact *> &currentBranch);

    class EngineInitializer
    {
    public:
        EngineInitializer(BuildGraph *bg);
        ~EngineInitializer();

    private:
        BuildGraph *buildGraph;
    };

    ProgressObserver *m_progressObserver;
    QString m_outputDirectoryRoot;   /// The directory where the 'build' and 'targets' subdirectories end up.
    QbsEngine *m_engine;
    unsigned int m_initEngineCalls;
    QScriptValue m_scope;
    QScriptValue m_prepareScriptScope;
    QHash<ResolvedProduct::Ptr, BuildProduct::Ptr> m_productCache;
    QHash<QString, QScriptValue> m_jsImportCache;
    QHash<QString, QScriptProgram> m_scriptProgramCache;
    mutable QSet<Artifact *> m_artifactsThatMustGetNewTransformers;

    friend class EngineInitializer;
};

class RulesApplicator
{
    Q_DECLARE_TR_FUNCTIONS(RulesApplicator)
public:
    RulesApplicator(BuildProduct *product, ArtifactsPerFileTagMap &artifactsPerFileTag,
            BuildGraph *bg);
    void applyAllRules();
    void applyRule(const Rule::ConstPtr &rule);

private:
    void doApply(const QSet<Artifact *> &inputArtifacts);
    void setupScriptEngineForArtifact(Artifact *artifact);
    Artifact *createOutputArtifact(const RuleArtifact::ConstPtr &ruleArtifact,
            const QSet<Artifact *> &inputArtifacts);
    QString resolveOutPath(const QString &path) const;

    QScriptValue scope() const;
    QbsEngine *engine() const { return m_buildGraph->engine(); }

    BuildProduct * const m_buildProduct;
    ArtifactsPerFileTagMap &m_artifactsPerFileTag;
    BuildGraph * const m_buildGraph;

    Rule::ConstPtr m_rule;
    QSharedPointer<Transformer> m_transformer;
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

} // namespace qbs

#endif // BUILDGRAPH_H
