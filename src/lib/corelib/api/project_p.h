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
#ifndef QBS_PROJECT_P_H
#define QBS_PROJECT_P_H

#include "projectdata.h"
#include "rulecommand.h"
#include "transformerdata.h"

#include <language/language.h>
#include <logging/logger.h>

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

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
    ProjectPrivate(TopLevelProjectPtr internalProject, Logger logger)
        : internalProject(std::move(internalProject)), logger(std::move(logger))
    {
    }

    ProjectData projectData();
    BuildJob *buildProducts(const QVector<ResolvedProductPtr> &products, const BuildOptions &options,
                            bool needsDepencencyResolving,
                            QObject *jobOwner);
    CleanJob *cleanProducts(const QVector<ResolvedProductPtr> &products, const CleanOptions &options,
                            QObject *jobOwner);
    InstallJob *installProducts(const QVector<ResolvedProductPtr> &products,
                                const InstallOptions &options, bool needsDepencencyResolving,
                                QObject *jobOwner);
    QVector<ResolvedProductPtr> internalProducts(const QList<ProductData> &products) const;
    QVector<ResolvedProductPtr> allEnabledInternalProducts(bool includingNonDefault) const;
    ResolvedProductPtr internalProduct(const ProductData &product) const;
    ProductData findProductData(const ProductData &product) const;
    QList<ProductData> findProductsByName(const QString &name) const;
    GroupData findGroupData(const ProductData &product, const QString &groupName) const;

    GroupData createGroupDataFromGroup(const GroupPtr &resolvedGroup,
                                       const ResolvedProductConstPtr &product);
    ArtifactData createApiSourceArtifact(const SourceArtifactConstPtr &sa);
    ArtifactData createArtifactData(const Artifact *artifact,
                                    const ResolvedProductConstPtr &product,
                                    const ArtifactSet &targetArtifacts);
    void setupInstallData(ArtifactData &artifact, const ResolvedProductConstPtr &product);

    struct GroupUpdateContext {
        QVector<ResolvedProductPtr> resolvedProducts;
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
                                             const QStringList &filePaths, bool forAdding);

    void addGroup(const ProductData &product, const QString &groupName);
    void addFiles(const ProductData &product, const GroupData &group, const QStringList &filePaths);
    void removeFiles(const ProductData &product, const GroupData &group,
                     const QStringList &filePaths);
    void removeGroup(const ProductData &product, const GroupData &group);

    void prepareChangeToProject();

    RuleCommandList ruleCommandListForTransformer(const Transformer *transformer);
    RuleCommandList ruleCommands(const ProductData &product,
            const QString &inputFilePath, const QString &outputFileTag);
    ProjectTransformerData transformerData();

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
