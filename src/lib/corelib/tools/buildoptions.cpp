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
#include "buildoptions.h"

#include "jsonhelper.h"

#include <QtCore/qjsonobject.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qthread.h>

namespace qbs {
namespace Internal {

class BuildOptionsPrivate : public QSharedData
{
public:
    BuildOptionsPrivate()
        : maxJobCount(0), dryRun(false), keepGoing(false), forceTimestampCheck(false),
          forceOutputCheck(false),
          logElapsedTime(false), echoMode(defaultCommandEchoMode()), install(true),
          removeExistingInstallation(false), onlyExecuteRules(false)
    {
    }

    QStringList changedFiles;
    QStringList filesToConsider;
    QStringList activeFileTags;
    JobLimits jobLimits;
    QString settingsDir;
    int maxJobCount;
    bool dryRun;
    bool keepGoing;
    bool forceTimestampCheck;
    bool forceOutputCheck;
    bool logElapsedTime;
    CommandEchoMode echoMode;
    bool install;
    bool removeExistingInstallation;
    bool onlyExecuteRules;
    bool jobLimitsFromProjectTakePrecedence = false;
};

} // namespace Internal

/*!
 * \class BuildOptions
 * \brief The \c BuildOptions class comprises parameters that influence the behavior of
 * build and clean operations.
 */

/*!
 * \brief Creates a \c BuildOptions object and initializes its members to sensible default values.
 */
BuildOptions::BuildOptions() : d(new Internal::BuildOptionsPrivate)
{
}

BuildOptions::BuildOptions(const BuildOptions &other) = default;

BuildOptions &BuildOptions::operator=(const BuildOptions &other) = default;

BuildOptions::~BuildOptions() = default;

/*!
 * \brief If non-empty, qbs pretends that only these files have changed.
 * By default, this list is empty.
 */
QStringList BuildOptions::changedFiles() const
{
    return d->changedFiles;
}

/*!
 * \brief If the given list is empty, qbs will pretend only the listed files are changed.
 * \note The list elements must be absolute file paths.
 */
void BuildOptions::setChangedFiles(const QStringList &changedFiles)
{
    d->changedFiles = changedFiles;
}

/*!
 * \brief The list of files to consider.
 * \sa setFilesToConsider.
 * By default, this list is empty.
 */
QStringList BuildOptions::filesToConsider() const
{
    return d->filesToConsider;
}

/*!
 * \brief If the given list is non-empty, qbs will run only commands whose rule has at least one
 *        of these files as an input.
 * \note The list elements must be absolute file paths.
 */
void BuildOptions::setFilesToConsider(const QStringList &files)
{
    d->filesToConsider = files;
}

/*!
 * \brief The list of active file tags.
 * \sa setActiveFileTags
 */
QStringList BuildOptions::activeFileTags() const
{
    return d->activeFileTags;
}

/*!
 * \brief Set the list of active file tags.
 * If this list is non-empty, then every transformer with non-matching output file tags is skipped.
 * E.g. call \c setFilesToConsider() with "foo.cpp" and \c setActiveFileTags() with "obj" to
 * run the compiler on foo.cpp without further processing like linking.
 * \sa activeFileTags
 */
void BuildOptions::setActiveFileTags(const QStringList &fileTags)
{
    d->activeFileTags = fileTags;
}

/*!
 * \brief Returns the default value for \c maxJobCount.
 * This value will be used when \c maxJobCount has not been set explicitly.
 */
int BuildOptions::defaultMaxJobCount()
{
    return QThread::idealThreadCount();
}

/*!
 * \brief Returns the maximum number of build commands to run concurrently.
 * If the value is not valid (i.e. <= 0), a sensible one will be derived at build time
 * from the number of available processor cores at build time.
 * The default is 0.
 * \sa BuildOptions::defaultMaxJobCount
 */
int BuildOptions::maxJobCount() const
{
    return d->maxJobCount;
}

/*!
 * \brief Controls how many build commands can be run in parallel.
 * A value <= 0 leaves the decision to qbs.
 */
void BuildOptions::setMaxJobCount(int jobCount)
{
    d->maxJobCount = jobCount;
}

/*!
 * \brief The base directory for qbs settings.
 * This value is used to locate profiles and preferences.
 */
QString BuildOptions::settingsDirectory() const
{
    return d->settingsDir;
}

/*!
 * \brief Sets the base directory for qbs settings.
 * \param settingsBaseDir Will be used to locate profiles and preferences.
 */
void BuildOptions::setSettingsDirectory(const QString &settingsBaseDir)
{
    d->settingsDir = settingsBaseDir;
}

JobLimits BuildOptions::jobLimits() const
{
    return d->jobLimits;
}

void BuildOptions::setJobLimits(const JobLimits &jobLimits)
{
    d->jobLimits = jobLimits;
}

bool BuildOptions::projectJobLimitsTakePrecedence() const
{
    return d->jobLimitsFromProjectTakePrecedence;
}

void BuildOptions::setProjectJobLimitsTakePrecedence(bool toggle)
{
    d->jobLimitsFromProjectTakePrecedence = toggle;
}

/*!
 * \brief Returns true iff qbs will not actually execute any commands, but just show what
 *        would happen.
 * The default is false.
 */
bool BuildOptions::dryRun() const
{
    return d->dryRun;
}

/*!
 * \brief Controls whether qbs will actually build something.
 * If the argument is true, qbs will just emit information about what it would do. Otherwise,
 * the build is actually done.
 * \note After you build with this setting enabled, the next call to \c build() on the same
 * \c Project object will do nothing, since the internal state needs to be updated the same way
 * as if an actual build had happened. You'll need to create a new \c Project object to do
 * a real build afterwards.
 */
void BuildOptions::setDryRun(bool dryRun)
{
    d->dryRun = dryRun;
}

/*!
 * \brief Returns true iff a build will continue after an error.
 * E.g. a failed compile command will result in a warning message being printed, instead of
 * stopping the build process right away. However, there might still be fatal errors after which the
 * build process cannot continue.
 * The default is \c false.
 */
bool BuildOptions::keepGoing() const
{
    return d->keepGoing;
}

/*!
 * \brief Controls whether a qbs will try to continue building after an error has occurred.
 */
void BuildOptions::setKeepGoing(bool keepGoing)
{
    d->keepGoing = keepGoing;
}

/*!
 * \brief Returns true if qbs is to use physical timestamps instead of the timestamps stored in the
 * build graph.
 * The default is \c false.
 */
bool BuildOptions::forceTimestampCheck() const
{
    return d->forceTimestampCheck;
}

/*!
 * \brief Controls whether qbs should use physical timestamps for up-to-date checks.
 */
void BuildOptions::setForceTimestampCheck(bool enabled)
{
    d->forceTimestampCheck = enabled;
}

/*!
 * \brief Returns true if qbs will test whether rules actually create their
 * declared output artifacts.
 * The default is \c false.
 */
bool BuildOptions::forceOutputCheck() const
{
    return d->forceOutputCheck;
}

/*!
 * \brief Controls whether qbs should test whether rules actually create their
 * declared output artifacts. Enabling this may introduce some small I/O overhead during the build.
 */
void BuildOptions::setForceOutputCheck(bool enabled)
{
    d->forceOutputCheck = enabled;
}

/*!
 * \brief Returns true iff the time the operation takes will be logged.
 * The default is \c false.
 */
bool BuildOptions::logElapsedTime() const
{
    return d->logElapsedTime;
}

/*!
 * \brief Controls whether the build time will be measured and logged.
 */
void BuildOptions::setLogElapsedTime(bool log)
{
    d->logElapsedTime = log;
}

/*!
 * \brief The kind of output that is displayed when executing commands.
 */
CommandEchoMode BuildOptions::echoMode() const
{
    return d->echoMode;
}

/*!
 * \brief Controls the kind of output that is displayed when executing commands.
 */
void BuildOptions::setEchoMode(CommandEchoMode echoMode)
{
    d->echoMode = echoMode;
}

/*!
 * \brief Returns true iff installation should happen as part of the build.
 * The default is \c true.
 */
bool BuildOptions::install() const
{
    return d->install;
}

/*!
 * \brief Controls whether to install artifacts as part of the build process.
 */
void BuildOptions::setInstall(bool install)
{
    d->install = install;
}

/*!
 * \brief Returns true iff an existing installation will be removed prior to building.
 * The default is false.
 */
bool BuildOptions::removeExistingInstallation() const
{
    return d->removeExistingInstallation;
}

/*!
 * Controls whether to remove an existing installation before installing.
 * \note qbs may do some safety checks and refuse to remove certain directories such as
 *       a user's home directory. You should still be careful with this option, since it
 *       deletes recursively.
 */
void BuildOptions::setRemoveExistingInstallation(bool removeExisting)
{
    d->removeExistingInstallation = removeExisting;
}

/*!
 * \brief Returns true iff instead of a full build, only the rules of the project will be run.
 * The default is false.
 */
bool BuildOptions::executeRulesOnly() const
{
    return d->onlyExecuteRules;
}

/*!
 * If \a onlyRules is \c true, then no artifacts are built, but only rules are being executed.
 * \note If the project contains highly dynamic rules that depend on output artifacts of child
 *       rules being already present, then the associated build job may fail even though
 *       the project is perfectly valid. Callers need to take this into consideration.
 */
void BuildOptions::setExecuteRulesOnly(bool onlyRules)
{
    d->onlyExecuteRules = onlyRules;
}


bool operator==(const BuildOptions &bo1, const BuildOptions &bo2)
{
    return bo1.changedFiles() == bo2.changedFiles()
            && bo1.dryRun() == bo2.dryRun()
            && bo1.keepGoing() == bo2.keepGoing()
            && bo1.logElapsedTime() == bo2.logElapsedTime()
            && bo1.echoMode() == bo2.echoMode()
            && bo1.maxJobCount() == bo2.maxJobCount()
            && bo1.install() == bo2.install()
            && bo1.removeExistingInstallation() == bo2.removeExistingInstallation();
}

namespace Internal {
template<> JobLimits fromJson(const QJsonValue &limitsData)
{
    JobLimits limits;
    const QJsonArray &limitsArray = limitsData.toArray();
    for (const QJsonValue &v : limitsArray) {
        const QJsonObject limitData = v.toObject();
        QString pool;
        int limit = 0;
        setValueFromJson(pool, limitData, "pool");
        setValueFromJson(limit, limitData, "limit");
        if (!pool.isEmpty() && limit > 0)
            limits.setJobLimit(pool, limit);
    }
    return limits;
}

template<> CommandEchoMode fromJson(const QJsonValue &modeData)
{
    const QString modeString = modeData.toString();
    if (modeString == QLatin1String("silent"))
        return CommandEchoModeSilent;
    if (modeString == QLatin1String("command-line"))
        return CommandEchoModeCommandLine;
    if (modeString == QLatin1String("command-line-with-environment"))
        return CommandEchoModeCommandLineWithEnvironment;
    return CommandEchoModeSummary;
}
} // namespace Internal

qbs::BuildOptions qbs::BuildOptions::fromJson(const QJsonObject &data)
{
    using namespace Internal;
    BuildOptions opt;
    setValueFromJson(opt.d->changedFiles, data, "changed-files");
    setValueFromJson(opt.d->filesToConsider, data, "files-to-consider");
    setValueFromJson(opt.d->activeFileTags, data, "active-file-tags");
    setValueFromJson(opt.d->jobLimits, data, "job-limits");
    setValueFromJson(opt.d->maxJobCount, data, "max-job-count");
    setValueFromJson(opt.d->dryRun, data, "dry-run");
    setValueFromJson(opt.d->keepGoing, data, "keep-going");
    setValueFromJson(opt.d->forceTimestampCheck, data, "check-timestamps");
    setValueFromJson(opt.d->forceOutputCheck, data, "check-outputs");
    setValueFromJson(opt.d->logElapsedTime, data, "log-time");
    setValueFromJson(opt.d->echoMode, data, "command-echo-mode");
    setValueFromJson(opt.d->install, data, "install");
    setValueFromJson(opt.d->removeExistingInstallation, data, "clean-install-root");
    setValueFromJson(opt.d->onlyExecuteRules, data, "only-execute-rules");
    setValueFromJson(opt.d->jobLimitsFromProjectTakePrecedence, data, "enforce-project-job-limits");
    return opt;
}

} // namespace qbs
