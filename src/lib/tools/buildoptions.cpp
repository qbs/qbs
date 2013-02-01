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
#include "buildoptions.h"

namespace qbs {

/*!
 * \class BuildOptions
 * \brief The \c BuildOptions class comprises parameters that influence the behavior of
 * build and clean operations.
 */

/*!
 * \brief Creates a \c BuildOptions object and initializes its members to sensible default values.
 */
BuildOptions::BuildOptions()
    : dryRun(false)
    , keepGoing(false)
    , maxJobCount(0)
{
}

/*!
 * \variable BuildOptions::changedFiles
 * \brief if non-empty, makes qbs pretend that only these files have changed
 */

 /*!
  * \variable BuildOptions::dryRun
  * \brief if true, qbs will not actually execute any commands, but just show what would happen
  * Note that the next call to build() on the same \c Project object will do nothing, since the
  * internal state needs to be updated the same way as if an actual build has happened. You'll
  * need to create a new \c Project object to do a real build.
  */

/*!
 * \variable BuildOptions::keepGoing
 * \brief if true, do not abort on errors if possible
 * E.g. a failed compile command will result in a warning message being printed, instead of
 * stopping the build process right away. However, there might still be fatal errors after which the
 * build process cannot continue.
 */

 /*!
  * \variable
  * \brief the maximum number of build commands to run concurrently
  * If the value is not valid (i.e. <= 0), a sensible one will be derived from the number of
  * available processor cores at build time.
  */

bool operator==(const BuildOptions &bo1, const BuildOptions &bo2)
{
    return bo1.changedFiles == bo2.changedFiles
            && bo1.dryRun == bo2.dryRun
            && bo1.keepGoing == bo2.keepGoing
            && bo1.maxJobCount == bo2.maxJobCount;
}

} // namespace qbs
