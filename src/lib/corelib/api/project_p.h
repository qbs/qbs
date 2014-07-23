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
#ifndef QBS_PROJECT_P_H
#define QBS_PROJECT_P_H

#include "projectdata.h"

#include <language/language.h>
#include <logging/logger.h>

#include <QObject>
#include <QStringList>

namespace qbs {
class BuildJob;
class BuildOptions;
class CleanJob;
class CleanOptions;
class InstallJob;
class InstallOptions;

namespace Internal {

class ProjectPrivate : public QSharedData
{
public:
    ProjectPrivate(const TopLevelProjectPtr &internalProject, const Logger &logger)
        : internalProject(internalProject), logger(logger)
    {
    }

    ProjectData projectData();
    BuildJob *buildProducts(const QList<ResolvedProductPtr> &products, const BuildOptions &options,
                            bool needsDepencencyResolving,
                            QObject *jobOwner);
    CleanJob *cleanProducts(const QList<ResolvedProductPtr> &products, const CleanOptions &options,
                            QObject *jobOwner);
    InstallJob *installProducts(const QList<ResolvedProductPtr> &products,
                                const InstallOptions &options, bool needsDepencencyResolving,
                                QObject *jobOwner);
    QList<ResolvedProductPtr> internalProducts(const QList<ProductData> &products) const;
    QList<ResolvedProductPtr> allEnabledInternalProducts() const;
    ResolvedProductPtr internalProduct(const ProductData &product) const;
    ProductData findProductData(const ProductData &product) const;
    QList<ProductData> findProductsByName(const QString &name) const;
    GroupData findGroupData(const ProductData &product, const QString &groupName) const;

    GroupData createGroupDataFromGroup(const GroupPtr &resolvedGroup);

    struct GroupUpdateContext {
        QList<ResolvedProductPtr> resolvedProducts;
        QList<GroupPtr> resolvedGroups;
        QList<ProductData> products;
        QList<GroupData> groups;
    };

    struct FileListUpdateContext {
        GroupUpdateContext groupContext;
        QStringList absoluteFilePaths;
        QStringList relativeFilePaths;
        QStringList absoluteFilePathsFromWildcards; // Not included in the other two lists.
    };

    GroupUpdateContext getGroupContext(const ProductData &product, const GroupData &group);
    FileListUpdateContext getFileListContext(const ProductData &product, const GroupData &group,
                                             const QStringList &filePaths);

    void addGroup(const ProductData &product, const QString &groupName);
    void addFiles(const ProductData &product, const GroupData &group, const QStringList &filePaths);
    void removeFiles(const ProductData &product, const GroupData &group,
                     const QStringList &filePaths);
    void removeGroup(const ProductData &product, const GroupData &group);
    void removeFilesFromBuildGraph(const ResolvedProductConstPtr &product,
                                   const QList<SourceArtifactPtr> &files);
    void updateInternalCodeLocations(const ResolvedProjectPtr &project,
                                     const CodeLocation &changeLocation, int lineOffset);
    void updateExternalCodeLocations(const ProjectData &project,
                                     const CodeLocation &changeLocation, int lineOffset);
    void prepareChangeToProject();

    TopLevelProjectPtr internalProject;
    Logger logger;

private:
    void retrieveProjectData(ProjectData &projectData,
                             const ResolvedProjectConstPtr &internalProject);

    ProjectData m_projectData;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
