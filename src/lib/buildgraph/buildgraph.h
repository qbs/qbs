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
#ifndef QBS_BUILDGRAPH_H
#define QBS_BUILDGRAPH_H

#include "forward_decls.h"
#include "rulesapplicator.h"

#include <language/forward_decls.h>
#include <logging/logger.h>

#include <QProcessEnvironment>
#include <QScriptValue>
#include <QStringList>

namespace qbs {
class SetupProjectParameters;
namespace Internal {
class Logger;
class ScriptEngine;
class ScriptPropertyObserver;

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &dirPath,
                         const QString &fileName);
Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &filePath);
Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const Artifact *artifact);

Artifact *createArtifact(const ResolvedProductPtr &product,
                         const SourceArtifactConstPtr &sourceArtifact, const Logger &logger);
void insertArtifact(const ResolvedProductPtr &product, Artifact *artifact, const Logger &logger);
void addTargetArtifacts(const ResolvedProductPtr &product,
                        ArtifactsPerFileTagMap &artifactsPerFileTag, const Logger &logger);
void dumpProductBuildData(const ResolvedProductConstPtr &product);


bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path);
void connect(Artifact *p, Artifact *c);
void loggedConnect(Artifact *u, Artifact *v, const Logger &logger);
bool safeConnect(Artifact *u, Artifact *v, const Logger &logger);
void removeGeneratedArtifactFromDisk(Artifact *artifact, const Logger &logger);
void disconnect(Artifact *u, Artifact *v, const Logger &logger);

void setupScriptEngineForProduct(ScriptEngine *engine, const ResolvedProductConstPtr &product,
                                 const RuleConstPtr &rule, QScriptValue targetObject,
                                 ScriptPropertyObserver *observer = 0);
QString relativeArtifactFileName(const Artifact *n); // Debugging helpers

template <typename T>
QStringList toStringList(const T &artifactContainer)
{
    QStringList l;
    foreach (Artifact *n, artifactContainer)
        l.append(relativeArtifactFileName(n));
    return l;
}

class BuildGraphLoader
{
public:
    BuildGraphLoader(const QProcessEnvironment &env, const Logger &logger);

    class LoadResult
    {
    public:
        LoadResult() : discardLoadedProject(false) {}

        ResolvedProjectPtr newlyResolvedProject;
        ResolvedProjectPtr loadedProject;
        bool discardLoadedProject;
    };

    LoadResult load(const SetupProjectParameters &parameters,
                    const RulesEvaluationContextPtr &evalContext);

private:
    void trackProjectChanges(const SetupProjectParameters &parameters,
                             const QString &buildGraphFilePath,
                             const ResolvedProjectPtr &restoredProject);
    void onProductRemoved(const ResolvedProductPtr &product);
    void onProductChanged(const ResolvedProductPtr &product,
                          const ResolvedProductPtr &changedProduct);
    void removeArtifactAndExclusiveDependents(Artifact *artifact,
            ArtifactList *removedArtifacts = 0);
    bool checkForPropertyChanges(const TransformerPtr &restoredTrafo, const ResolvedProductPtr &freshProduct);

    RulesEvaluationContextPtr m_evalContext;
    LoadResult m_result;
    Logger m_logger;
    QProcessEnvironment m_environment;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPH_H
