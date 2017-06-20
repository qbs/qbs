/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QBS_PROJECTBUILDDATA_H
#define QBS_PROJECTBUILDDATA_H

#include "forward_decls.h"
#include "rawscanresults.h"
#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/persistentobject.h>
#include <tools/set.h>

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {
class BuildGraphNode;
class FileDependency;
class FileResourceBase;
class ScriptEngine;

class QBS_AUTOTEST_EXPORT ProjectBuildData : public PersistentObject
{
public:
    ProjectBuildData(const ProjectBuildData *other = 0);
    ~ProjectBuildData();

    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId);

    void insertIntoLookupTable(FileResourceBase *fileres);
    void removeFromLookupTable(FileResourceBase *fileres);

    QList<FileResourceBase *> lookupFiles(const QString &filePath) const;
    QList<FileResourceBase *> lookupFiles(const QString &dirPath, const QString &fileName) const;
    QList<FileResourceBase *> lookupFiles(const Artifact *artifact) const;
    void insertFileDependency(FileDependency *dependency);
    void removeArtifactAndExclusiveDependents(Artifact *artifact, const Logger &logger,
            bool removeFromProduct = true, ArtifactSet *removedArtifacts = 0);
    void removeArtifact(Artifact *artifact, const Logger &logger, bool removeFromDisk = true,
                        bool removeFromProduct = true);


    Set<FileDependency *> fileDependencies;
    RawScanResults rawScanResults;

    // do not serialize:
    RulesEvaluationContextPtr evaluationContext;
    bool isDirty;

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    typedef QHash<QString, QList<FileResourceBase *> > ResultsPerDirectory;
    typedef QHash<QString, ResultsPerDirectory> ArtifactLookupTable;
    ArtifactLookupTable m_artifactLookupTable;
    bool m_doCleanupInDestructor;
};


class BuildDataResolver
{
public:
    BuildDataResolver(const Logger &logger);
    void resolveBuildData(const TopLevelProjectPtr &resolvedProject,
                          const RulesEvaluationContextPtr &evalContext);
    void resolveProductBuildDataForExistingProject(const TopLevelProjectPtr &project,
            const QList<ResolvedProductPtr> &freshProducts);

private:
    void resolveProductBuildData(const ResolvedProductPtr &product);
    void connectRulesToDependencies(const ResolvedProductPtr &product,
                                    const Set<ResolvedProductPtr> &dependencies);

    RulesEvaluationContextPtr evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    TopLevelProjectPtr m_project;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_PROJECTBUILDDATA_H
