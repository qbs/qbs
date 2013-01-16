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

namespace qbs {

// These are not inline because MSVC does not like it when source files have no content.
GroupData::GroupData() { }

bool operator==(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() == rhs.name()
            && lhs.location() == rhs.location()
            && lhs.expandedWildcards() == rhs.expandedWildcards()
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
 * \fn QString ProjectData::qbsFilePath() const
 * \brief The qbs source file in which the project is defined.
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
