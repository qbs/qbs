/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
#include "projectdata.h"

#include "projectdata_p.h"
#include "propertymap_p.h"
#include <language/propertymapinternal.h>
#include <tools/propertyfinder.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>

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
 * \sa GroupData::expandedWildcards
 */
QStringList GroupData::filePaths() const
{
    return d->filePaths;
}

/*!
 * \brief The list of files resulting from expanding all wildcard patterns in the group.
 */
QStringList GroupData::expandedWildcards() const
{
    return d->expandedWildcards;
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
 * \fn QStringList GroupData::allFilePaths() const
 * \brief All files in this group, regardless of how whether they were given explicitly
 *        or via wildcards.
 * \sa GroupData::filePaths
 * \sa GroupData::expandedWildcards
 */
QStringList GroupData::allFilePaths() const
{
    return d->filePaths + d->expandedWildcards;
}

bool operator!=(const GroupData &lhs, const GroupData &rhs)
{
    return !(lhs == rhs);
}

bool operator==(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() == rhs.name()
            && lhs.location() == rhs.location()
            && lhs.expandedWildcards() == rhs.expandedWildcards()
            && lhs.filePaths() == rhs.filePaths()
            && lhs.properties() == rhs.properties()
            && lhs.isEnabled() == rhs.isEnabled();
}

bool operator<(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() < rhs.name();
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
 * \brief The name of the product as given in the qbs source file.
 */
QString ProductData::name() const
{
    return d->name;
}

/*!
 * \brief The location at which the product is defined in the source file.
 */
CodeLocation ProductData::location() const
{
    return d->location;
}

/*!
 * \brief The file tags of this product. Corresponds to a Product's "type" property in
 *        a qbs source file.
 */
QStringList ProductData::fileTags() const
{
    return d->fileTags;
}

/*!
 * \brief The set of properties valid in this product.
 * \note product properties can be overwritten in a group.
 * \sa GroupData::properties()
 */
PropertyMap ProductData::properties() const
{
    return d->properties;
}

/*!
 * \brief The list of \c GroupData in this product.
 */
QList<GroupData> ProductData::groups() const
{
    return d->groups;
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

bool operator==(const ProductData &lhs, const ProductData &rhs)
{
    return lhs.name() == rhs.name()
            && lhs.location() == rhs.location()
            && lhs.fileTags() == rhs.fileTags()
            && lhs.properties() == rhs.properties()
            && lhs.groups() == rhs.groups()
            && lhs.isEnabled() == rhs.isEnabled();
}

bool operator!=(const ProductData &lhs, const ProductData &rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ProductData &lhs, const ProductData &rhs)
{
    return lhs.name() < rhs.name();
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
    return lhs.location() == rhs.location()
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
                    .arg(prefix, key, Internal::toJSLiteral(val));
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
