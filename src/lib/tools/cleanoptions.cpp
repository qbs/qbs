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

#include <QSharedData>

namespace qbs {
namespace Internal {

class CleanOptionsPrivate : public QSharedData
{
public:
    CleanOptionsPrivate()
        : cleanType(CleanOptions::CleanupAll), dryRun(false),
          keepGoing(false), logElapsedTime(false)
    { }

    CleanOptions::CleanType cleanType;
    bool dryRun;
    bool keepGoing;
    bool logElapsedTime;
};

}

/*!
 * \class CleanOptions
 * \brief The \c CleanOptions class comprises parameters that influence the behavior of
 * cleaning operations.
 */

/*!
 * \enum CleanOptions::CleanType
 * This enum type specifies which kind of build artifacts to remove.
 * \value CleanupAll Indicates that all files created by the build process should be removed.
 * \value CleanupTemporaries Indicates that only intermediate build artifacts should be removed.
 *        If, for example, the product to clean up for is a Linux shared library, the .so file
 *        would be left on the disk, but the .o files would be removed.
 */

CleanOptions::CleanOptions() : d(new Internal::CleanOptionsPrivate)
{
}

CleanOptions::CleanOptions(const CleanOptions &other) : d(other.d)
{
}

CleanOptions &CleanOptions::operator=(const CleanOptions &other)
{
    d = other.d;
    return *this;
}

CleanOptions::~CleanOptions()
{
}

/*!
 * \brief Returns information about which type of artifacts will be removed.
 */
CleanOptions::CleanType CleanOptions::cleanType() const
{
    return d->cleanType;
}

/*!
 * \brief Controls which kind of artifacts to remove.
 * \sa CleanOptions::CleanType
 */
void CleanOptions::setCleanType(CleanOptions::CleanType cleanType)
{
    d->cleanType = cleanType;
}

/*!
 * \brief Returns true iff qbs will not actually remove any files, but just show what would happen.
 * The default is false.
 */
bool CleanOptions::dryRun() const
{
    return d->dryRun;
}

/*!
 * \brief Controls whether clean-up will actually take place.
 * If the argument is true, then qbs will emit information about which files would be removed
 * instead of actually doing it.
 */
void CleanOptions::setDryRun(bool dryRun)
{
    d->dryRun = dryRun;
}

/*!
 * Returns true iff clean-up will continue if an error occurs.
 * The default is false.
 */
bool CleanOptions::keepGoing() const
{
    return d->keepGoing;
}

/*!
 * \brief Controls whether to abort on errors.
 * If the argument is true, then if a file cannot be removed e.g. due to a permission problem,
 * a warning will be printed and the clean-up will continue. If the argument is false,
 * then the clean-up will abort immediately in case of an error.
 */
void CleanOptions::setKeepGoing(bool keepGoing)
{
    d->keepGoing = keepGoing;
}

/*!
 * \brief Returns true iff the time the operation takes will be logged.
 * The default is false.
 */
bool CleanOptions::logElapsedTime() const
{
    return d->logElapsedTime;
}

/*!
 * \brief Controls whether the clean-up time will be measured and logged.
 */
void CleanOptions::setLogElapsedTime(bool log)
{
    d->logElapsedTime = log;
}

} // namespace qbs
