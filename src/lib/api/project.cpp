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
#include "project.h"

#include "internaljobs.h"
#include "jobs.h"
#include "projectdata.h"
#include "projectdata_p.h"
#include "propertymap_p.h"
#include "runenvironment.h"
#include <buildgraph/artifact.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/timestampsupdater.h>
#include <language/language.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/processresult.h>
#include <tools/profile.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedData>

namespace qbs {
namespace Internal {

static bool pluginsLoaded = false;
static QMutex pluginsLoadedMutex;

static void loadPlugins(const QStringList &_pluginPaths, const Logger &logger)
{
    QMutexLocker locker(&pluginsLoadedMutex);
    if (pluginsLoaded)
        return;

    QStringList pluginPaths;
    foreach (const QString &pluginPath, _pluginPaths) {
        if (!FileInfo::exists(pluginPath)) {
            logger.qbsWarning() << Tr::tr("Plugin path '%1' does not exist.")
                                    .arg(QDir::toNativeSeparators(pluginPath));
        } else {
            pluginPaths << pluginPath;
        }
    }
    ScannerPluginManager::instance()->loadPlugins(pluginPaths, logger);

    qRegisterMetaType<Error>("qbs::Error");
    qRegisterMetaType<ProcessResult>("qbs::ProcessResult");
    qRegisterMetaType<InternalJob *>("Internal::InternalJob *");
    pluginsLoaded = true;
}


class ProjectPrivate : public QSharedData
{
public:
    ProjectPrivate(const ResolvedProjectPtr &internalProject, const Logger &logger)
        : internalProject(internalProject), logger(logger), m_projectDataRetrieved(false)
    {
    }

    ProjectData projectData();
    BuildJob *buildProducts(const QList<ResolvedProductPtr> &products, const BuildOptions &options,
                            bool needsDepencencyResolving,
                            QObject *jobOwner);
    CleanJob *cleanProducts(const QList<ResolvedProductPtr> &products, const CleanOptions &options,
                            QObject *jobOwner);
    InstallJob *installProducts(const QList<ResolvedProductPtr> &products,
                                const InstallOptions &options, bool needsDepencencyResolving,
                                QObject *jobOwner);
    QList<ResolvedProductPtr> internalProducts(const QList<ProductData> &products) const;
    QList<ResolvedProductPtr> allEnabledInternalProducts() const;
    ResolvedProductPtr internalProduct(const ProductData &product) const;

    const ResolvedProjectPtr internalProject;
    Logger logger;

private:
    void retrieveProjectData();

    ProjectData m_projectData;
    bool m_projectDataRetrieved;
};

ProjectData ProjectPrivate::projectData()
{
    if (!m_projectDataRetrieved)
        retrieveProjectData();
    return m_projectData;
}

static void addDependencies(QList<ResolvedProductPtr> &products)
{
    for (int i = 0; i < products.count(); ++i) {
        const ResolvedProductPtr &product = products.at(i);
        foreach (const ResolvedProductPtr &dependency, product->dependencies) {
            if (!products.contains(dependency))
                products << dependency;
        }
    }
}

BuildJob *ProjectPrivate::buildProducts(const QList<ResolvedProductPtr> &products,
                                        const BuildOptions &options, bool needsDepencencyResolving,
                                        QObject *jobOwner)
{
    QList<ResolvedProductPtr> productsToBuild = products;
    if (needsDepencencyResolving)
        addDependencies(productsToBuild);

    BuildJob * const job = new BuildJob(logger, jobOwner);
    job->build(internalProject, productsToBuild, options);
    return job;
}

CleanJob *ProjectPrivate::cleanProducts(const QList<ResolvedProductPtr> &products,
        const CleanOptions &options, QObject *jobOwner)
{
    CleanJob * const job = new CleanJob(logger, jobOwner);
    job->clean(internalProject, products, options);
    return job;
}

InstallJob *ProjectPrivate::installProducts(const QList<ResolvedProductPtr> &products,
        const InstallOptions &options, bool needsDepencencyResolving, QObject *jobOwner)
{
    QList<ResolvedProductPtr> productsToInstall = products;
    if (needsDepencencyResolving)
        addDependencies(productsToInstall);
    InstallJob * const job = new InstallJob(logger, jobOwner);
    job->install(productsToInstall, options);
    return job;
}

QList<ResolvedProductPtr> ProjectPrivate::internalProducts(const QList<ProductData> &products) const
{
    QList<ResolvedProductPtr> internalProducts;
    foreach (const ProductData &product, products) {
        if (product.isEnabled())
            internalProducts << internalProduct(product);
    }
    return internalProducts;
}

QList<ResolvedProductPtr> ProjectPrivate::allEnabledInternalProducts() const
{
    QList<ResolvedProductPtr> products;
    foreach (const ResolvedProductPtr &p, internalProject->products) {
        if (p->enabled)
            products << p;
    }
    return products;
}

ResolvedProductPtr ProjectPrivate::internalProduct(const ProductData &product) const
{
    foreach (const ResolvedProductPtr &resolvedProduct, internalProject->products) {
        if (product.name() == resolvedProduct->name)
            return resolvedProduct;
    }
    qFatal("No build product '%s'", qPrintable(product.name()));
    return ResolvedProductPtr();
}

void ProjectPrivate::retrieveProjectData()
{
    m_projectData.d->location = internalProject->location;
    m_projectData.d->buildDir = internalProject->buildDirectory;
    foreach (const ResolvedProductConstPtr &resolvedProduct, internalProject->products) {
        ProductData product;
        product.d->name = resolvedProduct->name;
        product.d->location = resolvedProduct->location;
        product.d->fileTags = resolvedProduct->fileTags.toStringList();
        product.d->properties.d->m_map = resolvedProduct->properties;
        product.d->isEnabled = resolvedProduct->enabled;
        foreach (const GroupPtr &resolvedGroup, resolvedProduct->groups) {
            GroupData group;
            group.d->name = resolvedGroup->name;
            group.d->location = resolvedGroup->location;
            foreach (const SourceArtifactConstPtr &sa, resolvedGroup->files)
                group.d->filePaths << sa->absoluteFilePath;
            if (resolvedGroup->wildcards) {
                foreach (const SourceArtifactConstPtr &sa, resolvedGroup->wildcards->files)
                    group.d->expandedWildcards << sa->absoluteFilePath;
            }
            qSort(group.d->filePaths);
            qSort(group.d->expandedWildcards);
            group.d->properties.d->m_map = resolvedGroup->properties;
            group.d->isEnabled = resolvedGroup->enabled;
            product.d->groups << group;
        }
        qSort(product.d->groups);
        m_projectData.d->products << product;
    }
    qSort(m_projectData.d->products);
    m_projectDataRetrieved = true;
}

} // namespace Internal

using namespace Internal;

 /*!
  * \class Project
  * \brief The \c Project class provides services related to a qbs project.
  */

Project::Project(const ResolvedProjectPtr &internalProject, const Logger &logger)
    : d(new ProjectPrivate(internalProject, logger))
{
}

Project::Project(const Project &other) : d(other.d)
{
}

Project::~Project()
{
}

Project &Project::operator=(const Project &other)
{
    d = other.d;
    return *this;
}

// Generates a full build configuration from user input, using the settings.
static QVariantMap expandBuildConfiguration(const QVariantMap &buildConfig, Settings *settings)
{
    QVariantMap expandedConfig = buildConfig;

    const QString buildVariant = expandedConfig.value(QLatin1String("qbs.buildVariant")).toString();
    if (buildVariant.isEmpty())
        throw Error(Tr::tr("No build variant set."));
    if (buildVariant != QLatin1String("debug") && buildVariant != QLatin1String("release")) {
        throw Error(Tr::tr("Invalid build variant '%1'. Must be 'debug' or 'release'.")
                    .arg(buildVariant));
    }

    // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
    // 1) Things specified on command line (already in buildCfg at this point)
    // 2) Everything from the profile key
    QString profileName = expandedConfig.value("qbs.profile").toString();
    if (profileName.isNull()) {
        profileName = settings->defaultProfile();
        if (profileName.isNull()) {
            const QString profileNames = settings->profiles().join(QLatin1String(", "));
            throw Error(Tr::tr("No profile given and no default profile set.\n"
                    "Either set the configuration value 'defaultProfile' to a valid profile name\n"
                    "or specify the profile with the command line parameter 'profile:name'.\n"
                    "The following profiles are available:\n%1").arg(profileNames));
        }
        expandedConfig.insert("qbs.profile", profileName);
    }

    // (2)
    const Profile profile(profileName, settings);
    const QStringList profileKeys = profile.allKeys(Profile::KeySelectionRecursive);
    if (profileKeys.isEmpty())
        throw Error(Tr::tr("Unknown or empty profile '%1'.").arg(profileName));
    foreach (const QString &profileKey, profileKeys) {
        if (!expandedConfig.contains(profileKey))
            expandedConfig.insert(profileKey, profile.value(profileKey));
    }

    if (!expandedConfig.value("qbs.buildVariant").isValid())
        throw Error(Tr::tr("Property 'qbs.buildVariant' missing in build configuration."));

    foreach (const QString &property, expandedConfig.keys()) {
        QStringList nameElements = property.split('.');
        if (nameElements.count() > 2) { // ### workaround for submodules being represented internally as a single module of name "module/submodule" rather than two nested modules "module" and "submodule"
            QStringList allButLast = nameElements;
            allButLast.removeLast();
            QStringList newElements(allButLast.join("/"));
            newElements.append(nameElements.last());
            nameElements = newElements;
        }
        setConfigProperty(expandedConfig, nameElements, expandedConfig.value(property));
        expandedConfig.remove(property);
    }

    return expandedConfig;
}

/*!
 * \brief Sets up a \c Project from a source file, possibly re-using previously stored information.
 * The function will finish immediately, returning a \c SetupProjectJob which can be used to
 * track the results of the operation.
 * \note The qbs plugins will only be loaded once. As a result, the value of
 *       \c parameters.pluginPaths will only have an effect the first time this function is called.
 *       Similarly, the value of \c parameters.searchPaths will not have an effect if
 *       a stored build graph is available.
 */
SetupProjectJob *Project::setupProject(const SetupProjectParameters &_parameters,
                                       Settings *settings, ILogSink *logSink, QObject *jobOwner)
{
    Logger logger(logSink);
    SetupProjectJob * const job = new SetupProjectJob(logger, jobOwner);
    try {
        loadPlugins(_parameters.pluginPaths(), logger);
        SetupProjectParameters parameters = _parameters;
        parameters.setBuildConfiguration(expandBuildConfiguration(
                                             parameters.buildConfiguration(), settings));
        job->resolve(parameters);
    } catch (const Error &error) {
        // Throwing from here would complicate the API, so let's report the error the same way
        // as all others, via AbstractJob::error().
        job->reportError(error);
    }
    return job;
}


/*!
 * \brief Retrieves information for this project.
 * Call this function if you need insight into the project structure, e.g. because you want to know
 * which products or files are in it.
 */
ProjectData Project::projectData() const
{
    return d->projectData();
}

static bool isExecutable(const PropertyMapPtr &properties, const FileTags &tags)
{
    return tags.contains("application")
        || (properties->qbsPropertyValue(QLatin1String("targetOS"))
            == QLatin1String("osx") && tags.contains("applicationbundle"));
}

/*!
 * \brief Returns the file path of the executable associated with the given product.
 * If the product is not an application, an empty string is returned.
 * The \a installRoot parameter is used to look up the executable in case it is installable;
 * otherwise the parameter is ignored. To specify the default install root, leave it empty.
 */
QString Project::targetExecutable(const ProductData &product,
                                  const InstallOptions &installOptions) const
{
    if (!product.isEnabled())
        return QString();
    const ResolvedProductConstPtr internalProduct = d->internalProduct(product);
    if (!isExecutable(internalProduct->properties, internalProduct->fileTags))
        return QString();

    foreach (const Artifact * const artifact, internalProduct->buildData->targetArtifacts) {
        if (isExecutable(artifact->properties, artifact->fileTags)) {
            if (!artifact->properties->qbsPropertyValue(QLatin1String("install")).toBool())
                return artifact->filePath();
            const QString fileName = FileInfo::fileName(artifact->filePath());
            QString installRoot = installOptions.installRoot();
            if (installRoot.isEmpty()) {
                if (installOptions.installIntoSysroot()) {
                    // Yes, the executable is unlikely to run in this case. But we should still
                    // follow the protocol.
                    installRoot = artifact->properties
                            ->qbsPropertyValue(QLatin1String("sysroot")).toString();
                } else  {
                    installRoot = d->internalProject->buildDirectory
                            + QLatin1Char('/') + InstallOptions::defaultInstallRoot();
                }
            }
            QString installDir = artifact->properties
                    ->qbsPropertyValue(QLatin1String("installDir")).toString();
            return QDir::cleanPath(installRoot + QLatin1Char('/') + installDir + QLatin1Char('/')
                                   + fileName);
        }
    }
    return QString();
}

RunEnvironment Project::getRunEnvironment(const ProductData &product,
        const QProcessEnvironment &environment, Settings *settings) const
{
    QBS_CHECK(product.isEnabled());
    const ResolvedProductPtr resolvedProduct = d->internalProduct(product);
    return RunEnvironment(resolvedProduct, environment, settings, d->logger);
}

/*!
 * \brief Causes all products of this project to be built, if necessary.
 * The function will finish immediately, returning a \c BuildJob identifiying the operation.
 */
BuildJob *Project::buildAllProducts(const BuildOptions &options, QObject *jobOwner) const
{
    return d->buildProducts(d->allEnabledInternalProducts(), options, false, jobOwner);
}

/*!
 * \brief Causes the specified list of products to be built.
 * Use this function if you only want to build some products, not the whole project. If any of
 * the products in \a products depend on other products, those will also be built.
 * The function will finish immediately, returning a \c BuildJob identifiying the operation.
 */
BuildJob *Project::buildSomeProducts(const QList<ProductData> &products,
                                     const BuildOptions &options, QObject *jobOwner) const
{
    return d->buildProducts(d->internalProducts(products), options, true, jobOwner);
}

/*!
 * \brief Convenience function for \c buildSomeProducts().
 * \sa Project::buildSomeProducts().
 */
BuildJob *Project::buildOneProduct(const ProductData &product, const BuildOptions &options,
                                   QObject *jobOwner) const
{
    return buildSomeProducts(QList<ProductData>() << product, options, jobOwner);
}

/*!
 * \brief Removes the build artifacts of all products in the project.
 * The function will finish immediately, returning a \c CleanJob identifiying this operation.
 * \sa Project::cleanSomeProducts()
 */
CleanJob *Project::cleanAllProducts(const CleanOptions &options, QObject *jobOwner) const
{
    return d->cleanProducts(d->allEnabledInternalProducts(), options, jobOwner);
}

/*!
 * \brief Removes the build artifacts of the given products.
 * The function will finish immediately, returning a \c CleanJob identifiying this operation.
 */
CleanJob *Project::cleanSomeProducts(const QList<ProductData> &products,
        const CleanOptions &options, QObject *jobOwner) const
{
    return d->cleanProducts(d->internalProducts(products), options, jobOwner);
}

/*!
 * \brief Convenience function for \c cleanSomeProducts().
 * \sa Project::cleanSomeProducts().
 */
CleanJob *Project::cleanOneProduct(const ProductData &product, const CleanOptions &options,
                                   QObject *jobOwner) const
{
    return cleanSomeProducts(QList<ProductData>() << product, options, jobOwner);
}

/*!
 * \brief Installs the installable files of all products in the project.
 * The function will finish immediately, returning an \c InstallJob identifiying this operation.
 */
InstallJob *Project::installAllProducts(const InstallOptions &options, QObject *jobOwner) const
{
    return d->installProducts(d->allEnabledInternalProducts(), options, false, jobOwner);
}

/*!
 * \brief Installs the installable files of the given products.
 * The function will finish immediately, returning an \c InstallJob identifiying this operation.
 */
InstallJob *Project::installSomeProducts(const QList<ProductData> &products,
                                         const InstallOptions &options, QObject *jobOwner) const
{
    return d->installProducts(d->internalProducts(products), options, true, jobOwner);
}

/*!
 * \brief Convenience function for \c installSomeProducts().
 * \sa Project::installSomeProducts().
 */
InstallJob *Project::installOneProduct(const ProductData &product, const InstallOptions &options,
                                       QObject *jobOwner) const
{
    return installSomeProducts(QList<ProductData>() << product, options, jobOwner);
}

/*!
 * \brief Updates the timestamps of all build artifacts in the given products.
 * Afterwards, the build graph will have the same state as if a successful build had been done.
 */
void Project::updateTimestamps(const QList<ProductData> &products)
{
    TimestampsUpdater().updateTimestamps(d->internalProject, d->internalProducts(products),
                                         d->logger);
}

} // namespace qbs
