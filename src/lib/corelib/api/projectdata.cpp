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
#include "projectdata.h"

#include "projectdata_p.h"
#include "propertymap_p.h"
#include <language/propertymapinternal.h>
#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>

#include <algorithm>

namespace qbs {

/*!
 * \class GroupData
 * \brief The \c GroupData class corresponds to the Group item in a qbs source file.
 */

GroupData::GroupData() : d(new Internal::GroupDataPrivate)
{
}

GroupData::GroupData(const GroupData &other) : d(other.d)
{
}

GroupData &GroupData::operator=(const GroupData &other)
{
    d = other.d;
    return *this;
}

GroupData::~GroupData()
{
}

/*!
 * \brief Returns true if and only if the Group holds data that was initialized by Qbs.
 */
bool GroupData::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The location at which the group is defined in the respective source file.
 */
CodeLocation GroupData::location() const
{
    return d->location;
}

/*!
 * \brief The name of the group.
 */
QString GroupData::name() const
{
    return d->name;
}

/*!
 * \brief The prefix of the group.
 */
QString GroupData::prefix() const
{
    return d->prefix;
}

/*!
 * \brief The files listed in the group item's "files" binding.
 * \note These do not include expanded wildcards.
 * \sa GroupData::sourceArtifactsFromWildcards
 */
QList<ArtifactData> GroupData::sourceArtifacts() const
{
    return d->sourceArtifacts;
}

/*!
 * \brief The list of files resulting from expanding all wildcard patterns in the group.
 */
QList<ArtifactData> GroupData::sourceArtifactsFromWildcards() const
{
    return d->sourceArtifactsFromWildcards;
}

/*!
 * \brief All files in this group, regardless of how whether they were given explicitly
 *        or via wildcards.
 * \sa GroupData::sourceArtifacts
 * \sa GroupData::sourceArtifactsFromWildcards
 */
QList<ArtifactData> GroupData::allSourceArtifacts() const
{
    return sourceArtifacts() + sourceArtifactsFromWildcards();
}

/*!
 * \brief The set of properties valid in this group.
 * Typically, most of them are inherited from the respective \c Product.
 */
PropertyMap GroupData::properties() const
{
    return d->properties;
}

/*!
 * \brief Returns true if this group is enabled in Qbs
 * This method returns the "condition" property of the \c Group definition. If the group is enabled
 * then the files in this group will be processed, provided the product it belongs to is also
 * enabled.
 *
 * Note that a group can be enabled, even if the product it belongs to is not. In this case
 * the files in the group will not be processed.
 * \sa ProductData::isEnabled()
 */
bool GroupData::isEnabled() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isEnabled;
}

/*!
 * \brief The paths of all files in this group.
 * \sa GroupData::allSourceArtifacts
 */
QStringList GroupData::allFilePaths() const
{
    const QList<ArtifactData> &artifacts = allSourceArtifacts();
    QStringList paths;
    paths.reserve(artifacts.count());
    std::transform(artifacts.constBegin(), artifacts.constEnd(), std::back_inserter(paths),
                          [](const ArtifactData &sa) { return sa.filePath(); });
    return paths;
}

bool operator!=(const GroupData &lhs, const GroupData &rhs)
{
    return !(lhs == rhs);
}

bool operator==(const GroupData &lhs, const GroupData &rhs)
{
    if (!lhs.isValid() && !rhs.isValid())
        return true;

    return lhs.isValid() == rhs.isValid()
            && lhs.name() == rhs.name()
            && lhs.location() == rhs.location()
            && lhs.sourceArtifactsFromWildcards() == rhs.sourceArtifactsFromWildcards()
            && lhs.sourceArtifacts() == rhs.sourceArtifacts()
            && lhs.properties() == rhs.properties()
            && lhs.isEnabled() == rhs.isEnabled();
}

bool operator<(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() < rhs.name();
}


/*!
 * \class ArtifactData
 * The \c ArtifactData class describes a file in a product. It is either a source file
 * or it gets generated during the build process.
 */

ArtifactData::ArtifactData() : d(new Internal::ArtifactDataPrivate)
{
}

ArtifactData::ArtifactData(const ArtifactData &other) : d(other.d)
{
}

ArtifactData &ArtifactData::operator=(const ArtifactData &other)
{
    d = other.d;
    return *this;
}

ArtifactData::~ArtifactData()
{
}

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool ArtifactData::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The full path of this file.
 */
QString ArtifactData::filePath() const
{
    return d->filePath;
}

/*!
 * \brief The tags of this file.
 * Typically, this list will contain just one element.
 */
QStringList ArtifactData::fileTags() const
{
    return d->fileTags;
}

bool ArtifactData::isGenerated() const
{
    return d->isGenerated;
}

/*!
 * \brief True if and only if this file is executable,
 * either natively or through an interpreter or shell.
 */
bool ArtifactData::isExecutable() const
{
    return d->fileTags.contains(QLatin1String("application"))
            || d->fileTags.contains(QLatin1String("msi"));
}

/*!
 * \brief True if and only if this artifact is a target artifact of its product.
 */
bool ArtifactData::isTargetArtifact() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isTargetArtifact;
}

/*!
 * \brief The properties of this file.
 */
PropertyMap ArtifactData::properties() const
{
    return d->properties;
}

/*!
    \brief The installation-related data of this artifact.
 */
InstallData ArtifactData::installData() const
{
    return d->installData;
}

bool operator==(const ArtifactData &ad1, const ArtifactData &ad2)
{
    return ad1.filePath() == ad2.filePath()
            && ad1.fileTags() == ad2.fileTags()
            && ad1.isGenerated() == ad2.isGenerated()
            && ad1.properties() == ad2.properties();
}

bool operator!=(const ArtifactData &ta1, const ArtifactData &ta2)
{
    return !(ta1 == ta2);
}

bool operator<(const ArtifactData &ta1, const ArtifactData &ta2)
{
    return ta1.filePath() < ta2.filePath();
}


/*!
 * \class InstallData
 * \brief The \c InstallData class provides the installation-related data of an artifact.
 */

InstallData::InstallData() : d(new Internal::InstallDataPrivate)
{
}

InstallData::InstallData(const InstallData &other) : d(other.d)
{
}

InstallData &InstallData::operator=(const InstallData &other)
{
    d = other.d;
    return *this;
}

InstallData::~InstallData()
{
}

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool InstallData::isValid() const
{
    return d->isValid;
}

/*!
  \brief Returns true if and only if \c{qbs.install} is \c true for the artifact.
 */
bool InstallData::isInstallable() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isInstallable;
}

/*!
  \brief Returns the directory into which the artifact will be installed.
  \note This is not necessarily the same as \c{qbs.installDir}, because \c{qbs.installSourceBase}
        might have been used.
 */
QString InstallData::installDir() const
{
    QBS_ASSERT(isValid(), return QString());
    return Internal::FileInfo::path(installFilePath());
}

/*!
  \brief Returns the installed file path of the artifact.
 */
QString InstallData::installFilePath() const
{
    QBS_ASSERT(isValid(), return QString());
    return d->installFilePath;
}

/*!
  \brief Returns the value of \c{qbs.installRoot} for the artifact.
 */
QString InstallData::installRoot() const
{
    QBS_ASSERT(isValid(), return QString());
    return d->installRoot;
}

/*!
  \brief Returns the local installation directory of the artifact, that is \c installDir()
         prepended by \c installRoot().
 */
QString InstallData::localInstallDir() const
{
    return QDir::cleanPath(installRoot() + QLatin1Char('/') + installDir());
}

/*!
  \brief Returns the local installed file path of the artifact, that is \c installFilePath()
         prepended by \c installRoot().
 */
QString InstallData::localInstallFilePath() const
{
    return QDir::cleanPath(installRoot() + QLatin1Char('/') + installFilePath());
}

/*!
 * \class ProductData
 * \brief The \c ProductData class corresponds to the Product item in a qbs source file.
 */

ProductData::ProductData() : d(new Internal::ProductDataPrivate)
{
}

ProductData::ProductData(const ProductData &other) : d(other.d)
{
}

ProductData &ProductData::operator=(const ProductData &other)
{
    d = other.d;
    return *this;
}

ProductData::~ProductData()
{
}

/*!
 * \brief Returns true if and only if the Product holds data that was initialized by Qbs.
 */
bool ProductData::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The product type, which is the list of file tags matching the product's target artifacts.
 */
QStringList ProductData::type() const
{
    return d->type;
}

/*!
 * \brief The names of dependent products.
 */
QStringList ProductData::dependencies() const
{
    return d->dependencies;
}

/*!
 * \brief The name of the product as given in the qbs source file.
 */
QString ProductData::name() const
{
    return d->name;
}

/*!
 * \brief The base name of the product's target file as given in the qbs source file.
 */
QString ProductData::targetName() const
{
    return d->targetName;
}

/*!
 * \brief The version number of the product.
 */
QString ProductData::version() const
{
    return d->version;
}

/*!
 * \brief The profile this product will be built for.
 */
QString ProductData::profile() const
{
    return d->profile;
}

QString ProductData::multiplexConfigurationId() const
{
   return d->multiplexConfigurationId;
}

/*!
 * \brief The location at which the product is defined in the source file.
 */
CodeLocation ProductData::location() const
{
    return d->location;
}

/*!
 * \brief The directory under which the product's generated artifacts are located.
 */
QString ProductData::buildDirectory() const
{
    return d->buildDirectory;
}

/*!
 * \brief All artifacts that are generated when building this product.
 */
QList<ArtifactData> ProductData::generatedArtifacts() const
{
    return d->generatedArtifacts;
}

/*!
  \brief This product's target artifacts.
  This is a subset of \c generatedArtifacts()
 */
QList<ArtifactData> ProductData::targetArtifacts() const
{
    QList<ArtifactData> list;
    std::copy_if(d->generatedArtifacts.constBegin(), d->generatedArtifacts.constEnd(),
                 std::back_inserter(list),
                 [](const ArtifactData &a) { return a.isTargetArtifact(); });
    return list;
}

/*!
 * \brief The list of artifacts in this product that are to be installed.
 */
QList<ArtifactData> ProductData::installableArtifacts() const
{
    QList<ArtifactData> artifacts;
    for (const GroupData &g : groups()) {
        for (const ArtifactData &a : g.allSourceArtifacts()) {
            if (a.installData().isInstallable())
                artifacts << a;
        }
    }
    for (const ArtifactData &a : targetArtifacts()) {
        if (a.installData().isInstallable())
            artifacts << a;
    }
    return artifacts;
}

/*!
 * \brief Returns the file path of the executable associated with this product.
 * If the product is not an application, an empty string is returned.
 */
QString ProductData::targetExecutable() const
{
    QBS_ASSERT(isValid(), return QString());
    if (d->moduleProperties.getModuleProperty(QLatin1String("bundle"),
                                              QLatin1String("isBundle")).toBool()) {
        for (const ArtifactData &ta : targetArtifacts()) {
            if (ta.fileTags().contains(QLatin1String("bundle.application-executable"))) {
                if (ta.installData().isInstallable())
                    return ta.installData().localInstallFilePath();
                return ta.filePath();
            }
        }
    }
    for (const ArtifactData &ta : targetArtifacts()) {
        if (ta.isExecutable()) {
            if (ta.installData().isInstallable())
                return ta.installData().localInstallFilePath();
            return ta.filePath();
        }
    }
    return QString();
}

/*!
 * \brief The list of \c GroupData in this product.
 */
QList<GroupData> ProductData::groups() const
{
    return d->groups;
}

/*!
 * \brief The product properties.
 */
QVariantMap ProductData::properties() const
{
    return d->properties;
}

/*!
 * \brief The set of properties inherited from dependent products and modules.
 */
PropertyMap ProductData::moduleProperties() const
{
    return d->moduleProperties;
}

/*!
 * \brief Returns true if this Product is enabled in Qbs.
 * This method returns the \c condition property of the \c Product definition. If a product is
 * enabled, then it will be built in the current configuration.
 * \sa GroupData::isEnabled()
 */
bool ProductData::isEnabled() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isEnabled;
}

bool ProductData::isRunnable() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isRunnable;
}

bool ProductData::isMultiplexed() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isMultiplexed;
}

bool operator==(const ProductData &lhs, const ProductData &rhs)
{
    if (!lhs.isValid() && !rhs.isValid())
        return true;

    return lhs.isValid() == rhs.isValid()
            && lhs.name() == rhs.name()
            && lhs.targetName() == rhs.targetName()
            && lhs.type() == rhs.type()
            && lhs.version() == rhs.version()
            && lhs.dependencies() == rhs.dependencies()
            && lhs.profile() == rhs.profile()
            && lhs.multiplexConfigurationId() == rhs.multiplexConfigurationId()
            && lhs.location() == rhs.location()
            && lhs.groups() == rhs.groups()
            && lhs.generatedArtifacts() == rhs.generatedArtifacts()
            && lhs.properties() == rhs.properties()
            && lhs.moduleProperties() == rhs.moduleProperties()
            && lhs.isEnabled() == rhs.isEnabled()
            && lhs.isMultiplexed() == rhs.isMultiplexed();
}

bool operator!=(const ProductData &lhs, const ProductData &rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ProductData &lhs, const ProductData &rhs)
{
    const int nameCmp = lhs.name().compare(rhs.name());
    if (nameCmp < 0)
        return true;
    if (nameCmp > 0)
        return false;
    return lhs.profile() < rhs.profile()
            && lhs.multiplexConfigurationId() < rhs.multiplexConfigurationId();
}

/*!
 * \class ProjectData
 * \brief The \c ProjectData class corresponds to the \c Project item in a qbs source file.
 */

/*!
 * \fn QList<ProductData> ProjectData::products() const
 * \brief The products in this project.
 */

ProjectData::ProjectData() : d(new Internal::ProjectDataPrivate)
{
}

ProjectData::ProjectData(const ProjectData &other) : d(other.d)
{
}

ProjectData &ProjectData::operator =(const ProjectData &other)
{
    d = other.d;
    return *this;
}

ProjectData::~ProjectData()
{
}

/*!
 * \brief Returns true if and only if the Project holds data that was initialized by Qbs.
 */
bool ProjectData::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The name of this project.
 */
QString ProjectData::name() const
{
    return d->name;
}

/*!
 * \brief The location at which the project is defined in a qbs source file.
 */
CodeLocation ProjectData::location() const
{
    return d->location;
}

/*!
 * \brief Whether the project is enabled.
 * \note Disabled projects never have any products or sub-projects.
 */
bool ProjectData::isEnabled() const
{
    QBS_ASSERT(isValid(), return false);
    return d->enabled;
}

/*!
 * \brief The base directory under which the build artifacts of this project will be created.
 * This is only valid for the top-level project.
 */
QString ProjectData::buildDirectory() const
{
    return d->buildDir;
}

/*!
 * The products in this project.
 * \note This also includes disabled products.
 */
QList<ProductData> ProjectData::products() const
{
    return d->products;
}

/*!
 * The sub-projects of this project.
 */
QList<ProjectData> ProjectData::subProjects() const
{
    return d->subProjects;
}

/*!
 * All products in this projects and its direct and indirect sub-projects.
 */
QList<ProductData> ProjectData::allProducts() const
{
    QList<ProductData> productList = products();
    for (const ProjectData &pd : subProjects())
        productList << pd.allProducts();
    return productList;
}

/*!
 * The artifacts of all products in this project that are to be installed.
 */
QList<ArtifactData> ProjectData::installableArtifacts() const
{
    QList<ArtifactData> artifacts;
    for (const ProductData &p : allProducts())
        artifacts << p.installableArtifacts();
    return artifacts;
}

bool operator==(const ProjectData &lhs, const ProjectData &rhs)
{
    if (!lhs.isValid() && !rhs.isValid())
        return true;

    return lhs.isValid() == rhs.isValid()
            && lhs.isEnabled() == rhs.isEnabled()
            && lhs.name() == rhs.name()
            && lhs.buildDirectory() == rhs.buildDirectory()
            && lhs.location() == rhs.location()
            && lhs.subProjects() == rhs.subProjects()
            && lhs.products() == rhs.products();
}

bool operator!=(const ProjectData &lhs, const ProjectData &rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ProjectData &lhs, const ProjectData &rhs)
{
    return lhs.name() < rhs.name();
}

/*!
 * \class PropertyMap
 * \brief The \c PropertyMap class represents the properties of a group or a product.
 */

PropertyMap::PropertyMap()
    : d(new Internal::PropertyMapPrivate)
{
    static Internal::PropertyMapPtr defaultInternalMap = Internal::PropertyMapInternal::create();
    d->m_map = defaultInternalMap;
}

PropertyMap::PropertyMap(const PropertyMap &other)
    : d(new Internal::PropertyMapPrivate(*other.d))
{
}

PropertyMap::~PropertyMap()
{
    delete d;
}

PropertyMap &PropertyMap::operator =(const PropertyMap &other)
{
    if (this != &other) {
        delete d;
        d = new Internal::PropertyMapPrivate(*other.d);
    }
    return *this;
}

/*!
 * \brief Returns the names of all properties.
 */
QStringList PropertyMap::allProperties() const
{
    QStringList properties;
    for (QVariantMap::ConstIterator it = d->m_map->value().constBegin();
            it != d->m_map->value().constEnd(); ++it) {
        if (!it.value().canConvert<QVariantMap>())
            properties << it.key();
    }
    return properties;
}

/*!
 * \brief Returns the value of the given property of a product or group.
 */
QVariant PropertyMap::getProperty(const QString &name) const
{
    return d->m_map->value().value(name);
}

/*!
 * \brief Convenience wrapper around \c PropertyMap::getModuleProperty for properties of list type.
 *
 */
QStringList PropertyMap::getModulePropertiesAsStringList(const QString &moduleName,
                                                          const QString &propertyName) const
{
    const QVariantList &vl = d->m_map->moduleProperty(moduleName, propertyName).toList();
    QStringList sl;
    for (const QVariant &v : vl) {
        QBS_ASSERT(v.canConvert<QString>(), continue);
        sl << v.toString();
    }
    return sl;
}

/*!
 * \brief Returns the value of the given module property.
 */
QVariant PropertyMap::getModuleProperty(const QString &moduleName,
                                        const QString &propertyName) const
{
    return d->m_map->moduleProperty(moduleName, propertyName);
}

static QString mapToString(const QVariantMap &map, const QString &prefix)
{
    QStringList keys(map.keys());
    qSort(keys);
    QString stringRep;
    for (const QString &key : qAsConst(keys)) {
        const QVariant &val = map.value(key);
        if (val.type() == QVariant::Map) {
            stringRep += mapToString(val.value<QVariantMap>(), prefix + key + QLatin1Char('.'));
        } else {
            stringRep += QString::fromLatin1("%1%2: %3\n")
                    .arg(prefix, key, toJSLiteral(val));
        }
    }
    return stringRep;
}

QString PropertyMap::toString() const
{
    return mapToString(d->m_map->value(), QString());
}

bool operator==(const PropertyMap &pm1, const PropertyMap &pm2)
{
    return *pm1.d->m_map == *pm2.d->m_map;
}

bool operator!=(const PropertyMap &pm1, const PropertyMap &pm2)
{
    return !(*pm1.d->m_map == *pm2.d->m_map);
}

} // namespace qbs
