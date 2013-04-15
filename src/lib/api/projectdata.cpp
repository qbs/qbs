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

#include "propertymap_p.h"
#include <language/propertymapinternal.h>
#include <tools/propertyfinder.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>

namespace qbs {

GroupData::GroupData() { }

bool operator==(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() == rhs.name()
            && lhs.location() == rhs.location()
            && lhs.expandedWildcards() == rhs.expandedWildcards()
            && lhs.filePaths() == rhs.filePaths()
            && lhs.properties() == rhs.properties()
            && lhs.isEnabled() == rhs.isEnabled();
}

bool operator!=(const GroupData &lhs, const GroupData &rhs)
{
    return !(lhs == rhs);
}

ProductData::ProductData() { }

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

bool operator<(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() < rhs.name();
}

ProjectData::ProjectData() { }

bool operator==(const ProjectData &lhs, const ProjectData &rhs)
{
    return lhs.location() == rhs.location()
            && lhs.products() == rhs.products();
}

bool operator!=(const ProjectData &lhs, const ProjectData &rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ProductData &lhs, const ProductData &rhs)
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

/*!
 * \class GroupData
 * \brief The \c GroupData class corresponds to the Group item in a qbs source file.
 */

 /*!
  * \fn QString GroupData::name() const
  * \brief The name of the group.
  */

/*!
 * \fn int GroupData::qbsLine() const
 * \brief The line at which the group is defined in the respective source file.
 */

 /*!
  * \fn QStringList GroupData::filePaths() const
  * \brief The files listed in the group item's "files" binding.
  * Note that these do not include expanded wildcards.
  * \sa GroupData::expandedWildcards
  */

/*!
 * \fn QStringList GroupData::expandedWildcards() const
 * \brief The list of files resulting from expanding all wildcard patterns in the group.
 */

/*!
 * \fn QVariantMap GroupData::properties() const
 * \brief The set of properties valid in this group.
 * Typically, most of them are inherited from the respective Product.
 */

 /*!
  * \fn QStringList GroupData::allFilePaths() const
  * \brief All files in this group, regardless of how whether they were given explicitly
  *        or via wildcards.
  * \sa GroupData::filePaths
  * \sa GroupData::expandedWildcards
  */

/*!
 * \fn bool GroupData::isEnabled() const
 * \brief Returns true if this Group is enabled in Qbs
 * This method returns the "condition" property of the Group definition. If the group is enabled
 * then the files in this Group will be processed, provided the Product it belongs to is also
 * enabled.
 *
 * Note that a Group can be enabled, even if the Product it belongs to is not. In this case
 * the files in the Group will not be processed.
 * \sa ProductData::isEnabled()
 */

/*!
 * \class ProductData
 * \brief The \c ProductData class corresponds to the Product item in a qbs source file.
 */

/*!
 * \fn QString ProductData::name() const
 * \brief The name of the product as given in the qbs source file.
 */

/*!
 * \fn QString ProductData::qbsFilePath() const
 * \brief The qbs source file in which the product is defined.
 */

/*!
 * \fn int ProductData::qbsLine() const
 * \brief The line in at which the product is defined in the source file.
 */

/*!
 * \fn QString ProductData::fileTags() const
 * \brief The file tags of this product. Corresponds to a Product's "type" property in
 *        a qbs source file.
 */

/*!
 * \fn QVariantMap ProductData::properties() const
 * \brief The set of properties valid in this product.
 * Note that product properties can be overwritten in a Group.
 * \sa GroupData::properties()
 */

/*!
 * \fn QList<GroupData> groups() const
 * \brief The list of \c GroupData in this product.
 */


/*!
 * \class ProjectData
 * \brief The \c ProjectData class corresponds to the Project item in a qbs source file.
 */

/*!
 * \fn CodeLocation ProjectData::location() const
 * \brief The qbs source file in which the project is defined.
 */

/*!
 * \fn QString ProjectData::buildDirectory() const
 * \brief The base directory under which the build artifacts of this project will be created.
 */

/*!
 * \fn QList<ProductData> ProjectData::products() const
 * \brief The products in this project.
 */

/*!
 * \fn bool ProductData::isEnabled() const
 * \brief Returns true if this Product is enabled in Qbs.
 * This method returns the "condition" property of the Product definition. If a product is
 * enabled, then it will be built in the current configuration.
 * \sa GroupData::isEnabled()
 */

} // namespace qbs
