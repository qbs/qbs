/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "installoptions.h"

#include "jsonhelper.h"
#include "stringconstants.h"

#include <language/language.h>

#include <QtCore/qdir.h>
#include <QtCore/qshareddata.h>

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

QString effectiveInstallRoot(const InstallOptions &options, const TopLevelProject *project)
{
    const QString installRoot = options.installRoot();
    if (!installRoot.isEmpty())
        return installRoot;

    if (options.installIntoSysroot()) {
        return project->buildConfiguration().value(StringConstants::qbsModule()).toMap()
                .value(QStringLiteral("sysroot")).toString();
    }
    return project->buildConfiguration().value(StringConstants::qbsModule()).toMap()
            .value(StringConstants::installRootProperty()).toString();
}

} // namespace Internal

/*!
 * \class InstallOptions
 * \brief The \c InstallOptions class comprises parameters that influence the behavior of
 * install operations.
 */

InstallOptions::InstallOptions() : d(new Internal::InstallOptionsPrivate)
{
}

InstallOptions::InstallOptions(const InstallOptions &other) = default;

InstallOptions::InstallOptions(InstallOptions &&other) Q_DECL_NOEXCEPT = default;

InstallOptions &InstallOptions::operator=(const InstallOptions &other) = default;

InstallOptions &InstallOptions::operator=(InstallOptions &&other) Q_DECL_NOEXCEPT = default;

InstallOptions::~InstallOptions() = default;

/*!
 * \brief The default install root, relative to the build directory.
 */
QString InstallOptions::defaultInstallRoot()
{
    return QStringLiteral("install-root");
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
 *       (which might not yet exist, in which case it will be created).
 */
void InstallOptions::setInstallRoot(const QString &installRoot)
{
    d->installRoot = QDir::cleanPath(installRoot);
    if (!QDir(installRoot).isRoot()) {
        while (d->installRoot.endsWith(QLatin1Char('/')))
            d->installRoot.chop(1);
    }
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

qbs::InstallOptions qbs::InstallOptions::fromJson(const QJsonObject &data)
{
    using namespace Internal;
    InstallOptions opt;
    setValueFromJson(opt.d->installRoot, data, "install-root");
    setValueFromJson(opt.d->useSysroot, data, "use-sysroot");
    setValueFromJson(opt.d->removeExisting, data, "clean-install-root");
    setValueFromJson(opt.d->dryRun, data, "dry-run");
    setValueFromJson(opt.d->keepGoing, data, "keep-going");
    setValueFromJson(opt.d->logElapsedTime, data, "log-time");
    return opt;
}

} // namespace qbs
