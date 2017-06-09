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
    BuildGraphLoader(const Logger &logger);
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
                              const QList<ResolvedProductPtr> &restoredProducts) const;
    bool hasEnvironmentChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasCanonicalFilePathResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileExistsResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasDirectoryEntriesResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasFileLastModifiedResultChanged(const TopLevelProjectConstPtr &restoredProject) const;
    bool hasProductFileChanged(const QList<ResolvedProductPtr> &restoredProducts,
                               const FileTime &referenceTime,
                               Set<QString> &remainingBuildSystemFiles,
                               QList<ResolvedProductPtr> &productsWithChangedFiles);
    bool hasBuildSystemFileChanged(const Set<QString> &buildSystemFiles,
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
    bool checkTransformersForChanges(const ResolvedProductPtr &restoredProduct,
                                             const ResolvedProductPtr &newlyResolvedProduct);
    void onProductRemoved(const ResolvedProductPtr &product, ProjectBuildData *projectBuildData,
                          bool removeArtifactsFromDisk = true);
    bool checkForPropertyChanges(const TransformerPtr &restoredTrafo,
            const ResolvedProductPtr &freshProduct);
    bool checkForEnvChanges(const TransformerPtr &restoredTrafo);
    bool checkForPropertyChange(const Property &restoredProperty,
                                const QVariantMap &newProperties);
    void replaceFileDependencyWithArtifact(const ResolvedProductPtr &fileDepProduct,
            FileDependency *filedep, Artifact *artifact);
    bool checkConfigCompatibility();
    bool isPrepareScriptUpToDate(const ScriptFunctionConstPtr &script,
                                 const FileTime &referenceTime);

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
                            const ChildListHash &childLists,
                            const AllRescuableArtifactData &existingRad);

    RulesEvaluationContextPtr m_evalContext;
    SetupProjectParameters m_parameters;
    BuildGraphLoadResult m_result;
    Logger m_logger;
    QStringList m_artifactsRemovedFromDisk;
    qint64 m_wildcardExpansionEffort;
    qint64 m_propertyComparisonEffort;

    // These must only be deleted at the end so we can still peek into the old look-up table.
    QList<FileResourceBase *> m_objectsToDelete;

    bool m_envChange = false;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
