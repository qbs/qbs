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
#include <QScriptEngine>
#include <QSet>
#include <QSharedPointer>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace qbs {

class Artifact;
class Transformer;
class BuildProject;

class BuildProduct : public PersistentObject
{
public:
    typedef QSharedPointer<BuildProduct> Ptr;

    static Ptr create() { return Ptr(new BuildProduct); }

    ~BuildProduct();

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

    BuildProject(BuildGraph *bg);
    ~BuildProject();

    static BuildProject::Ptr loadBuildGraph(const QString &buildGraphFilePath,
                                            BuildGraph *buildGraph,
                                            const FileTime &minTimeStamp,
                                            const QStringList &loaderSearchPaths);

    struct LoadResult
    {
        ResolvedProject::Ptr changedResolvedProject;
        BuildProject::Ptr loadedProject;
        bool discardLoadedProject;
    };

    static LoadResult load(BuildGraph *bg,
                           const FileTime &minTimeStamp,
                           Configuration::Ptr cfg,
                           const QStringList &loaderSearchPaths);
    void store();


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
    static QString storedProjectFilePath(BuildGraph *bg, const QString &configId);
    static QStringList storedProjectFiles(BuildGraph *bg);
    static void restoreBuildGraph(const QString &buildGraphFilePath,
                                  BuildGraph *buildGraph,
                                  const FileTime &minTimeStamp,
                                  Configuration::Ptr configuration,
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
    BuildGraph();
    ~BuildGraph();

    QbsEngine *engine() { return m_engine; }

    BuildProject::Ptr resolveProject(ResolvedProject::Ptr);
    BuildProduct::Ptr resolveProduct(BuildProject *, ResolvedProduct::Ptr);

    void dump(BuildProduct::Ptr) const;
    void applyRules(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag);
    static void detectCycle(BuildProject *project);
    static void detectCycle(Artifact *a);

    void setProgressObserver(ProgressObserver *observer);
    void setOutputDirectoryRoot(const QString &buildDirectoryRoot) { m_outputDirectoryRoot = buildDirectoryRoot; }
    const QString &outputDirectoryRoot() const { return m_outputDirectoryRoot; }
    QString buildDirectoryRoot() const;
    QString resolveOutPath(const QString &path, BuildProduct *) const;

    static void connect(Artifact *p, Artifact *c);
    static void loggedConnect(Artifact *u, Artifact *v);
    static bool safeConnect(Artifact *u, Artifact *v);
    void insert(BuildProduct::Ptr target, Artifact *n) const;
    void insert(BuildProduct *target, Artifact *n) const;
    void remove(Artifact *artifact) const;
    void removeArtifactAndExclusiveDependents(Artifact *artifact, QList<Artifact*> *removedArtifacts = 0);
    static void removeGeneratedArtifactFromDisk(Artifact *artifact);

    void createOutputArtifact(BuildProduct *product,
                          const Rule::ConstPtr &rule, const RuleArtifact::ConstPtr &ruleArtifact,
                          const QSet<Artifact *> &inputArtifacts,
                          QList<QPair<const RuleArtifact *, Artifact *> > *ruleArtifactArtifactMap,
                          QList<Artifact *> *outputArtifacts,
                          QSharedPointer<Transformer> &transformer);

    void onProductChanged(BuildProduct::Ptr product, ResolvedProduct::Ptr changedProduct, bool *discardStoredProject);
    void updateNodesThatMustGetNewTransformer();

    static void setupScriptEngineForProduct(QbsEngine *scriptEngine,
                                            const ResolvedProduct::ConstPtr &product,
                                            Rule::ConstPtr rule);
    static void disconnect(Artifact *u, Artifact *v);
    static void disconnectChildren(Artifact *u);
    static void disconnectParents(Artifact *u);
    static void disconnectAll(Artifact *u);

private:
    Artifact *createArtifact(BuildProduct::Ptr product, SourceArtifact::ConstPtr sourceArtifact);
    void applyRule(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag, Rule::ConstPtr rule);
    void applyRule(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag, const Rule::ConstPtr &rule, const QSet<Artifact *> &inputArtifacts);
    void createTransformerCommands(const RuleScript::ConstPtr &script, Transformer *transformer);
    void setupScriptEngineForArtifact(BuildProduct *product, Artifact *artifact);
    void updateNodeThatMustGetNewTransformer(Artifact *artifact);
    static void detectCycle(Artifact *v, QSet<Artifact *> &done, QSet<Artifact *> &currentBranch);

private:
    ProgressObserver *m_progressObserver;
    QString m_outputDirectoryRoot;   /// The directory where the 'build' and 'targets' subdirectories end up.
    QbsEngine *m_engine;
    QHash<ResolvedProduct::Ptr, BuildProduct::Ptr> m_productCache;
    QHash<QString, QScriptValue> m_jsImportCache;
    QHash<QString, QScriptProgram> m_scriptProgramCache;
    mutable QSet<Artifact *> m_artifactsThatMustGetNewTransformers;
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
