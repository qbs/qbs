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

#include <memory>
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
    PropertyMap(PropertyMap &&other) Q_DECL_NOEXCEPT;
    ~PropertyMap();

    PropertyMap &operator =(const PropertyMap &other);
    PropertyMap &operator =(PropertyMap &&other) Q_DECL_NOEXCEPT;

    QStringList allProperties() const;
    QVariant getProperty(const QString &name) const;

    QStringList getModulePropertiesAsStringList(const QString &moduleName,
                                                 const QString &propertyName) const;
    QVariant getModuleProperty(const QString &moduleName, const QString &propertyName) const;

    // For debugging.
    QString toString() const;

private:
    std::unique_ptr<Internal::PropertyMapPrivate> d;
};

class InstallData;

class QBS_EXPORT ArtifactData
{
    friend class Internal::ProjectPrivate;
public:
    ArtifactData();
    ArtifactData(const ArtifactData &other);
    ArtifactData(ArtifactData &&) Q_DECL_NOEXCEPT;
    ArtifactData &operator=(const ArtifactData &other);
    ArtifactData &operator=(ArtifactData &&) Q_DECL_NOEXCEPT;
    ~ArtifactData();

    bool isValid() const;
    QJsonObject toJson(const QStringList &moduleProperties = {}) const;

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
    InstallData(InstallData &&) Q_DECL_NOEXCEPT;
    InstallData &operator=(const InstallData &other);
    InstallData &operator=(InstallData &&) Q_DECL_NOEXCEPT;
    ~InstallData();

    bool isValid() const;
    QJsonObject toJson() const;

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
    GroupData(GroupData &&) Q_DECL_NOEXCEPT;
    GroupData &operator=(const GroupData &other);
    GroupData &operator=(GroupData &&) Q_DECL_NOEXCEPT;
    ~GroupData();

    bool isValid() const;
    QJsonObject toJson(const QStringList &moduleProperties = {}) const;

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
    ProductData(ProductData &&) Q_DECL_NOEXCEPT;
    ProductData &operator=(const ProductData &other);
    ProductData &operator=(ProductData &&) Q_DECL_NOEXCEPT;
    ~ProductData();

    bool isValid() const;
    QJsonObject toJson(const QStringList &propertyNames = {}) const;

    const QStringList &type() const;
    const QStringList &dependencies() const;
    const QString &name() const;
    QString fullDisplayName() const;
    const QString &targetName() const;
    const QString &version() const;
    QString profile() const;
    const QString &multiplexConfigurationId() const;
    const CodeLocation &location() const;
    const QString &buildDirectory() const;
    const QList<ArtifactData> &generatedArtifacts() const;
    const QList<ArtifactData> targetArtifacts() const;
    const QList<ArtifactData> installableArtifacts() const;
    QString targetExecutable() const;
    const QList<GroupData> &groups() const;
    const QVariantMap &properties() const;
    const PropertyMap &moduleProperties() const;
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
    ProjectData(ProjectData &&) Q_DECL_NOEXCEPT;
    ProjectData &operator=(const ProjectData &other);
    ProjectData &operator=(ProjectData &&) Q_DECL_NOEXCEPT;
    ~ProjectData();

    bool isValid() const;
    QJsonObject toJson(const QStringList &moduleProperties = {}) const;

    const QString &name() const;
    const CodeLocation &location() const;
    bool isEnabled() const;
    const QString &buildDirectory() const;
    const QList<ProductData> &products() const;
    const QList<ProjectData> &subProjects() const;
    const QList<ProductData> allProducts() const;
    const QList<ArtifactData> installableArtifacts() const;

private:
    QExplicitlySharedDataPointer<Internal::ProjectDataPrivate> d;
};

QBS_EXPORT bool operator==(const ProjectData &lhs, const ProjectData &rhs);
QBS_EXPORT bool operator!=(const ProjectData &lhs, const ProjectData &rhs);
QBS_EXPORT bool operator<(const ProjectData &lhs, const ProjectData &rhs);

} // namespace qbs

#endif // QBS_PROJECTDATA_H
