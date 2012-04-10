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

#ifndef BUILDGRAPH_H
#define BUILDGRAPH_H

#include "artifactlist.h"

#include <language/language.h>
#include <tools/error.h>
#include <tools/persistence.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QStringList>

#include <QtCore/QDir>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtScript/QScriptEngine>
#if QT_VERSION >= 0x050000
#    include <QtConcurrent/QFutureInterface>
#else
#    include <QtCore/QFutureInterface>
#endif

namespace qbs {

class Artifact;
class Transformer;
class BuildProject;

class BuildProduct : public PersistentObject
{
public:
    typedef QSharedPointer<BuildProduct> Ptr;

    BuildProduct();
    ~BuildProduct();

    const QList<Rule::Ptr> &topSortedRules() const;
    Artifact *lookupArtifact(const QString &filePath) const;

    BuildProject *project;
    ResolvedProduct::Ptr rProduct;
    QSet<Artifact *> targetArtifacts;
    QList<BuildProduct *> usings;
    QHash<QString, Artifact *> artifacts;

private:
    void load(PersistentPool &pool, PersistentObjectData &data);
    void store(PersistentPool &pool, PersistentObjectData &data) const;

private:
    mutable QList<Rule::Ptr> m_topSortedRules;
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
    static BuildProject::Ptr load(BuildGraph *bg,
                                  const FileTime &minTimeStamp,
                                  Configuration::Ptr cfg,
                                  const QStringList &loaderSearchPaths);
    void store();


    BuildGraph *buildGraph() const;
    ResolvedProject::Ptr resolvedProject() const;
    QSet<BuildProduct::Ptr> buildProducts() const;
    bool dirty() const;
    QList<Artifact *> lookupArtifacts(const QString &filePath, bool stopAtFirstResult = false, const BuildProduct *preferredProduct = 0) const;
    void insertFileDependency(Artifact *artifact);

private:
    static QString storedProjectFilePath(BuildGraph *bg, const QString &configId);
    static QStringList storedProjectFiles(BuildGraph *bg);
    static BuildProject::Ptr restoreBuildGraph(const QString &buildGraphFilePath,
                                               BuildGraph *buildGraph,
                                               const FileTime &minTimeStamp,
                                               Configuration::Ptr configuration,
                                               const QStringList &loaderSearchPaths);
    void load(PersistentPool &pool, PersistentObjectData &data);
    void store(PersistentPool &pool, PersistentObjectData &data) const;
    void markDirty();
    void addBuildProduct(const BuildProduct::Ptr &product);
    void setResolvedProject(const ResolvedProject::Ptr & resolvedProject);

private:
    BuildGraph *m_buildGraph;
    ResolvedProject::Ptr m_resolvedProject;
    QSet<BuildProduct::Ptr> m_buildProducts;
    QHash<QString, Artifact *> m_dependencyArtifacts;
    bool m_dirty;
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
    Q_DECLARE_TR_FUNCTIONS(BuildGraph)
public:
    BuildGraph();
    ~BuildGraph();

    BuildProject::Ptr resolveProject(ResolvedProject::Ptr, QFutureInterface<bool> &futureInterface);
    BuildProduct::Ptr resolveProduct(BuildProject *, ResolvedProduct::Ptr, QFutureInterface<bool> &futureInterface);

    void dump(BuildProduct::Ptr) const;
    void applyRules(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag);
    static void detectCycle(BuildProject *project);
    static void detectCycle(Artifact *a);

    void setOutputDirectoryRoot(const QString &buildDirectoryRoot) { m_outputDirectoryRoot = buildDirectoryRoot; }
    const QString &outputDirectoryRoot() const { return m_outputDirectoryRoot; }
    QString buildDirectoryRoot() const;
    QString resolveOutPath(const QString &path, BuildProduct *) const;

    void connect(Artifact *p, Artifact *c);
    void loggedConnect(Artifact *u, Artifact *v);
    bool safeConnect(Artifact *u, Artifact *v);
    void insert(BuildProduct::Ptr target, Artifact *n) const;
    void insert(BuildProduct *target, Artifact *n) const;
    void remove(Artifact *artifact) const;
    void removeArtifactAndExclusiveDependents(Artifact *artifact, QList<Artifact*> *removedArtifacts = 0);

    void createOutputArtifact(BuildProduct *product,
                          const Rule::Ptr &rule, const RuleArtifact::Ptr &ruleArtifact,
                          const QSet<Artifact *> &inputArtifacts,
                          QList<QPair<RuleArtifact *, Artifact *> > *ruleArtifactArtifactMap,
                          QList<Artifact *> *outputArtifacts,
                          QSharedPointer<Transformer> &transformer);

    void onProductChanged(BuildProduct::Ptr product, ResolvedProduct::Ptr changedProduct, bool *discardStoredProject);
    void updateNodesThatMustGetNewTransformer();

    static void setupScriptEngineForProduct(QScriptEngine *scriptEngine, ResolvedProduct::Ptr product, Rule::Ptr rule, BuildGraph *bg = 0);

private:
    Artifact *createArtifact(BuildProduct::Ptr product, SourceArtifact::Ptr sourceArtifact);
    void applyRule(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag, Rule::Ptr rule);
    void applyRule(BuildProduct *product, QMap<QString, QSet<Artifact *> > &artifactsPerFileTag, Rule::Ptr rule, const QSet<Artifact *> &inputArtifacts);
    void createTransformerCommands(RuleScript::Ptr script, Transformer *transformer);
    static void disconnect(Artifact *u, Artifact *v);
    QSet<Artifact *> disconnect(Artifact *n) const;
    void setupScriptEngineForArtifact(BuildProduct *product, Artifact *artifact);
    void updateNodeThatMustGetNewTransformer(Artifact *artifact);
    static void detectCycle(Artifact *v, QSet<Artifact *> &done, QSet<Artifact *> &currentBranch);

    QScriptEngine *scriptEngine();

private:
    QString m_outputDirectoryRoot;   /// The directory where the 'build' and 'targets' subdirectories end up.
    QHash<QThread *, QScriptEngine *> m_scriptEnginePerThread;
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
