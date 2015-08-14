/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "setupprojectparameters.h"

#include <logging/translator.h>
#include <tools/installoptions.h>
#include <tools/profile.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>
#include <tools/settings.h>

#include <QFileInfo>

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
        : ignoreDifferentProjectFilePath(false)
        , dryRun(false)
        , logElapsedTime(false)
        , restoreBehavior(SetupProjectParameters::RestoreAndTrackChanges)
        , propertyCheckingMode(SetupProjectParameters::PropertyCheckingRelaxed)
        , environment(QProcessEnvironment::systemEnvironment())
    {
    }

    QString projectFilePath;
    QString topLevelProfile;
    QString buildVariant;
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
    bool ignoreDifferentProjectFilePath;
    bool dryRun;
    bool logElapsedTime;
    SetupProjectParameters::RestoreBehavior restoreBehavior;
    SetupProjectParameters::PropertyCheckingMode propertyCheckingMode;
    QProcessEnvironment environment;
};

} // namespace Internal

SetupProjectParameters::SetupProjectParameters() : d(new Internal::SetupProjectParametersPrivate)
{
}

SetupProjectParameters::SetupProjectParameters(const SetupProjectParameters &other) : d(other.d)
{
}

SetupProjectParameters::~SetupProjectParameters()
{
}

SetupProjectParameters &SetupProjectParameters::operator=(const SetupProjectParameters &other)
{
    d = other.d;
    return *this;
}

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
 * \brief Returns the build variant for building the project.
 */
QString SetupProjectParameters::buildVariant() const
{
    return d->buildVariant;
}

/*!
 * \brief Sets the build variant for building the project.
 * \param buildVariant "debug" or "release"
 */
void SetupProjectParameters::setBuildVariant(const QString &buildVariant)
{
    d->buildConfigurationTree.clear();
    d->finalBuildConfigTree.clear();
    d->buildVariant = buildVariant;
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
        d->projectFilePath = projectFilePath;
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
    // warn if somebody tries to set a build configuration tree:
    for (QVariantMap::const_iterator i = values.constBegin();
         i != values.constEnd(); ++i) {
        QBS_ASSERT(i.value().type() != QVariant::Map, return);
    }
    d->overriddenValues = values;
    d->overriddenValuesTree.clear();
    d->finalBuildConfigTree.clear();
}

static void provideValuesTree(const QVariantMap &values, QVariantMap *valueTree)
{
    if (!valueTree->isEmpty() || values.isEmpty())
        return;

    valueTree->clear();
    for (QVariantMap::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
        const QString name = it.key();
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

static QVariantMap expandedBuildConfigurationInternal(const QString &settingsBaseDir,
        const QString &profileName, const QString &buildVariant)
{
    Settings settings(settingsBaseDir);
    QVariantMap buildConfig;

    // (1) Values from profile, if given.
    if (!profileName.isEmpty()) {
        ErrorInfo err;
        const Profile profile(profileName, &settings);
        const QStringList profileKeys = profile.allKeys(Profile::KeySelectionRecursive, &err);
        if (err.hasError())
            throw err;
        if (profileKeys.isEmpty())
            throw ErrorInfo(Internal::Tr::tr("Unknown or empty profile '%1'.").arg(profileName));
        foreach (const QString &profileKey, profileKeys) {
            buildConfig.insert(profileKey, profile.value(profileKey, QVariant(), &err));
                if (err.hasError())
                    throw err;
        }
    }

    // (2) Build Variant.
    if (buildVariant.isEmpty())
        throw ErrorInfo(Internal::Tr::tr("No build variant set."));
    if (buildVariant != QLatin1String("debug") && buildVariant != QLatin1String("release")) {
        throw ErrorInfo(Internal::Tr::tr("Invalid build variant '%1'. Must be 'debug' or "
                                         "'release'.").arg(buildVariant));
    }
    buildConfig.insert(QLatin1String("qbs.buildVariant"), buildVariant);
    return buildConfig;
}


QVariantMap SetupProjectParameters::expandedBuildConfiguration(const QString &settingsBaseDir,
        const QString &profileName, const QString &buildVariant, ErrorInfo *errorInfo)
{
    try {
        return expandedBuildConfigurationInternal(settingsBaseDir, profileName, buildVariant);
    } catch (const ErrorInfo &err) {
        if (errorInfo)
            *errorInfo = err;
        return QVariantMap();
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
    QVariantMap expandedConfig = expandedBuildConfiguration(d->settingsBaseDir, topLevelProfile(),
                                                            buildVariant(), &err);
    if (err.hasError())
        return err;
    if (d->buildConfiguration != expandedConfig) {
        d->buildConfigurationTree.clear();
        d->buildConfiguration = expandedConfig;
    }
    return err;
}

QVariantMap SetupProjectParameters::finalBuildConfigurationTree(const QVariantMap &buildConfig,
        const QVariantMap &overriddenValues, const QString &buildRoot,
        const QString &topLevelProfile)
{
    QVariantMap flatBuildConfig = buildConfig;
    for (QVariantMap::ConstIterator it = overriddenValues.constBegin();
         it != overriddenValues.constEnd(); ++it) {
        flatBuildConfig.insert(it.key(), it.value());
    }

    const QString installRootKey = QLatin1String("qbs.installRoot");
    QString installRoot = flatBuildConfig.value(installRootKey).toString();
    if (installRoot.isEmpty()) {
        installRoot = buildRoot + QLatin1Char('/') + topLevelProfile + QLatin1Char('-')
                + flatBuildConfig.value(QLatin1String("qbs.buildVariant")).toString()
                + QLatin1Char('/') + InstallOptions::defaultInstallRoot();
        flatBuildConfig.insert(installRootKey, installRoot);
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
    if (d->finalBuildConfigTree.isEmpty()) {
        d->finalBuildConfigTree = finalBuildConfigurationTree(buildConfiguration(),
                overriddenValues(), buildRoot(), topLevelProfile());
    }
    return d->finalBuildConfigTree;
}

/*!
 * \variable SetupProjectParameters::ignoreDifferentProjectFilePath
 * \brief Returns true iff the saved build graph should be used even if its path to the
 *        project file is different from \c SetupProjectParameters::projectFilePath()
 */
bool SetupProjectParameters::ignoreDifferentProjectFilePath() const
{
    return d->ignoreDifferentProjectFilePath;
}

/*!
 * \brief Controls whether the path to the main project file may be different from the one
 *        stored in a possible build graph file.
 * The default is false.
 */
void SetupProjectParameters::setIgnoreDifferentProjectFilePath(bool doIgnore)
{
    d->ignoreDifferentProjectFilePath = doIgnore;
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
            = buildConfigurationTree().value(QLatin1String("buildEnvironment")).toMap();
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
 * \enum SetupProjectParamaters::PropertyCheckingMode
 * This enum type specifies how \QBS should behave if it encounters unknown properties.
 * \value PropertyCheckingStrict Project resolving will stop with an error message.
 * \value PropertyCheckingRelaxed Project resolving will continue, and a warning will be printed.
 */

/*!
 * Indicates how to handle unknown properties.
 */
SetupProjectParameters::PropertyCheckingMode SetupProjectParameters::propertyCheckingMode() const
{
    return d->propertyCheckingMode;
}

/*!
 * Controls how to handle unknown properties.
 * The default is \c PropertyCheckingRelaxed.
 */
void SetupProjectParameters::setPropertyCheckingMode(SetupProjectParameters::PropertyCheckingMode mode)
{
    d->propertyCheckingMode = mode;
}

} // namespace qbs
