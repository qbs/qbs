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
namespace Internal {

/*!
 * \class SetupProjectParameters
 * \brief The \c SetupProjectParameters class comprises data required to set up a qbs project.
 */

class SetupProjectParametersPrivate : public QSharedData
{
public:
    SetupProjectParametersPrivate()
        : ignoreDifferentProjectFilePath(false), dryRun(false), logElapsedTime(false),
          environment(QProcessEnvironment::systemEnvironment())
    {
    }

    QString projectFilePath;
    QString buildRoot;
    QStringList searchPaths;
    QStringList pluginPaths;
    QVariantMap buildConfiguration;
    bool ignoreDifferentProjectFilePath;
    bool dryRun;
    bool logElapsedTime;
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
 * \brief The collection of properties to use for resolving the project.
 */
QVariantMap SetupProjectParameters::buildConfiguration() const
{
    return d->buildConfiguration;
}

/*!
 * Sets the collection of properties to use for resolving the project.
 */
void SetupProjectParameters::setBuildConfiguration(const QVariantMap &buildConfiguration)
{
    d->buildConfiguration = buildConfiguration;
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

} // namespace qbs
