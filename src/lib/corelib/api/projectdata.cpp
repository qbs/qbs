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
#include "projectdata.h"

#include "projectdata_p.h"
#include "propertymap_p.h"
#include <language/propertymapinternal.h>
#include <tools/fileinfo.h>
#include <tools/propertyfinder.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>

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
 * \brief The files listed in the group item's "files" binding.
 * \note These do not include expanded wildcards.
 * \sa GroupData::sourceArtifactsFromWildcards
 */
QList<SourceArtifact> GroupData::sourceArtifacts() const
{
    return d->sourceArtifacts;
}

/*!
 * \brief The list of files resulting from expanding all wildcard patterns in the group.
 */
QList<SourceArtifact> GroupData::sourceArtifactsFromWildcards() const
{
    return d->sourceArtifactsFromWildcards;
}

/*!
 * \brief All files in this group, regardless of how whether they were given explicitly
 *        or via wildcards.
 * \sa GroupData::sourceArtifacts
 * \sa GroupData::sourceArtifactsFromWildcards
 */
QList<SourceArtifact> GroupData::allSourceArtifacts() const
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
    const QList<SourceArtifact> &artifacts = allSourceArtifacts();
    QStringList paths;
    paths.reserve(artifacts.count());
    std::transform(artifacts.constBegin(), artifacts.constEnd(), std::back_inserter(paths),
                          [](const SourceArtifact &sa) { return sa.filePath(); });
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
 * \class SourceArtifact
 * \brief The \c SourceArtifact class describes a source file in a product.
 */

SourceArtifact::SourceArtifact() : d(new Internal::SourceArtifactPrivate)
{
}

SourceArtifact::SourceArtifact(const SourceArtifact &other) : d(other.d)
{
}

SourceArtifact &SourceArtifact::operator=(const SourceArtifact &other)
{
    d = other.d;
    return *this;
}

SourceArtifact::~SourceArtifact()
{
}

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool SourceArtifact::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The full path of this file.
 */
QString SourceArtifact::filePath() const
{
    return d->filePath;
}

/*!
 * \brief The tags of this file.
 * Typically, this list will contain just one element.
 */
QStringList SourceArtifact::fileTags() const
{
    return d->fileTags;
}

bool operator==(const SourceArtifact &sa1, const SourceArtifact &sa2)
{
    return sa1.filePath() == sa2.filePath() && sa1.fileTags() == sa2.fileTags();
}

bool operator!=(const SourceArtifact &sa1, const SourceArtifact &sa2)
{
    return !(sa1 == sa2);
}

bool operator<(const SourceArtifact &sa1, const SourceArtifact &sa2)
{
    return sa1.filePath() < sa2.filePath();
}


/*!
 * \class TargetArtifact
 * \brief The \c TargetArtifact class describes a top-level build result of a product.
 * For instance, the target artifact of a product with type "application" is an executable file.
 */

TargetArtifact::TargetArtifact() : d(new Internal::TargetArtifactPrivate)
{
}

TargetArtifact::TargetArtifact(const TargetArtifact &other) : d(other.d)
{
}

TargetArtifact &TargetArtifact::operator=(const TargetArtifact &other)
{
    d = other.d;
    return *this;
}

TargetArtifact::~TargetArtifact()
{
}

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool TargetArtifact::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The full path of this file.
 */
QString TargetArtifact::filePath() const
{
    return d->filePath;
}

/*!
 * \brief The tags of this file.
 * Typically, this list will contain just one element.
 */
QStringList TargetArtifact::fileTags() const
{
    return d->fileTags;
}

/*!
 * \brief True if and only if this file is executable,
 * either natively or through an interpreter or shell.
 */
bool TargetArtifact::isExecutable() const
{
    return d->fileTags.contains(QLatin1String("application"))
            || d->fileTags.contains(QLatin1String("msi"));
}

/*!
 * \brief The properties of this file.
 */
PropertyMap TargetArtifact::properties() const
{
    return d->properties;
}

bool operator==(const TargetArtifact &ta1, const TargetArtifact &ta2)
{
    return ta1.filePath() == ta2.filePath()
            && ta1.fileTags() == ta2.fileTags()
            && ta1.properties() == ta2.properties();
}

bool operator!=(const TargetArtifact &ta1, const TargetArtifact &ta2)
{
    return !(ta1 == ta2);
}

bool operator<(const TargetArtifact &ta1, const TargetArtifact &ta2)
{
    return ta1.filePath() < ta2.filePath();
}


/*!
 * \class InstallableFile
 * \brief Describes a file that is marked for installation.
 */

InstallableFile::InstallableFile() : d(new Internal::InstallableFilePrivate)
{
}

InstallableFile::InstallableFile(const InstallableFile &other) : d(other.d)
{
}

InstallableFile &InstallableFile::operator=(const InstallableFile &other)
{
    d = other.d;
    return *this;
}

InstallableFile::~InstallableFile()
{
}

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool InstallableFile::isValid() const
{
    return d->isValid;
}

/*!
 * \brief The location of the file from which it will be copied to \c targetFilePath()
 *        on installation.
 */
QString InstallableFile::sourceFilePath() const
{
    return d->sourceFilePath;
}

/*!
 * \brief The file path that this file will be copied to on installation.
 */
QString InstallableFile::targetFilePath() const
{
    return d->targetFilePath;
}

/*!
 * \brief The file's tags.
 */
QStringList InstallableFile::fileTags() const
{
    return d->fileTags;
}

/*!
 * \brief True if and only if the file is an executable.
 */
bool InstallableFile::isExecutable() const
{
    return d->fileTags.contains(QLatin1String("application"));
}

bool operator==(const InstallableFile &file1, const InstallableFile &file2)
{
    return file1.sourceFilePath() == file2.sourceFilePath()
            && file1.targetFilePath() == file2.targetFilePath()
            && file1.fileTags() == file2.fileTags();
}

bool operator!=(const InstallableFile &file1, const InstallableFile &file2)
{
    return !(file1 == file2);
}

bool operator<(const InstallableFile &file1, const InstallableFile &file2)
{
    return file1.sourceFilePath() < file2.sourceFilePath();
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

/*!
 * \brief The location at which the product is defined in the source file.
 */
CodeLocation ProductData::location() const
{
    return d->location;
}

/*!
 * \brief This product's target artifacts.
 */
QList<TargetArtifact> ProductData::targetArtifacts() const
{
    return d->targetArtifacts;
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
            && lhs.location() == rhs.location()
            && lhs.groups() == rhs.groups()
            && lhs.targetArtifacts() == rhs.targetArtifacts()
            && lhs.properties() == rhs.properties()
            && lhs.isEnabled() == rhs.isEnabled();
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
    return lhs.profile() < rhs.profile();
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
    foreach (const ProjectData &pd, subProjects())
        productList << pd.allProducts();
    return productList;
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
    delete d;
    d = new Internal::PropertyMapPrivate(*other.d);
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
 * \brief Returns the values of the given module property.
 * This function is intended for properties of list type, such as "cpp.includes".
 * The values will be gathered both directly from the product/group as well as from the
 * product's module dependencies.
 */
QVariantList PropertyMap::getModuleProperties(const QString &moduleName,
                                              const QString &propertyName) const
{
    return Internal::PropertyFinder().propertyValues(d->m_map->value(), moduleName, propertyName);
}

/*!
 * \brief Convenience function for \c PropertyMap::getModuleProperties.
 */
QStringList PropertyMap::getModulePropertiesAsStringList(const QString &moduleName,
                                                          const QString &propertyName) const
{
    const QVariantList &vl = getModuleProperties(moduleName, propertyName);
    QStringList sl;
    foreach (const QVariant &v, vl) {
        QBS_ASSERT(v.canConvert<QString>(), continue);
        sl << v.toString();
    }
    return sl;
}

/*!
 * \brief Returns the value of the given module property.
 * This function is intended for properties of "integral" type, such as "qbs.targetOS".
 * The property will be looked up first at the product or group itself. If it is not found there,
 * the module dependencies are searched in undefined order.
 */
QVariant PropertyMap::getModuleProperty(const QString &moduleName,
                                        const QString &propertyName) const
{
    return Internal::PropertyFinder().propertyValue(d->m_map->value(), moduleName, propertyName);
}

static QString mapToString(const QVariantMap &map, const QString &prefix)
{
    QStringList keys(map.keys());
    qSort(keys);
    QString stringRep;
    foreach (const QString &key, keys) {
        const QVariant &val = map.value(key);
        if (val.type() == QVariant::Map) {
            stringRep += mapToString(val.value<QVariantMap>(), prefix + key + QLatin1Char('.'));
        } else {
            stringRep += QString::fromLocal8Bit("%1%2: %3\n")
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
    return pm1.d->m_map->value() == pm2.d->m_map->value();
}

bool operator!=(const PropertyMap &pm1, const PropertyMap &pm2)
{
    return !(pm1.d->m_map->value() == pm2.d->m_map->value());
}

} // namespace qbs
