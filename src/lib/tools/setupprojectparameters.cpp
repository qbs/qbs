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

#include <logging/translator.h>
#include <tools/profile.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>
#include <tools/settings.h>

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
        , environment(QProcessEnvironment::systemEnvironment())
    {
    }

    QString projectFilePath;
    QString buildRoot;
    QStringList searchPaths;
    QStringList pluginPaths;
    QVariantMap overriddenValues;
    QVariantMap buildConfiguration;
    mutable QVariantMap overriddenValuesTree;
    mutable QVariantMap buildConfigurationTree;
    bool ignoreDifferentProjectFilePath;
    bool dryRun;
    bool logElapsedTime;
    SetupProjectParameters::RestoreBehavior restoreBehavior;
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
}

static void provideValuesTree(const QVariantMap &values, QVariantMap *valueTree)
{
    if (!valueTree->isEmpty() || values.isEmpty())
        return;

    valueTree->clear();
    for (QVariantMap::const_iterator it = values.constBegin(); it != values.constEnd(); ++it) {
        QStringList nameElements = it.key().split(QLatin1Char('.'));
        if (nameElements.count() > 2) { // ### workaround for submodules being represented internally as a single module of name "module/submodule" rather than two nested modules "module" and "submodule"
            const QStringList allButLast = nameElements.mid(0, nameElements.length() - 1);
            QStringList newElements(allButLast.join(QLatin1String("/")));
            newElements.append(nameElements.last());
            nameElements = newElements;
        }
        Internal::setConfigProperty(*valueTree, nameElements, it.value());
    }
}

QVariantMap SetupProjectParameters::overriddenValuesTree() const
{
    provideValuesTree(d->overriddenValues, &d->overriddenValuesTree);
    return d->overriddenValuesTree;
}

/*!
 * \brief The collection of properties to use for resolving the project.
 */
QVariantMap SetupProjectParameters::buildConfiguration() const
{
    return d->buildConfiguration;
}

/*!
 * Sets the collection of properties to use for resolving the project.
 *
 * Keys are expected to be in dotted syntax (e.g. Qt.declarative.qmlDebugging) that is
 * used by "qbs config".
 */
void SetupProjectParameters::setBuildConfiguration(const QVariantMap &buildConfiguration)
{
    // warn if somebody tries to set a build configuration tree:
    for (QVariantMap::const_iterator i = buildConfiguration.constBegin();
         i != buildConfiguration.constEnd(); ++i) {
        QBS_ASSERT(i.value().type() != QVariant::Map, return);
    }
    d->buildConfiguration = buildConfiguration;
    d->buildConfigurationTree.clear();
}

/*!
 * \brief Returns the build configuration in tree form.
 * \return the tree form of the build configuration.
 */
QVariantMap SetupProjectParameters::buildConfigurationTree() const
{
    provideValuesTree(d->buildConfiguration, &d->buildConfigurationTree);
    return d->buildConfigurationTree;
}

/*!
 * \brief Expands the build configuration based on the given settings.
 *
 * Expansion is the process by which the build configuration is completed based on the given
 * settings. E.g. the information configured in a profile is filled into the build
 * configuration by this step.
 *
 * This method returns an Error. The list of entries in this error will be empty is the
 * expansion was successful.
 */
ErrorInfo SetupProjectParameters::expandBuildConfiguration(Settings *settings)
{
    ErrorInfo err;

    // Generates a full build configuration from user input, using the settings.
    QVariantMap expandedConfig = d->buildConfiguration;

    const QString buildVariant = expandedConfig.value(QLatin1String("qbs.buildVariant")).toString();
    if (buildVariant.isEmpty())
        throw ErrorInfo(Internal::Tr::tr("No build variant set."));
    if (buildVariant != QLatin1String("debug") && buildVariant != QLatin1String("release")) {
        err.append(Internal::Tr::tr("Invalid build variant '%1'. Must be 'debug' or 'release'.")
                   .arg(buildVariant));
        return err;
    }

    // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
    // 1) Things specified on command line (already in buildCfg at this point)
    // 2) Everything from the profile key
    QString profileName = expandedConfig.value("qbs.profile").toString();
    if (profileName.isNull()) {
        profileName = settings->defaultProfile();
        if (profileName.isNull()) {
            const QString profileNames = settings->profiles().join(QLatin1String(", "));
            err.append(Internal::Tr::tr("No profile given and no default profile set.\n"
                                        "Either set the configuration value 'defaultProfile' to a "
                                        "valid profile name\n"
                                        "or specify the profile with the command line parameter "
                                        "'profile:name'.\n"
                                        "The following profiles are available:\n%1")
                       .arg(profileNames));
            return err;
        }
        expandedConfig.insert("qbs.profile", profileName);
    }

    // (2)
    const Profile profile(profileName, settings);
    const QStringList profileKeys = profile.allKeys(Profile::KeySelectionRecursive);
    if (profileKeys.isEmpty()) {
        err.append(Internal::Tr::tr("Unknown or empty profile '%1'.").arg(profileName));
        return err;
    }
    foreach (const QString &profileKey, profileKeys) {
        if (!expandedConfig.contains(profileKey))
            expandedConfig.insert(profileKey, profile.value(profileKey));
    }

    if (d->buildConfiguration != expandedConfig) {
        d->buildConfigurationTree.clear();
        d->buildConfiguration = expandedConfig;
    }
    return err;
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

} // namespace qbs
