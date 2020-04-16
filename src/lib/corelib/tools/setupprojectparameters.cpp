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
#include "setupprojectparameters.h"

#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/buildgraphlocker.h>
#include <tools/installoptions.h>
#include <tools/jsonhelper.h>
#include <tools/profile.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>
#include <tools/settings.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qjsonobject.h>

namespace qbs {
namespace Internal {

/*!
 * \class SetupProjectParameters
 * \brief The \c SetupProjectParameters class comprises data required to set up a qbs project.
 */

class SetupProjectParametersPrivate : public QSharedData
{
public:
    SetupProjectParametersPrivate()
        : overrideBuildGraphData(false)
        , dryRun(false)
        , logElapsedTime(false)
        , forceProbeExecution(false)
        , waitLockBuildGraph(false)
        , restoreBehavior(SetupProjectParameters::RestoreAndTrackChanges)
        , propertyCheckingMode(ErrorHandlingMode::Strict)
        , productErrorMode(ErrorHandlingMode::Strict)
    {
    }

    QString projectFilePath;
    QString topLevelProfile;
    QString configurationName = QLatin1String("default");
    QString buildRoot;
    QStringList searchPaths;
    QStringList pluginPaths;
    QString libexecPath;
    QString settingsBaseDir;
    QVariantMap overriddenValues;
    QVariantMap buildConfiguration;
    mutable QVariantMap buildConfigurationTree;
    mutable QVariantMap overriddenValuesTree;
    mutable QVariantMap finalBuildConfigTree;
    bool overrideBuildGraphData;
    bool dryRun;
    bool logElapsedTime;
    bool forceProbeExecution;
    bool waitLockBuildGraph;
    bool fallbackProviderEnabled = true;
    SetupProjectParameters::RestoreBehavior restoreBehavior;
    ErrorHandlingMode propertyCheckingMode;
    ErrorHandlingMode productErrorMode;
    QProcessEnvironment environment;
};

} // namespace Internal

SetupProjectParameters::SetupProjectParameters() : d(new Internal::SetupProjectParametersPrivate)
{
}

SetupProjectParameters::SetupProjectParameters(const SetupProjectParameters &other) = default;

SetupProjectParameters::SetupProjectParameters(
        SetupProjectParameters &&other) Q_DECL_NOEXCEPT = default;

SetupProjectParameters::~SetupProjectParameters() = default;

SetupProjectParameters &SetupProjectParameters::operator=(
        const SetupProjectParameters &other) = default;

namespace Internal {
template<> ErrorHandlingMode fromJson(const QJsonValue &v)
{
    if (v.toString() == QLatin1String("relaxed"))
        return ErrorHandlingMode::Relaxed;
    return ErrorHandlingMode::Strict;
}

template<> SetupProjectParameters::RestoreBehavior fromJson(const QJsonValue &v)
{
    const QString value = v.toString();
    if (value == QLatin1String("restore-only"))
        return SetupProjectParameters::RestoreOnly;
    if (value == QLatin1String("resolve-only"))
        return SetupProjectParameters::ResolveOnly;
    return SetupProjectParameters::RestoreAndTrackChanges;
}
} // namespace Internal

SetupProjectParameters SetupProjectParameters::fromJson(const QJsonObject &data)
{
    using namespace Internal;
    SetupProjectParameters params;
    setValueFromJson(params.d->topLevelProfile, data, "top-level-profile");
    setValueFromJson(params.d->configurationName, data, "configuration-name");
    setValueFromJson(params.d->projectFilePath, data, "project-file-path");
    setValueFromJson(params.d->buildRoot, data, "build-root");
    setValueFromJson(params.d->settingsBaseDir, data, "settings-directory");
    setValueFromJson(params.d->overriddenValues, data, "overridden-properties");
    setValueFromJson(params.d->dryRun, data, "dry-run");
    setValueFromJson(params.d->logElapsedTime, data, "log-time");
    setValueFromJson(params.d->forceProbeExecution, data, "force-probe-execution");
    setValueFromJson(params.d->waitLockBuildGraph, data, "wait-lock-build-graph");
    setValueFromJson(params.d->fallbackProviderEnabled, data, "fallback-provider-enabled");
    setValueFromJson(params.d->environment, data, "environment");
    setValueFromJson(params.d->restoreBehavior, data, "restore-behavior");
    setValueFromJson(params.d->propertyCheckingMode, data, "error-handling-mode");
    params.d->productErrorMode = params.d->propertyCheckingMode;
    return params;
}

SetupProjectParameters &SetupProjectParameters::operator=(SetupProjectParameters &&other) Q_DECL_NOEXCEPT = default;

/*!
 * \brief Returns the name of the top-level profile for building the project.
 */
QString SetupProjectParameters::topLevelProfile() const
{
    return d->topLevelProfile;
}

/*!
 * \brief Sets the top-level profile for building the project.
 */
void SetupProjectParameters::setTopLevelProfile(const QString &profile)
{
    d->buildConfigurationTree.clear();
    d->finalBuildConfigTree.clear();
    d->topLevelProfile = profile;
}

/*!
 * Returns the name of the current project build configuration.
 */
QString SetupProjectParameters::configurationName() const
{
    return d->configurationName;
}

/*!
 * Sets the name of the current project build configuration to an arbitrary user-specified name,
 * \a configurationName.
 */
void SetupProjectParameters::setConfigurationName(const QString &configurationName)
{
    d->buildConfigurationTree.clear();
    d->finalBuildConfigTree.clear();
    d->configurationName = configurationName;
}

/*!
 * \brief Returns the absolute path to the qbs project file.
 * This file typically has a ".qbs" suffix.
 */
QString SetupProjectParameters::projectFilePath() const
{
    return d->projectFilePath;
}

/*!
 * \brief Sets the path to the main project file.
 * \note The argument must be an absolute file path.
 */
void SetupProjectParameters::setProjectFilePath(const QString &projectFilePath)
{
    d->projectFilePath = projectFilePath;

    const QString canonicalProjectFilePath = QFileInfo(d->projectFilePath).canonicalFilePath();
    if (!canonicalProjectFilePath.isEmpty())
        d->projectFilePath = canonicalProjectFilePath;
}

/*!
 * \brief Returns the base path of where to put the build artifacts and store the build graph.
 */
QString SetupProjectParameters::buildRoot() const
{
    return d->buildRoot;
}

/*!
 * \brief Sets the base path of where to put the build artifacts and store the build graph.
 * The same base path can be used for several build profiles of the same project without them
 * interfering with each other.
 * It might look as if this parameter would not be needed at the time of setting up the project,
 * but keep in mind that the project information could already exist on disk, in which case
 * loading it will be much faster than setting up the project from scratch.
 * \note The argument must be an absolute path to a directory.
 */
void SetupProjectParameters::setBuildRoot(const QString &buildRoot)
{
    d->buildRoot = buildRoot;

    // Calling mkpath() may be necessary to get the canonical build root, but if we do it,
    // it must be reverted immediately afterwards as not to create directories needlessly,
    // e.g in the case of a dry run build.
    Internal::DirectoryManager dirManager(buildRoot, Internal::Logger());

    // We don't do error checking here, as this is not a convenient place to report an error.
    // If creation of the build directory is not possible, we will get sensible error messages
    // later, e.g. from the code that attempts to store the build graph.
    QDir::root().mkpath(buildRoot);

    const QString canonicalBuildRoot = QFileInfo(d->buildRoot).canonicalFilePath();
    if (!canonicalBuildRoot.isEmpty())
        d->buildRoot = canonicalBuildRoot;
}

/*!
 * \brief Where to look for modules and items to import.
 */
QStringList SetupProjectParameters::searchPaths() const
{
    return d->searchPaths;
}

/*!
 * \brief Sets the information about where to look for modules and items to import.
 * \note The elements of the list must be absolute paths to directories.
 */
void SetupProjectParameters::setSearchPaths(const QStringList &searchPaths)
{
    d->searchPaths = searchPaths;
}

/*!
 * \brief Where to look for plugins.
 */
QStringList SetupProjectParameters::pluginPaths() const
{
    return d->pluginPaths;
}

/*!
 * \brief Sets the information about where to look for plugins.
 * \note The elements of the list must be absolute paths to directories.
 */
void SetupProjectParameters::setPluginPaths(const QStringList &pluginPaths)
{
    d->pluginPaths = pluginPaths;
}

/*!
 * \brief Where to look for internal binaries.
 */
QString SetupProjectParameters::libexecPath() const
{
    return d->libexecPath;
}

/*!
 * \brief Sets the information about where to look for internal binaries.
 * \note \p libexecPath must be an absolute path.
 */
void SetupProjectParameters::setLibexecPath(const QString &libexecPath)
{
    d->libexecPath = libexecPath;
}

/*!
 * \brief The base directory for qbs settings.
 * This value is used to locate profiles and preferences.
 */
QString SetupProjectParameters::settingsDirectory() const
{
    return d->settingsBaseDir;
}

/*!
 * \brief Sets the base directory for qbs settings.
 * \param settingsBaseDir Will be used to locate profiles and preferences.
 */
void SetupProjectParameters::setSettingsDirectory(const QString &settingsBaseDir)
{
    d->settingsBaseDir = settingsBaseDir;
}

/*!
 * Returns the overridden values of the build configuration.
 */
QVariantMap SetupProjectParameters::overriddenValues() const
{
    return d->overriddenValues;
}

/*!
 * Set the overridden values of the build configuration.
 */
void SetupProjectParameters::setOverriddenValues(const QVariantMap &values)
{
    d->overriddenValues = values;
    d->overriddenValuesTree.clear();
    d->finalBuildConfigTree.clear();
}

static void provideValuesTree(const QVariantMap &values, QVariantMap *valueTree)
{
    if (!valueTree->empty() || values.empty())
        return;

    valueTree->clear();
    for (QVariantMap::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
        const QString &name = it.key();
        int idx = name.lastIndexOf(QLatin1Char('.'));
        const QStringList nameElements = (idx == -1)
                ? QStringList() << name
                : QStringList() << name.left(idx) << name.mid(idx + 1);
        Internal::setConfigProperty(*valueTree, nameElements, it.value());
    }
}

QVariantMap SetupProjectParameters::overriddenValuesTree() const
{
    provideValuesTree(d->overriddenValues, &d->overriddenValuesTree);
    return d->overriddenValuesTree;
}

/*!
 * \brief Returns the build configuration.
 * Overridden values are not taken into account.
 */
QVariantMap SetupProjectParameters::buildConfiguration() const
{
    return d->buildConfiguration;
}

/*!
 * \brief Returns the build configuration in tree form.
 * Overridden values are not taken into account.
 */
QVariantMap SetupProjectParameters::buildConfigurationTree() const
{
    provideValuesTree(d->buildConfiguration, &d->buildConfigurationTree);
    return d->buildConfigurationTree;
}

static QVariantMap expandedBuildConfigurationInternal(const Profile &profile,
                                                      const QString &configurationName)
{
    QVariantMap buildConfig;

    // (1) Values from profile, if given.
    if (profile.exists() && profile.name() != Profile::fallbackName()) {
        ErrorInfo err;
        const QStringList profileKeys = profile.allKeys(Profile::KeySelectionRecursive, &err);
        if (err.hasError())
            throw err;
        if (profileKeys.empty())
            throw ErrorInfo(Internal::Tr::tr("Unknown or empty profile '%1'.").arg(profile.name()));
        for (const QString &profileKey : profileKeys) {
            buildConfig.insert(profileKey, profile.value(profileKey, QVariant(), &err));
                if (err.hasError())
                    throw err;
        }
    }

    // (2) Build configuration name.
    if (configurationName.isEmpty())
        throw ErrorInfo(Internal::Tr::tr("No build configuration name set."));
    buildConfig.insert(QStringLiteral("qbs.configurationName"), configurationName);
    return buildConfig;
}


QVariantMap SetupProjectParameters::expandedBuildConfiguration(const Profile &profile,
        const QString &configurationName, ErrorInfo *errorInfo)
{
    try {
        return expandedBuildConfigurationInternal(profile, configurationName);
    } catch (const ErrorInfo &err) {
        if (errorInfo)
            *errorInfo = err;
        return {};
    }
}


/*!
 * \brief Expands the build configuration.
 *
 * Expansion is the process by which the build configuration is completed based on the settings
 * in \c settingsDirectory(). E.g. the information configured in a profile is filled into the build
 * configuration by this step.
 *
 * This method returns an Error. The list of entries in this error will be empty is the
 * expansion was successful.
 */
ErrorInfo SetupProjectParameters::expandBuildConfiguration()
{
    ErrorInfo err;
    Settings settings(d->settingsBaseDir);
    Profile profile(topLevelProfile(), &settings);
    QVariantMap expandedConfig = expandedBuildConfiguration(profile, configurationName(), &err);
    if (err.hasError())
        return err;
    if (d->buildConfiguration != expandedConfig) {
        d->buildConfigurationTree.clear();
        d->buildConfiguration = expandedConfig;
    }
    return err;
}

QVariantMap SetupProjectParameters::finalBuildConfigurationTree(const QVariantMap &buildConfig,
        const QVariantMap &overriddenValues)
{
    QVariantMap flatBuildConfig = buildConfig;
    for (QVariantMap::ConstIterator it = overriddenValues.constBegin();
         it != overriddenValues.constEnd(); ++it) {
        flatBuildConfig.insert(it.key(), it.value());
    }

    QVariantMap buildConfigTree;
    provideValuesTree(flatBuildConfig, &buildConfigTree);
    return buildConfigTree;
}

/*!
 * \brief Returns the build configuration in tree form, with overridden values taken into account.
 */
QVariantMap SetupProjectParameters::finalBuildConfigurationTree() const
{
    if (d->finalBuildConfigTree.empty()) {
        d->finalBuildConfigTree = finalBuildConfigurationTree(buildConfiguration(),
                                                              overriddenValues());
    }
    return d->finalBuildConfigTree;
}

 /*!
  * \brief if true, qbs will not store the build graph of the resolved project.
  */
bool SetupProjectParameters::dryRun() const
{
    return d->dryRun;
}

 /*!
  * \brief Controls whether the build graph will be stored.
  * If the argument is true, qbs will not store the build graph after resolving the project.
  * The default is false.
  */
void SetupProjectParameters::setDryRun(bool dryRun)
{
    d->dryRun = dryRun;
}

 /*!
  * \brief Returns true iff the time the operation takes should be logged
  */
bool SetupProjectParameters::logElapsedTime() const
{
    return d->logElapsedTime;
}

/*!
 * Controls whether to log the time taken up for resolving the project.
 * The default is false.
 */
void SetupProjectParameters::setLogElapsedTime(bool logElapsedTime)
{
    d->logElapsedTime = logElapsedTime;
}


/*!
 * \brief Returns true iff probes should be re-run.
 */
bool SetupProjectParameters::forceProbeExecution() const
{
    return d->forceProbeExecution;
}

/*!
 * Controls whether to re-run probes even if they do not appear to be outdated.
 * This option only has an effect if \c restoreBehavior() is \c RestoreAndTrackChanges.
 */
void SetupProjectParameters::setForceProbeExecution(bool force)
{
    d->forceProbeExecution = force;
}

/*!
 * \brief Returns true if qbs should wait for the build graph lock to become available,
 * otherwise qbs will exit immediately if the lock cannot be acquired.
 */
bool SetupProjectParameters::waitLockBuildGraph() const
{
    return d->waitLockBuildGraph;
}

/*!
 * Controls whether to wait indefinitely for the build graph lock to be released.
 * This allows multiple conflicting qbs processes to be spawned simultaneously.
 */
void SetupProjectParameters::setWaitLockBuildGraph(bool wait)
{
    d->waitLockBuildGraph = wait;
}

/*!
 * \brief Returns true if qbs should fall back to pkg-config if a dependency is not found.
 */
bool SetupProjectParameters::fallbackProviderEnabled() const
{
    return d->fallbackProviderEnabled;
}

/*!
 * Controls whether to fall back to pkg-config if a dependency is not found.
 */
void SetupProjectParameters::setFallbackProviderEnabled(bool enable)
{
    d->fallbackProviderEnabled = enable;
}

/*!
 * \brief Gets the environment used while resolving the project.
 */
QProcessEnvironment SetupProjectParameters::environment() const
{
    return d->environment;
}

/*!
 * \brief Sets the environment used while resolving the project.
 */
void SetupProjectParameters::setEnvironment(const QProcessEnvironment &env)
{
    d->environment = env;
}

QProcessEnvironment SetupProjectParameters::adjustedEnvironment() const
{
    QProcessEnvironment result = environment();
    const QVariantMap environmentFromProfile
            = buildConfigurationTree().value(QStringLiteral("buildEnvironment")).toMap();
    for (QVariantMap::const_iterator it = environmentFromProfile.begin();
         it != environmentFromProfile.end(); ++it) {
        result.insert(it.key(), it.value().toString());
    }
    return result;
}


/*!
 * \enum SetupProjectParamaters::RestoreBehavior
 * This enum type specifies how to deal with existing on-disk build information.
 * \value RestoreOnly Indicates that a stored build graph is to be loaded and the information
 *                    therein assumed to be up to date. It is then considered an error if no
 *                    such build graph exists.
 * \value ResolveOnly Indicates that no attempt should be made to restore an existing build graph.
 *                    Instead, the project is to be resolved from scratch.
 * \value RestoreAndTrackChanges Indicates that the build graph should be restored from disk
 *                               if possible and otherwise set up from scratch. In the first case,
 *                               (parts of) the project might still be re-resolved if certain
 *                               parameters have changed (e.g. environment variables used in the
 *                               project files).
 */


/*!
 * Returns information about how restored build data will be handled.
 */
SetupProjectParameters::RestoreBehavior SetupProjectParameters::restoreBehavior() const
{
    return d->restoreBehavior;
}

/*!
 * Controls how restored build data will be handled.
 */
void SetupProjectParameters::setRestoreBehavior(SetupProjectParameters::RestoreBehavior behavior)
{
    d->restoreBehavior = behavior;
}

/*!
 * Returns true if and only if environment, project file path and overridden property values
 * should be taken from this object even if a build graph already exists.
 * If this function returns \c false and a build graph exists, then it is an error to provide a
 * project file path or overridden property values that differ from the respective values
 * in the build graph.
 */
bool SetupProjectParameters::overrideBuildGraphData() const
{
    return d->overrideBuildGraphData;
}

/*!
 * If \c doOverride is true, then environment, project file path and overridden property values
 * are taken from this object rather than from the build graph.
 * The default is \c false.
 */
void SetupProjectParameters::setOverrideBuildGraphData(bool doOverride)
{
    d->overrideBuildGraphData = doOverride;
}

/*!
 * \enum ErrorHandlingMode
 * This enum type specifies how \QBS should behave if errors occur during project resolving.
 * \value ErrorHandlingMode::Strict Project resolving will stop with an error message.
 * \value ErrorHandlingMode::Relaxed Project resolving will continue (if possible), and a warning
 *        will be printed.
 */

/*!
 * Indicates how to handle unknown properties.
 */
ErrorHandlingMode SetupProjectParameters::propertyCheckingMode() const
{
    return d->propertyCheckingMode;
}

/*!
 * Controls how to handle unknown properties.
 * The default is \c PropertyCheckingRelaxed.
 */
void SetupProjectParameters::setPropertyCheckingMode(ErrorHandlingMode mode)
{
    d->propertyCheckingMode = mode;
}

/*!
 * \brief Indicates how errors occurring during product resolving are handled.
 */
ErrorHandlingMode SetupProjectParameters::productErrorMode() const
{
    return d->productErrorMode;
}

/*!
 * \brief Specifies whether an error occurring during product resolving should be fatal or not.
 * \note Not all errors can be ignored; this setting is mainly intended for things such as
 *       missing dependencies or references to non-existing source files.
 */
void SetupProjectParameters::setProductErrorMode(ErrorHandlingMode mode)
{
    d->productErrorMode = mode;
}

} // namespace qbs
