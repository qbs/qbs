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
#ifndef QBS_PROJECTDATA_H
#define QBS_PROJECTDATA_H

#include "../tools/codelocation.h"
#include "../tools/qbs_export.h"

#include <QtCore/qshareddata.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <utility>

namespace qbs {
namespace Internal {
class ArtifactDataPrivate;
class GroupDataPrivate;
class InstallDataPrivate;
class ProductDataPrivate;
class ProjectPrivate;
class ProjectDataPrivate;
class PropertyMapPrivate;
} // namespace Internal

class PropertyMap;

QBS_EXPORT bool operator==(const PropertyMap &pm1, const PropertyMap &pm2);
QBS_EXPORT bool operator!=(const PropertyMap &pm1, const PropertyMap &pm2);

class QBS_EXPORT PropertyMap
{
    friend class Internal::ProjectPrivate;
    friend QBS_EXPORT bool operator==(const PropertyMap &, const PropertyMap &);
    friend QBS_EXPORT bool operator!=(const PropertyMap &, const PropertyMap &);

public:
    PropertyMap();
    PropertyMap(const PropertyMap &other);
    ~PropertyMap();

    PropertyMap &operator =(const PropertyMap &other);

    QStringList allProperties() const;
    QVariant getProperty(const QString &name) const;

    QStringList getModulePropertiesAsStringList(const QString &moduleName,
                                                 const QString &propertyName) const;
    QVariant getModuleProperty(const QString &moduleName, const QString &propertyName) const;

    // For debugging.
    QString toString() const;

private:
    Internal::PropertyMapPrivate *d;
};

class InstallData;

class QBS_EXPORT ArtifactData
{
    friend class Internal::ProjectPrivate;
public:
    ArtifactData();
    ArtifactData(const ArtifactData &other);
    ArtifactData &operator=(const ArtifactData &other);
    ~ArtifactData();

    bool isValid() const;

    QString filePath() const;
    QStringList fileTags() const;
    bool isGenerated() const;
    bool isExecutable() const;
    bool isTargetArtifact() const;
    PropertyMap properties() const;
    InstallData installData() const;

private:
    QExplicitlySharedDataPointer<Internal::ArtifactDataPrivate> d;
};

class QBS_EXPORT InstallData
{
    friend class Internal::ProjectPrivate;
public:
    InstallData();
    InstallData(const InstallData &other);
    InstallData &operator=(const InstallData &other);
    ~InstallData();

    bool isValid() const;

    bool isInstallable() const;
    QString installDir() const;
    QString installFilePath() const;
    QString installRoot() const;
    QString localInstallDir() const;
    QString localInstallFilePath() const;
private:
    QExplicitlySharedDataPointer<Internal::InstallDataPrivate> d;
};

QBS_EXPORT bool operator==(const ArtifactData &ta1, const ArtifactData &ta2);
QBS_EXPORT bool operator!=(const ArtifactData &ta1, const ArtifactData &ta2);
QBS_EXPORT bool operator<(const ArtifactData &ta1, const ArtifactData &ta2);

class QBS_EXPORT GroupData
{
    friend class Internal::ProjectPrivate;
public:
    GroupData();
    GroupData(const GroupData &other);
    GroupData &operator=(const GroupData &other);
    ~GroupData();

    bool isValid() const;

    CodeLocation location() const;
    QString name() const;
    QString prefix() const;
    QList<ArtifactData> sourceArtifacts() const;
    QList<ArtifactData> sourceArtifactsFromWildcards() const;
    QList<ArtifactData> allSourceArtifacts() const;
    PropertyMap properties() const;
    bool isEnabled() const;
    QStringList allFilePaths() const;

private:
    QExplicitlySharedDataPointer<Internal::GroupDataPrivate> d;
};

QBS_EXPORT bool operator==(const GroupData &lhs, const GroupData &rhs);
QBS_EXPORT bool operator!=(const GroupData &lhs, const GroupData &rhs);
QBS_EXPORT bool operator<(const GroupData &lhs, const GroupData &rhs);

class QBS_EXPORT ProductData
{
    friend class Internal::ProjectPrivate;
public:
    ProductData();
    ProductData(const ProductData &other);
    ProductData &operator=(const ProductData &other);
    ~ProductData();

    bool isValid() const;

    QStringList type() const;
    QStringList dependencies() const;
    QString name() const;
    QString targetName() const;
    QString version() const;
    QString profile() const;
    QString multiplexConfigurationId() const;
    CodeLocation location() const;
    QString buildDirectory() const;
    QList<ArtifactData> generatedArtifacts() const;
    QList<ArtifactData> targetArtifacts() const;
    QList<ArtifactData> installableArtifacts() const;
    QString targetExecutable() const;
    QList<GroupData> groups() const;
    QVariantMap properties() const;
    PropertyMap moduleProperties() const;
    bool isEnabled() const;
    bool isRunnable() const;
    bool isMultiplexed() const;

private:
    QExplicitlySharedDataPointer<Internal::ProductDataPrivate> d;
};

QBS_EXPORT bool operator==(const ProductData &lhs, const ProductData &rhs);
QBS_EXPORT bool operator!=(const ProductData &lhs, const ProductData &rhs);
QBS_EXPORT bool operator<(const ProductData &lhs, const ProductData &rhs);

class QBS_EXPORT ProjectData
{
    friend class Internal::ProjectPrivate;
public:
    ProjectData();
    ProjectData(const ProjectData &other);
    ProjectData &operator=(const ProjectData &other);
    ~ProjectData();

    bool isValid() const;

    QString name() const;
    CodeLocation location() const;
    bool isEnabled() const;
    QString buildDirectory() const;
    QList<ProductData> products() const;
    QList<ProjectData> subProjects() const;
    QList<ProductData> allProducts() const;
    QList<ArtifactData> installableArtifacts() const;

private:
    QExplicitlySharedDataPointer<Internal::ProjectDataPrivate> d;
};

QBS_EXPORT bool operator==(const ProjectData &lhs, const ProjectData &rhs);
QBS_EXPORT bool operator!=(const ProjectData &lhs, const ProjectData &rhs);
QBS_EXPORT bool operator<(const ProjectData &lhs, const ProjectData &rhs);

} // namespace qbs

#endif // QBS_PROJECTDATA_H
