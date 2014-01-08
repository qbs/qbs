/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#ifndef QBS_BUILDGRAPHLOADER_H
#define QBS_BUILDGRAPHLOADER_H

#include "forward_decls.h"

#include <buildgraph/artifactlist.h>
#include <language/forward_decls.h>
#include <logging/logger.h>

#include <QProcessEnvironment>
#include <QVariantMap>

namespace qbs {
class SetupProjectParameters;

namespace Internal {
class FileDependency;
class FileResourceBase;
class FileTime;
class Property;

class BuildGraphLoadResult
{
public:
    TopLevelProjectPtr newlyResolvedProject;
    TopLevelProjectPtr loadedProject;
};


class BuildGraphLoader
{
public:
    BuildGraphLoader(const QProcessEnvironment &env, const Logger &logger);
    ~BuildGraphLoader();

    BuildGraphLoadResult load(const SetupProjectParameters &parameters,
                              const RulesEvaluationContextPtr &evalContext);

private:
    void trackProjectChanges(const SetupProjectParameters &parameters,
                             const QString &buildGraphFilePath,
                             const TopLevelProjectPtr &restoredProject,
                             const QVariantMap &oldProjectConfig);
    bool hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileExistsResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasProductFileChanged(const QList<ResolvedProductPtr> &restoredProducts,
                               const FileTime &referenceTime,
                               QSet<QString> &remainingBuildSystemFiles,
                               QList<ResolvedProductPtr> &productsWithChangedFiles);
    bool hasBuildSystemFileChanged(const QSet<QString> &buildSystemFiles,
                                   const FileTime &referenceTime);
    void checkAllProductsForChanges(const QList<ResolvedProductPtr> &restoredProducts,
            const QMap<QString, ResolvedProductPtr> &newlyResolvedProductsByName,
            QList<ResolvedProductPtr> &changedProducts,
            QList<ResolvedProductPtr> &productsWithChangedFiles);
    bool checkProductForChanges(const ResolvedProductPtr &restoredProduct,
                                const ResolvedProductPtr &newlyResolvedProduct);
    bool checkProductForInstallInfoChanges(const ResolvedProductPtr &restoredProduct,
                                           const ResolvedProductPtr &newlyResolvedProduct);
    bool checkForPropertyChanges(const ResolvedProductPtr &restoredProduct,
                                 const ResolvedProductPtr &newlyResolvedProduct);
    void onProductRemoved(const ResolvedProductPtr &product, ProjectBuildData *projectBuildData,
                          bool removeArtifactsFromDisk = true);
    void onProductFileListChanged(const ResolvedProductPtr &restoredProduct,
            const ResolvedProductPtr &newlyResolvedProduct, const ProjectBuildData *oldBuildData);
    void removeArtifactAndExclusiveDependents(Artifact *artifact,
            ArtifactList *removedArtifacts = 0);
    bool checkForPropertyChanges(const TransformerConstPtr &restoredTrafo,
            const ResolvedProductPtr &freshProduct);
    bool checkForPropertyChange(const Property &restoredProperty,
                                const PropertyMapConstPtr &newProperties);
    void replaceFileDependencyWithArtifact(const ResolvedProductPtr &fileDepProduct,
            FileDependency *filedep, Artifact *artifact);

    typedef QHash<const Artifact *, ArtifactList> ChildListHash;
    void rescueOldBuildData(const ResolvedProductConstPtr &restoredProduct,
                            const ResolvedProductPtr &newlyResolvedProduct,
                            const ProjectBuildData *oldBuildData,
                            const ChildListHash &childLists);

    RulesEvaluationContextPtr m_evalContext;
    BuildGraphLoadResult m_result;
    Logger m_logger;
    QProcessEnvironment m_environment;

    // These must only be deleted at the end so we can still peek into the old look-up table.
    QList<FileResourceBase *> m_objectsToDelete;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
