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
#ifndef QBS_BUILDPROJECT_H
#define QBS_BUILDPROJECT_H

#include "artifactlist.h"
#include "forward_decls.h"
#include <language/forward_decls.h>
#include <tools/persistentobject.h>

#include <QHash>
#include <QList>
#include <QScriptValue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace qbs {
namespace Internal {
class BuildProjectLoader;
class BuildProjectResolver;
class ScriptEngine;

class BuildProject : public PersistentObject
{
    friend class BuildProjectLoader;
    friend class BuildProjectResolver;
public:
    BuildProject();
    ~BuildProject();

    void store() const;
    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId);
    QString buildGraphFilePath() const;

    void setEvaluationContext(const RulesEvaluationContextPtr &evalContext);
    RulesEvaluationContextPtr evaluationContext() const { return m_evalContext; }

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
    RulesEvaluationContextPtr m_evalContext;
    ResolvedProjectPtr m_resolvedProject;
    QSet<BuildProductPtr> m_buildProducts;
    ArtifactList m_dependencyArtifacts;
    QHash<QString, QHash<QString, QList<Artifact *> > > m_artifactLookupTable;
    QSet<Artifact *> m_artifactsThatMustGetNewTransformers;
    mutable bool m_dirty;
};


class BuildProjectResolver
{
public:
    BuildProjectPtr resolveProject(const ResolvedProjectPtr &resolvedProject,
                                   const RulesEvaluationContextPtr &evalContext);

private:
    BuildProductPtr resolveProduct(const ResolvedProductPtr &rProduct);

    RulesEvaluationContextPtr evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    BuildProjectPtr m_project;
    QHash<ResolvedProductPtr, BuildProductPtr> m_productCache;
};


class BuildProjectLoader
{
public:
    class LoadResult
    {
    public:
        LoadResult() : discardLoadedProject(false) {}

        ResolvedProjectPtr changedResolvedProject;
        BuildProjectPtr loadedProject;
        bool discardLoadedProject;
    };

    LoadResult load(const QString &projectFilePath, const RulesEvaluationContextPtr &evalContext,
                    const QString &buildRoot, const QVariantMap &cfg,
                    const QStringList &loaderSearchPaths);

private:
    void onProductRemoved(const BuildProductPtr &product);
    void onProductChanged(const BuildProductPtr &product,
                          const ResolvedProductPtr &changedProduct);
    void removeArtifactAndExclusiveDependents(Artifact *artifact,
                                              QList<Artifact*> *removedArtifacts = 0);

    RulesEvaluationContextPtr m_evalContext;
    LoadResult m_result;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDPROJECT_H
