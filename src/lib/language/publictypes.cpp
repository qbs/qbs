/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#include "publictypes.h"

namespace qbs {

// These are not inline because MSVC does not like it when source files have no content.
Group::Group() { }
Product::Product() { }
Project::Project() { }

/*!
 * \class Group
 * \brief The \c Group class corresponds to the respective item in a qbs source file.
 */

 /*!
  * \fn ObjectId Group::id() const
  * \brief Uniquely identifies a group.
  */

 /*!
  * \fn QString Group::name() const
  * \brief The name of the group.
  */

 /*!
  * \fn QStringList Group::filePaths() const
  * \brief The files listed in the group item's "files" binding.
  * Note that these do not include expanded wildcards.
  * \sa Group::expandedWildcards
  */

/*!
 * \fn QStringList Group::expandedWildcards() const
 * \brief The list of files resulting from expanding all wildcard patterns in the group.
 */

/*!
 * \fn QVariantMap Group::properties() const
 * \brief The set of properties valid in this group.
 * Typically, most of them are inherited from the respective \c Product.
 */

 /*!
  * \fn QStringList Group::allFilePaths() const
  * \brief All files in this group, regardless of how whether they were given explicitly
  *        or via wildcards.
  * \sa Group::filePaths
  * \sa Group::expandedWildcards
  */


/*!
 * \class Product
 * \brief The \c Product class corresponds to the respective item in a qbs source file.
 */

 /*!
  * \fn ObjectId Product::id() const
  * \brief Uniquely identifies a product.
  */

/*!
 * \fn QString Product::name() const
 * \brief The name of the product as given in the qbs source file.
 */

/*!
 * \fn QString Product::qbsFilePath() const
 * \brief The qbs source file in which the product is defined.
 */

/*!
 * \fn QString Product::fileTags() const
 * \brief The file tags of this product. Corresponds to a Product's "type" property in
 *        a qbs source file.
 */

/*!
 * \fn QVariantMap product::properties() const
 * \brief The set of properties valid in this product.
 * Note that a \c Group can override product properties.
 * \sa Group::properties()
 */

/*!
 * \fn QList<Group> groups() const
 * \brief The list of \c Groups in this product.
 */


/*!
 * \class Project
 * \brief The \c Project class corresponds to the respective item in a qbs source file.
 */

/*!
 * \fn ObjectId Project::id() const
 * \brief Uniquely identifies a project.
 */

/*!
 * \fn QString Project::qbsFilePath() const
 * \brief The qbs source file in which the project is defined.
 */

/*!
 * \fn QList<Product> Project::products() const
 * \brief The products in this project.
 */
} // namespace qbs
