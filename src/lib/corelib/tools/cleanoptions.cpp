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
#include "cleanoptions.h"

#include "jsonhelper.h"

#include <QtCore/qshareddata.h>

namespace qbs {
namespace Internal {

class CleanOptionsPrivate : public QSharedData
{
public:
    CleanOptionsPrivate()
        : dryRun(false),
          keepGoing(false), logElapsedTime(false)
    { }

    bool dryRun;
    bool keepGoing;
    bool logElapsedTime;
};

} // namespace Internal

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

CleanOptions::CleanOptions(const CleanOptions &other) = default;

CleanOptions::CleanOptions(CleanOptions &&other) Q_DECL_NOEXCEPT = default;

CleanOptions &CleanOptions::operator=(const CleanOptions &other) = default;

CleanOptions &CleanOptions::operator=(CleanOptions &&other) Q_DECL_NOEXCEPT = default;

CleanOptions::~CleanOptions() = default;

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

qbs::CleanOptions qbs::CleanOptions::fromJson(const QJsonObject &data)
{
    CleanOptions opt;
    using namespace Internal;
    setValueFromJson(opt.d->dryRun, data, "dry-run");
    setValueFromJson(opt.d->keepGoing, data, "keep-going");
    setValueFromJson(opt.d->logElapsedTime, data, "log-time");
    return opt;
}

} // namespace qbs
