/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#ifndef QBS_BUILDGRAPHLOADER_H
#define QBS_BUILDGRAPHLOADER_H

#include "forward_decls.h"

#include "rescuableartifactdata.h"

#include <buildgraph/artifactset.h>
#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/setupprojectparameters.h>

#include <QProcessEnvironment>
#include <QVariantMap>

namespace qbs {

namespace Internal {
class FileDependency;
class FileResourceBase;
class FileTime;
class NodeSet;
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

    BuildGraphLoadResult load(const TopLevelProjectPtr &existingProject,
                              const SetupProjectParameters &parameters,
                              const RulesEvaluationContextPtr &evalContext);

private:
    void loadBuildGraphFromDisk();
    void checkBuildGraphCompatibility(const TopLevelProjectConstPtr &project);
    void trackProjectChanges();
    bool hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasCanonicalFilePathResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileExistsResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileLastModifiedResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasProductFileChanged(const QList<ResolvedProductPtr> &restoredProducts,
                               const FileTime &referenceTime,
                               QSet<QString> &remainingBuildSystemFiles,
                               QList<ResolvedProductPtr> &productsWithChangedFiles);
    bool hasBuildSystemFileChanged(const QSet<QString> &buildSystemFiles,
                                   const FileTime &referenceTime);
    void checkAllProductsForChanges(const QList<ResolvedProductPtr> &restoredProducts,
            const QMap<QString, ResolvedProductPtr> &newlyResolvedProductsByName,
            QList<ResolvedProductPtr> &changedProducts);
    bool checkProductForChanges(const ResolvedProductPtr &restoredProduct,
                                const ResolvedProductPtr &newlyResolvedProduct);
    bool checkProductForInstallInfoChanges(const ResolvedProductPtr &restoredProduct,
                                           const ResolvedProductPtr &newlyResolvedProduct);
    bool checkForPropertyChanges(const ResolvedProductPtr &restoredProduct,
                                 const ResolvedProductPtr &newlyResolvedProduct);
    bool checkTransformersForPropertyChanges(const ResolvedProductPtr &restoredProduct,
                                             const ResolvedProductPtr &newlyResolvedProduct);
    void onProductRemoved(const ResolvedProductPtr &product, ProjectBuildData *projectBuildData,
                          bool removeArtifactsFromDisk = true);
    bool checkForPropertyChanges(const TransformerPtr &restoredTrafo,
            const ResolvedProductPtr &freshProduct);
    bool checkForPropertyChange(const Property &restoredProperty,
                                const QVariantMap &newProperties);
    void replaceFileDependencyWithArtifact(const ResolvedProductPtr &fileDepProduct,
            FileDependency *filedep, Artifact *artifact);
    bool isConfigCompatible();

    struct ChildrenInfo {
        ChildrenInfo() {}
        ChildrenInfo(const ArtifactSet &c1, const ArtifactSet &c2)
            : children(c1), childrenAddedByScanner(c2) {}
        ArtifactSet children;
        ArtifactSet childrenAddedByScanner;
    };
    typedef QHash<const Artifact *, ChildrenInfo> ChildListHash;
    void rescueOldBuildData(const ResolvedProductConstPtr &restoredProduct,
                            const ResolvedProductPtr &newlyResolvedProduct,
                            ProjectBuildData *oldBuildData,
                            const ChildListHash &childLists,
                            const AllRescuableArtifactData &existingRad);

    RulesEvaluationContextPtr m_evalContext;
    SetupProjectParameters m_parameters;
    BuildGraphLoadResult m_result;
    Logger m_logger;
    QProcessEnvironment m_environment;
    QStringList m_artifactsRemovedFromDisk;

    // These must only be deleted at the end so we can still peek into the old look-up table.
    QList<FileResourceBase *> m_objectsToDelete;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
