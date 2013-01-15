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

#include "jobs.h"
#include "projectdata.h"
#include "runenvironment.h"
#include <buildgraph/artifact.h>
#include <buildgraph/buildproduct.h>
#include <buildgraph/buildproject.h>
#include <buildgraph/rulesevaluationcontext.h>
#include <buildgraph/timestampsupdater.h>
#include <language/language.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/platform.h>
#include <tools/preferences.h>
#include <tools/profile.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
#include <tools/settings.h>

#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedData>

namespace qbs {
namespace Internal {

static bool pluginsLoaded = false;
static QMutex pluginsLoadedMutex;

static void loadPlugins()
{
    QMutexLocker locker(&pluginsLoadedMutex);
    if (pluginsLoaded)
        return;

    QStringList pluginPaths;
    const QStringList settingsPluginPaths = Preferences().pluginPaths();
    foreach (const QString &pluginPath, settingsPluginPaths) {
        if (!FileInfo::exists(pluginPath)) {
            qbsWarning() << Tr::tr("Plugin path '%1' does not exist.")
                    .arg(QDir::toNativeSeparators(pluginPath));
        } else {
            pluginPaths << pluginPath;
        }
    }
    ScannerPluginManager::instance()->loadPlugins(pluginPaths);

    pluginsLoaded = true;
}


class ProjectPrivate : public QSharedData
{
public:
    ProjectPrivate(const BuildProjectPtr &internalProject)
        : internalProject(internalProject), m_projectDataRetrieved(false)
    {
    }

    ProjectData projectData();
    BuildJob *buildProducts(const QList<BuildProductPtr> &products, const BuildOptions &options,
                            bool needsDepencencyResolving, const QProcessEnvironment &env,
                            QObject *jobOwner);
    CleanJob *cleanProducts(const QList<BuildProductPtr> &products, const BuildOptions &options,
                            Project::CleanType cleanType, QObject *jobOwner);
    QList<BuildProductPtr> internalProducts(const QList<ProductData> &products) const;
    BuildProductPtr internalProduct(const ProductData &product) const;

    const BuildProjectPtr internalProject;

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

BuildJob *ProjectPrivate::buildProducts(const QList<BuildProductPtr> &products,
                                        const BuildOptions &options, bool needsDepencencyResolving,
                                        const QProcessEnvironment &env, QObject *jobOwner)
{
    QList<BuildProductPtr> productsToBuild = products;
    if (needsDepencencyResolving) {
        for (int i = 0; i < productsToBuild.count(); ++i) {
            const BuildProductConstPtr &product = productsToBuild.at(i);
            foreach (const BuildProductPtr &dependency, product->dependencies) {
                if (!productsToBuild.contains(dependency))
                    productsToBuild << dependency;
            }
        }
    }

    BuildJob * const job = new BuildJob(jobOwner);
    job->build(productsToBuild, options, env);
    return job;
}

CleanJob *ProjectPrivate::cleanProducts(const QList<BuildProductPtr> &products,
        const BuildOptions &options, Project::CleanType cleanType, QObject *jobOwner)
{
    CleanJob * const job = new CleanJob(jobOwner);
    job->clean(products, options, cleanType == Project::CleanupAll);
    return job;
}

QList<BuildProductPtr> ProjectPrivate::internalProducts(const QList<ProductData> &products) const
{
    QList<Internal::BuildProductPtr> internalProducts;
    foreach (const ProductData &product, products)
        internalProducts << internalProduct(product);
    return internalProducts;
}

BuildProductPtr ProjectPrivate::internalProduct(const ProductData &product) const
{
    foreach (const Internal::BuildProductPtr &buildProduct, internalProject->buildProducts()) {
        if (product.name() == buildProduct->rProduct->name)
            return buildProduct;
    }
    qFatal("No build product '%s'", qPrintable(product.name()));
    return BuildProductPtr();
}

void ProjectPrivate::retrieveProjectData()
{
    m_projectData.m_location = internalProject->resolvedProject()->location;
    foreach (const BuildProductPtr &buildProduct, internalProject->buildProducts()) {
        const ResolvedProductConstPtr resolvedProduct = buildProduct->rProduct;
        ProductData product;
        product.m_name = resolvedProduct->name;
        product.m_location = resolvedProduct->location;
        product.m_fileTags = resolvedProduct->fileTags;
        product.m_properties = resolvedProduct->properties->value();
        foreach (const GroupPtr &resolvedGroup, resolvedProduct->groups) {
            GroupData group;
            group.m_name = resolvedGroup->name;
            group.m_location = resolvedGroup->location;
            foreach (const SourceArtifactConstPtr &sa, resolvedGroup->files)
                group.m_filePaths << sa->absoluteFilePath;
            if (resolvedGroup->wildcards) {
                foreach (const SourceArtifactConstPtr &sa, resolvedGroup->wildcards->files)
                    group.m_expandedWildcards << sa->absoluteFilePath;
            }
            qSort(group.m_filePaths);
            qSort(group.m_expandedWildcards);
            group.m_properties = resolvedGroup->properties->value();
            product.m_groups << group;
        }
        qSort(product.m_groups);
        m_projectData.m_products << product;
    }
    qSort(m_projectData.m_products);
    m_projectDataRetrieved = true;
}

} // namespace Internal


/*!
 * \enum Project::CleanType
 * This enum type specifies which kind of build artifacts to remove.
 * \value CleanupAll Indicates that all files created by the build process should be removed.
 * \value CleanupTemporaries Indicates that only intermediate build artifacts should be removed.
 *        If, for example, the product to clean up for is a Linux shared library, the .so file
 *        would be left on the disk, but the .o files would be removed.
 */

 /*!
  * \class Project
  * \brief The \c Project class provides services related to a qbs project.
  */

Project::Project(const Internal::BuildProjectPtr &internalProject)
    : d(new Internal::ProjectPrivate(internalProject))
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

/*!
 * \brief Sets up a \c Project from a source file, possibly re-using previously stored information.
 * The \a projectFilePath parameter is the path to the project file, typically ending in ".qbs".
 * The \a buildConfig parameter is the set of properties used for resolving the project. Note that
 * calling this function with the same \a projectFilePath and different \a buildConfig parameters
 * will result in two distinct \c Projects, since the different properties will potentially cause
 * the bindings in the project file to evaluate to different values.
 * The \a buildRoot parameter is the base directory for building the project. It will be used
 * to derive the actual build directory and is required here because the project information might
 * already exist on disk, in which case it will be available faster. If you know that the project
 * has never been built and you do not plan to do so later, \a buildRoot can be an arbitrary string.
 * The function will finish immediately, returning a \c SetupProjectJob which can be used to
 * track the results of the operation.
 */
SetupProjectJob *Project::setupProject(const QString &projectFilePath,
        const QVariantMap &buildConfig, const QString &buildRoot, QObject *jobOwner)
{
    loadPlugins();
    SetupProjectJob * const job = new SetupProjectJob(jobOwner);
    job->resolve(projectFilePath, buildRoot, expandBuildConfiguration(buildConfig));
    return job;
}

/*!
 * \brief Project::expandBuildConfiguration generates a full build configuration from user input
 * \param buildConfig is the base build configuration that will be expanded with settings from the
 * configuration
 * \return The expanded build configuration
 */
QVariantMap Project::expandBuildConfiguration(const QVariantMap &buildConfig)
{
    QHash<QString, Platform::Ptr > platforms = Platform::platforms();
    if (platforms.isEmpty())
        throw Error(Tr::tr("No platforms configured. You must run 'qbs platforms probe' first."));

    Settings settings;
    QVariantMap expandedConfig = buildConfig;

    // Fill in buildCfg in this order (making sure not to overwrite a key already set by a previous stage)
    // 1) Things specified on command line (already in buildCfg at this point)
    // 2) Everything from the profile key
    // 3) Everything from the platform
    QString profileName = expandedConfig.value("qbs.profile").toString();
    if (profileName.isNull()) {
        profileName = settings.defaultProfile();
        if (profileName.isNull()) {
            throw Error(Tr::tr("No profile given and no default profile set.\n"
                    "Either set the configuration value 'profile' to a valid profile's name\n"
                    "or specify the profile with the command line parameter 'profile:name'."));
        }
        expandedConfig.insert("qbs.profile", profileName);
    }

    // (2)
    const Profile profile(profileName, &settings);
    const QStringList profileKeys = profile.allKeys();
    if (profileKeys.isEmpty())
        throw Error(Tr::tr("Unknown or empty profile '%1'.").arg(profileName));
    foreach (const QString &profileKey, profileKeys) {
        QString fixedKey(profileKey);
        fixedKey.replace(QChar('/'), QChar('.'));
        if (!expandedConfig.contains(fixedKey))
            expandedConfig.insert(fixedKey, profile.value(profileKey));
    }

    // (3) Need to make sure we have a value for qbs.platform before going any further
    QVariant platformName = expandedConfig.value("qbs.platform");
    if (!platformName.isValid()) {
        platformName = profile.value("qbs/platform");
        if (platformName.isValid())
            expandedConfig.insert("qbs.platform", platformName);
    }
    Platform::Ptr platform = platforms.value(platformName.toString());
    if (!platform.isNull()) {
        foreach (const QString &key, platform->settings.allKeys()) {
            if (key.startsWith(Platform::internalKey()))
                continue;
            QString fixedKey = key;
            int idx = fixedKey.lastIndexOf(QChar('/'));
            if (idx > 0)
                fixedKey[idx] = QChar('.');
            if (!expandedConfig.contains(fixedKey))
                expandedConfig.insert(fixedKey, platform->settings.value(key));
        }
    }

    if (!expandedConfig.value("qbs.buildVariant").isValid())
        throw Error(Tr::tr("Property 'qbs.buildVariant' missing in build configuration."));

    foreach (const QString &property, expandedConfig.keys()) {
        QStringList nameElements = property.split('.');
        if (nameElements.count() == 1 && nameElements.first() != "project") // ### Still need this because platform doesn't supply fully-qualified properties (yet), need to fix this
            nameElements.prepend("qbs");
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
 * \brief Retrieves information for this project.
 * Call this function if you need insight into the project structure, e.g. because you want to know
 * which products or files are in it.
 */
ProjectData Project::projectData() const
{
    return d->projectData();
}

/*!
 * \brief Returns the file path of the executable associated with the given product.
 * If the product is not an application, an empty string is returned.
 */
QString Project::targetExecutable(const ProductData &product) const
{
    const Internal::BuildProductConstPtr buildProduct = d->internalProduct(product);
    if (!buildProduct->rProduct->fileTags.contains(QLatin1String("application")))
        return QString();

    foreach (const Internal::Artifact * const artifact, buildProduct->targetArtifacts) {
        if (artifact->fileTags.contains(QLatin1String("application")))
            return artifact->filePath();
    }
    return QString();
}

RunEnvironment Project::getRunEnvironment(const ProductData &product,
                                          const QProcessEnvironment &environment) const
{
    const Internal::ResolvedProductPtr resolvedProduct = d->internalProduct(product)->rProduct;
    return RunEnvironment(resolvedProduct, environment);
}

/*!
 * \brief Causes all products of this project to be built, if necessary.
 * The function will finish immediately, returning a \c BuildJob identifiying the operation.
 */
BuildJob *Project::buildAllProducts(const BuildOptions &options, const QProcessEnvironment &env,
                                    QObject *jobOwner) const
{
    return d->buildProducts(d->internalProject->buildProducts().toList(), options, false, env,
                            jobOwner);
}

/*!
 * \brief Causes the specified list of products to be built.
 * Use this function if you only want to build some products, not the whole project. If any of
 * the products in \a products depend on other products, those will also be built.
 * The function will finish immediately, returning a \c BuildJob identifiying the operation.
 */
BuildJob *Project::buildSomeProducts(const QList<ProductData> &products,
                                     const BuildOptions &options,
                                     const QProcessEnvironment &env, QObject *jobOwner) const
{
    return d->buildProducts(d->internalProducts(products), options, true, env, jobOwner);
}

/*!
 * \brief Convenience function for \c buildSomeProducts().
 * \sa Project::buildSomeProducts().
 */
BuildJob *Project::buildOneProduct(const ProductData &product, const BuildOptions &options,
                                   const QProcessEnvironment &env, QObject *jobOwner) const
{
    return buildSomeProducts(QList<ProductData>() << product, options, env, jobOwner);
}

/*!
 * \brief Removes the build artifacts of all products in the project.
 * The function will finish immediately, returning a \c BuildJob identifiying this operation.
 */
CleanJob *Project::cleanAllProducts(const BuildOptions &options, CleanType cleanType,
                                    QObject *jobOwner) const
{
    return d->cleanProducts(d->internalProject->buildProducts().toList(), options, cleanType,
                            jobOwner);
}

/*!
 * \brief Removes the build artifacts of the given products.
 * The function will finish immediately, returning a \c BuildJob identifiying this operation.
 */
CleanJob *Project::cleanSomeProducts(const QList<ProductData> &products,
        const BuildOptions &options, CleanType cleanType, QObject *jobOwner) const
{
    return d->cleanProducts(d->internalProducts(products), options, cleanType, jobOwner);
}

/*!
 * \brief Convenience function for \c cleanSomeProducts().
 * \sa Project::cleanSomeProducts().
 */
CleanJob *Project::cleanOneProduct(const ProductData &product, const BuildOptions &options,
                                   CleanType cleanType, QObject *jobOwner) const
{
    return cleanSomeProducts(QList<ProductData>() << product, options, cleanType, jobOwner);
}

/*!
 * \brief Updates the timestamps of all build artifacts in the given products.
 * Afterwards, the build graph will have the same state as if a successful build had been done.
 */
void Project::updateTimestamps(const QList<ProductData> &products)
{
    TimestampsUpdater().updateTimestamps(d->internalProducts(products));
}

} // namespace qbs
