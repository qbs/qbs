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
#include "cleanoptions.h"

namespace qbs {

/*!
 * \class CleanOptions
 * \brief The \c CleanOptions class comprises parameters that influence the behavior of
 * cleaning operations.
 */

 /*!
  * \variable CleanOptions::dryRun
  * \brief if true, qbs will not actually remove any files, but just show what would happen.
  */

/*!
 * \variable CleanOptions::keepGoing
 * \brief if true, do not abort on errors
 * If a file cannot be removed, e.g. due to a permission problem, a warning will be printed and
 * cleaning will continue. If this flag is not set, then the operation will abort
 * immediately in case of an error.
 */

/*!
 * \enum CleanOptions::CleanType
 * This enum type specifies which kind of build artifacts to remove.
 * \value CleanupAll Indicates that all files created by the build process should be removed.
 * \value CleanupTemporaries Indicates that only intermediate build artifacts should be removed.
 *        If, for example, the product to clean up for is a Linux shared library, the .so file
 *        would be left on the disk, but the .o files would be removed.
 */

/*!
 * \variable CleanOptions::cleanType
 * \brief what to remove
 */

CleanOptions::CleanOptions() : cleanType(CleanupAll), dryRun(false), keepGoing(false)
{
}

} // namespace qbs
