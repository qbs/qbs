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
#include <tools/persistence.h>
#include <tools/set.h>
#include <tools/qttools.h>

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <QtScript/qscriptvalue.h>

#include <unordered_map>

namespace qbs {
namespace Internal {
class BuildGraphNode;
class FileDependency;
class FileResourceBase;
class ScriptEngine;

class QBS_AUTOTEST_EXPORT ProjectBuildData
{
public:
    ProjectBuildData(const ProjectBuildData *other = nullptr);
    ~ProjectBuildData();

    static QString deriveBuildGraphFilePath(const QString &buildDir, const QString &projectId);

    void insertIntoLookupTable(FileResourceBase *fileres);
    void removeFromLookupTable(FileResourceBase *fileres);

    const std::vector<FileResourceBase *> &lookupFiles(const QString &filePath) const;
    const std::vector<FileResourceBase *> &lookupFiles(const QString &dirPath, const QString &fileName) const;
    const std::vector<FileResourceBase *> &lookupFiles(const Artifact *artifact) const;
    void insertFileDependency(FileDependency *dependency);
    void removeArtifactAndExclusiveDependents(Artifact *artifact, const Logger &logger,
            bool removeFromProduct = true, ArtifactSet *removedArtifacts = nullptr);
    void removeArtifact(Artifact *artifact, const Logger &logger, bool removeFromDisk = true,
                        bool removeFromProduct = true);

    void setDirty();
    void setClean();
    bool isDirty() const { return m_isDirty; }


    Set<FileDependency *> fileDependencies;
    RawScanResults rawScanResults;

    // do not serialize:
    RulesEvaluationContextPtr evaluationContext;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool);

private:
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(fileDependencies, rawScanResults);
    }

    using ArtifactKey = std::pair<QString /*fileName*/, QString /*dirName*/>;
    using ArtifactLookupTable = std::unordered_map<ArtifactKey, std::vector<FileResourceBase *>>;
    ArtifactLookupTable m_artifactLookupTable;

    bool m_doCleanupInDestructor = true;
    bool m_isDirty = true;
};


class BuildDataResolver
{
public:
    BuildDataResolver(Logger logger);
    void resolveBuildData(const TopLevelProjectPtr &resolvedProject,
                          const RulesEvaluationContextPtr &evalContext);
    void resolveProductBuildDataForExistingProject(const TopLevelProjectPtr &project,
            const std::vector<ResolvedProductPtr> &freshProducts);

private:
    void resolveProductBuildData(const ResolvedProductPtr &product);
    void connectRulesToDependencies(const ResolvedProductPtr &product,
                                    const std::vector<ResolvedProductPtr> &dependencies);

    RulesEvaluationContextPtr evalContext() const;
    ScriptEngine *engine() const;
    QScriptValue scope() const;

    TopLevelProjectPtr m_project;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_PROJECTBUILDDATA_H
