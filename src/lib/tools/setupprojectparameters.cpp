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
#include "setupprojectparameters.h"

namespace qbs {

SetupProjectParameters::SetupProjectParameters()
    : ignoreDifferentProjectFilePath(false), dryRun(false), logElapsedTime(false)
{
}

/*!
 * \class SetupProjectParameters
 * \brief The \c SetupProjectParameters class comprises data required to set up a qbs project.
 */

/*!
 * \variable SetupProjectParameters::projectFilePath
 * \brief The absolute path to the qbs project file.
 * This file typically has a ".qbs" suffix.
 */

/*!
 * \variable SetupProjectParameters::buildRoot
 * \brief The base path of where to put the build artifacts.
 * The same base path can be used for several build profiles of the same project without them
 * interfering with each other.
 * It might look as if this parameter would not be needed at the time of setting up the project,
 * but keep in mind that the project information might already exist on disk, in which case
 * loading it will be much faster than setting up the project from scratch.
 */

/*!
 * \variable SetupProjectParameters::searchPaths
 * \brief Where to look for modules and items to import.
 */

/*!
 * \variable SetupProjectParameters::pluginPaths
 * \brief Where to look for plugins.
 */

/*!
 * \variable SetupProjectParameters::buildConfiguration
 * \brief The collection of properties to use for resolving the project.
 */

 /*!
  * \variable SetupProjectParameters::ignoreDifferentProjectFilePath
  * \brief true iff the saved build graph should be used even if its path to the project file
  *        is different from \c projectFilePath
  */

 /*!
  * \variable SetupProjectParameters::logElapsedTime
  * \brief true iff the time the operation takes should be logged
  */

} // namespace qbs
