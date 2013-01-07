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
#include "installoptions.h"

namespace qbs {

/*!
 * \class InstallOptions
 * \brief The \c InstallOptions class comprises parameters that influence the behavior of
 * install operations.
 */

 /*!
  * \variable InstallOptions::dryRun
  * \brief if true, qbs will not actually copy any files, but just show what would happen
  */

/*!
 * \variable InstallOptions::keepGoing
 * \brief if true, do not abort on errors
 * If a file cannot be copied e.g. due to a permission problem, a warning will be printed and
 * the installation will continue. If this flag is not set, then the installation will abort
 * immediately in case of an error.
 */

/*!
 * \variable InstallOptions::installRoot
 * \brief the base directory for the installation
 * All "qbs.installDir" paths are relative to this root. If the string is empty, the value of
 * qbs.sysroot will be used. If that is also empty, the base directory is
 * "<build dir>/install-root".
 */

 /*!
  * \variable InstallOptions::removeFirst
  * \brief if true, removes the installRoot before installing any files.
  * \note qbs may do some safety checks here and refuse to remove certain directories such as
  *       a user's home directory. You should still be careful with this option, since it
  *       deletes recursively.
  */

InstallOptions::InstallOptions() : removeFirst(false), dryRun(false), keepGoing(false)
{
}

/*!
 * \brief The default install root, relative to the build directory.
 */
QString InstallOptions::defaultInstallRoot()
{
    return QLatin1String("install-root");
}

} // namespace qbs
