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
#ifndef QBS_BUILDGRAPHLOADER_H
#define QBS_BUILDGRAPHLOADER_H

#include "forward_decls.h"

#include "artifact.h"
#include "rescuableartifactdata.h"

#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/setupprojectparameters.h>

#include <QtCore/qprocess.h>
#include <QtCore/qvariant.h>

#include <unordered_map>

namespace qbs {

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
    BuildGraphLoader(Logger logger);
    ~BuildGraphLoader();

    BuildGraphLoadResult load(const TopLevelProjectPtr &existingProject,
                              const SetupProjectParameters &parameters,
                              const RulesEvaluationContextPtr &evalContext);

    static TopLevelProjectConstPtr loadProject(const QString &bgFilePath);

private:
    void loadBuildGraphFromDisk();
    bool checkBuildGraphCompatibility(const TopLevelProjectConstPtr &project);
    void trackProjectChanges();
    bool probeExecutionForced(const TopLevelProjectConstPtr &restoredProject,
                              const std::vector<ResolvedProductPtr> &restoredProducts) const;
    bool hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasCanonicalFilePathResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileExistsResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasDirectoryEntriesResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileLastModifiedResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasProductFileChanged(const std::vector<ResolvedProductPtr> &restoredProducts,
                               const FileTime &referenceTime,
                               Set<QString> &remainingBuildSystemFiles,
                               std::vector<ResolvedProductPtr> &productsWithChangedFiles);
    bool hasBuildSystemFileChanged(const Set<QString> &buildSystemFiles,
                                   const TopLevelProject *restoredProject);
    void markTransformersForChangeTracking(const std::vector<ResolvedProductPtr> &restoredProducts);
    void checkAllProductsForChanges(const std::vector<ResolvedProductPtr> &restoredProducts,
            std::vector<ResolvedProductPtr> &changedProducts);
    bool checkProductForChanges(const ResolvedProductPtr &restoredProduct,
                                const ResolvedProductPtr &newlyResolvedProduct);
    bool checkProductForChangesInSourceFiles(const ResolvedProductPtr &restoredProduct,
                                             const ResolvedProductPtr &newlyResolvedProduct);
    bool checkProductForInstallInfoChanges(const ResolvedProductPtr &restoredProduct,
                                           const ResolvedProductPtr &newlyResolvedProduct);
    bool checkForPropertyChanges(const ResolvedProductPtr &restoredProduct,
                                 const ResolvedProductPtr &newlyResolvedProduct);
    QVariantMap propertyMapByKind(const ResolvedProductConstPtr &product, const Property &property);
    void onProductRemoved(const ResolvedProductPtr &product, ProjectBuildData *projectBuildData,
                          bool removeArtifactsFromDisk = true);
    bool checkForPropertyChanges(const TransformerPtr &restoredTrafo,
            const ResolvedProductPtr &freshProduct);
    bool checkForPropertyChange(const Property &restoredProperty,
                                const QVariantMap &newProperties);
    void replaceFileDependencyWithArtifact(const ResolvedProductPtr &fileDepProduct,
            FileDependency *filedep, Artifact *artifact);
    bool checkConfigCompatibility();

    struct ChildrenInfo {
        ChildrenInfo() = default;
        ChildrenInfo(ArtifactSet c1, ArtifactSet c2)
            : children(std::move(c1)), childrenAddedByScanner(std::move(c2)) {}
        ArtifactSet children;
        ArtifactSet childrenAddedByScanner;
    };
    using ChildListHash = QHash<const Artifact *, ChildrenInfo>;
    void rescueOldBuildData(const ResolvedProductConstPtr &restoredProduct,
                            const ResolvedProductPtr &newlyResolvedProduct,
                            const ChildListHash &childLists,
                            const AllRescuableArtifactData &existingRad);

    QMap<QString, ResolvedProductPtr> m_freshProductsByName;
    RulesEvaluationContextPtr m_evalContext;
    SetupProjectParameters m_parameters;
    BuildGraphLoadResult m_result;
    Logger m_logger;
    QStringList m_artifactsRemovedFromDisk;
    std::unordered_map<QString, std::vector<SourceArtifactConstPtr>> m_changedSourcesByProduct;
    Set<QString> m_productsWhoseArtifactsNeedUpdate;
    qint64 m_wildcardExpansionEffort = 0;
    qint64 m_propertyComparisonEffort = 0;

    // These must only be deleted at the end so we can still peek into the old look-up table.
    QList<FileResourceBase *> m_objectsToDelete;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
