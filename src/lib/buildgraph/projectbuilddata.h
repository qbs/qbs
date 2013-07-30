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
#ifndef QBS_PROJECTBUILDDATA_H
#define QBS_PROJECTBUILDDATA_H

#include "artifactlist.h"
#include "forward_decls.h"
#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/persistentobject.h>

#include <QHash>
#include <QList>
#include <QScriptValue>
#include <QSet>
#include <QString>

namespace qbs {
namespace Internal {
class FileDependency;
class FileResourceBase;
class ScriptEngine;

class ProjectBuildData : public PersistentObject
{
public:
    ProjectBuildData();
    ~ProjectBuildData();

    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId);

    void insertIntoLookupTable(FileResourceBase *fileres);
    void removeFromLookupTable(FileResourceBase *fileres);

    QList<FileResourceBase *> lookupFiles(const QString &filePath) const;
    QList<FileResourceBase *> lookupFiles(const QString &dirPath, const QString &fileName) const;
    QList<FileResourceBase *> lookupFiles(const Artifact *artifact) const;
    void insertFileDependency(FileDependency *dependency);
    void updateNodesThatMustGetNewTransformer(const Logger &logger);
    void removeArtifact(Artifact *artifact, const Logger &logger);
    void removeArtifact(Artifact *artifact, ProjectBuildData *projectBuildData,
                        const Logger &logger);

    QSet<FileDependency *> fileDependencies;
    RulesEvaluationContextPtr evaluationContext;
    QSet<Artifact *> artifactsThatMustGetNewTransformers;
    bool isDirty;

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
    void updateNodeThatMustGetNewTransformer(Artifact *artifact, const Logger &logger);

    typedef QHash<QString, QList<FileResourceBase *> > ResultsPerDirectory;
    typedef QHash<QString, ResultsPerDirectory> ArtifactLookupTable;
    ArtifactLookupTable m_artifactLookupTable;
};


class BuildDataResolver
{
public:
    BuildDataResolver(const Logger &logger);
    void resolveBuildData(const TopLevelProjectPtr &resolvedProject,
                          const RulesEvaluationContextPtr &evalContext);
    void resolveProductBuildDataForExistingProject(const TopLevelProjectPtr &project,
            const QList<ResolvedProductPtr> &freshProducts);

    static void rescueBuildData(const TopLevelProjectConstPtr &source,
                                const TopLevelProjectPtr &target, Logger logger);

private:
    void resolveProductBuildData(const ResolvedProductPtr &product);
    RulesEvaluationContextPtr evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    TopLevelProjectPtr m_project;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_PROJECTBUILDDATA_H
