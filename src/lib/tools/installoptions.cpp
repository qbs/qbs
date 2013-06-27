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

#include <QSharedData>

namespace qbs {
namespace Internal {

class InstallOptionsPrivate : public QSharedData
{
public:
    InstallOptionsPrivate()
        : useSysroot(false), removeExisting(false), dryRun(false),
          keepGoing(false), logElapsedTime(false)
    {}

    QString installRoot;
    bool useSysroot;
    bool removeExisting;
    bool dryRun;
    bool keepGoing;
    bool logElapsedTime;
};

} // namespace Internal

/*!
 * \class InstallOptions
 * \brief The \c InstallOptions class comprises parameters that influence the behavior of
 * install operations.
 */

InstallOptions::InstallOptions() : d(new Internal::InstallOptionsPrivate)
{
}

InstallOptions::InstallOptions(const InstallOptions &other) : d(other.d)
{
}

InstallOptions &InstallOptions::operator=(const InstallOptions &other)
{
    d = other.d;
    return *this;
}

InstallOptions::~InstallOptions()
{
}

/*!
 * \brief The default install root, relative to the build directory.
 */
QString InstallOptions::defaultInstallRoot()
{
    return QLatin1String("install-root");
}

/*!
 * Returns the base directory for the installation.
 * The \c qbs.installPrefix path is relative to this root. If the string is empty, either the value of
 * qbs.sysroot or "<build dir>/install-root" will be used, depending on what \c installIntoSysroot()
 * returns.
 * The default is empty.
 */
QString InstallOptions::installRoot() const
{
    return d->installRoot;
}

/*!
 * \brief Sets the base directory for the installation.
 * \note The argument must either be an empty string or an absolute path to a directory
 *       (which might not yet exists, in which case it will be created).
 */
void InstallOptions::setInstallRoot(const QString &installRoot)
{
    d->installRoot = installRoot;
}

/*!
 * Returns whether to use the sysroot as the default install root.
 * The default is false.
 */
bool InstallOptions::installIntoSysroot() const
{
    return d->useSysroot;
}

void InstallOptions::setInstallIntoSysroot(bool useSysroot)
{
    d->useSysroot = useSysroot;
}

/*!
 * \brief Returns true iff an existing installation will be removed prior to installing.
 * The default is false.
 */
bool InstallOptions::removeExistingInstallation() const
{
    return d->removeExisting;
}

/*!
 * Controls whether to remove an existing installation before installing.
 * \note qbs may do some safety checks and refuse to remove certain directories such as
 *       a user's home directory. You should still be careful with this option, since it
 *       deletes recursively.
 */
void InstallOptions::setRemoveExistingInstallation(bool removeExisting)
{
    d->removeExisting = removeExisting;
}

/*!
 * \brief Returns true iff qbs will not actually copy any files, but just show what would happen.
 * The default is false.
 */
bool InstallOptions::dryRun() const
{
    return d->dryRun;
}

/*!
 * \brief Controls whether installation will actually take place.
 * If the argument is true, then qbs will emit information about which files would be copied
 * instead of actually doing it.
 */
void InstallOptions::setDryRun(bool dryRun)
{
    d->dryRun = dryRun;
}

/*!
 * Returns true iff installation will continue if an error occurs.
 * The default is false.
 */
bool InstallOptions::keepGoing() const
{
    return d->keepGoing;
}

/*!
 * \brief Controls whether to abort on errors.
 * If the argument is true, then if a file cannot be copied e.g. due to a permission problem,
 * a warning will be printed and the installation will continue. If the argument is false,
 * then the installation will abort immediately in case of an error.
 */
void InstallOptions::setKeepGoing(bool keepGoing)
{
    d->keepGoing = keepGoing;
}

/*!
 * \brief Returns true iff the time the operation takes will be logged.
 * The default is false.
 */
bool InstallOptions::logElapsedTime() const
{
    return d->logElapsedTime;
}

/*!
 * \brief Controls whether the installation time will be measured and logged.
 */
void InstallOptions::setLogElapsedTime(bool logElapsedTime)
{
    d->logElapsedTime = logElapsedTime;
}

} // namespace qbs
