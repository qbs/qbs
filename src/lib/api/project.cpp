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
#include "projectfileupdater.h"
#include "propertymap_p.h"
#include "runenvironment.h"
#include <buildgraph/artifact.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/productinstaller.h>
#include <buildgraph/projectbuilddata.h>
#include <buildgraph/rulesevaluationcontext.h>
#include <buildgraph/timestampsupdater.h>
#include <language/language.h>
#include <language/projectresolver.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/processresult.h>
#include <tools/propertyfinder.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>
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

    qRegisterMetaType<ErrorInfo>("qbs::ErrorInfo");
    qRegisterMetaType<ProcessResult>("qbs::ProcessResult");
    qRegisterMetaType<InternalJob *>("Internal::InternalJob *");
    pluginsLoaded = true;
}


class ProjectPrivate : public QSharedData
{
public:
    ProjectPrivate(const TopLevelProjectPtr &internalProject, const Logger &logger)
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
    ProductData findProductData(const QString &productName) const;
    GroupData findGroupData(const ProductData &product, const QString &groupName) const;

    GroupData createGroupDataFromGroup(const GroupPtr &resolvedGroup);

    void addGroup(const ProductData &product, const QString &groupName);
    void addFiles(const ProductData &product, const GroupData &group, const QStringList &filePaths);
    void updateInternalCodeLocations(const ResolvedProjectPtr &project,
                                     const CodeLocation &changeLocation, int lineOffset);
    void updateExternalCodeLocations(const ProjectData &project,
                                     const CodeLocation &changeLocation, int lineOffset);

    const TopLevelProjectPtr internalProject;
    Logger logger;

private:
    void retrieveProjectData(ProjectData &projectData,
                             const ResolvedProjectConstPtr &internalProject);

    ProjectData m_projectData;
    bool m_projectDataRetrieved;
};

ProjectData ProjectPrivate::projectData()
{
    if (!m_projectDataRetrieved) {
        retrieveProjectData(m_projectData, internalProject);
        m_projectData.d->buildDir = internalProject->buildDirectory;
        m_projectDataRetrieved = true;
    }
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
    job->install(internalProject, productsToInstall, options);
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

static QList<ResolvedProductPtr> enabledInternalProducts(const ResolvedProjectConstPtr &project)
{
    QList<ResolvedProductPtr> products;
    foreach (const ResolvedProductPtr &p, project->products) {
        if (p->enabled)
            products << p;
    }
    foreach (const ResolvedProjectConstPtr &subProject, project->subProjects)
        products << enabledInternalProducts(subProject);
    return products;
}

QList<ResolvedProductPtr> ProjectPrivate::allEnabledInternalProducts() const
{
    return enabledInternalProducts(internalProject);
}

static ResolvedProductPtr internalProductForProject(const ResolvedProjectConstPtr &project,
                                                    const ProductData &product)
{
    foreach (const ResolvedProductPtr &resolvedProduct, project->products) {
        if (product.name() == resolvedProduct->name)
            return resolvedProduct;
    }
    foreach (const ResolvedProjectConstPtr &subProject, project->subProjects) {
        const ResolvedProductPtr &p = internalProductForProject(subProject, product);
        if (p)
            return p;
    }
    return ResolvedProductPtr();
}

ResolvedProductPtr ProjectPrivate::internalProduct(const ProductData &product) const
{
    return internalProductForProject(internalProject, product);
}

ProductData ProjectPrivate::findProductData(const QString &productName) const
{
    foreach (const ProductData &p, m_projectData.allProducts()) {
        if (p.name() == productName)
            return p;
    }
    return ProductData();
}

GroupData ProjectPrivate::findGroupData(const ProductData &product, const QString &groupName) const
{
    foreach (const GroupData &g, product.groups()) {
        if (g.name() == groupName)
            return g;
    }
    return GroupData();
}

GroupData ProjectPrivate::createGroupDataFromGroup(const GroupPtr &resolvedGroup)
{
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
    group.d->isValid = true;
    return group;
}

void ProjectPrivate::addGroup(const ProductData &product, const QString &groupName)
{
    if (groupName.isEmpty())
        throw ErrorInfo(Tr::tr("Group has an empty name."));
    if (!product.isValid())
        throw ErrorInfo(Tr::tr("Product is invalid."));
    const ResolvedProductPtr resolvedProduct = internalProduct(product);
    if (!resolvedProduct)
        throw ErrorInfo(Tr::tr("Product '%1' does not exist.").arg(product.name()));

    // Guard against calls with outdated product data.
    const ProductData currentProduct= findProductData(product.name());
    QBS_CHECK(currentProduct.isValid());

    foreach (const GroupPtr &resolvedGroup, resolvedProduct->groups) {
        if (resolvedGroup->name == groupName) {
            throw ErrorInfo(Tr::tr("Group '%1' already exists in product '%2'.")
                            .arg(groupName, product.name()), resolvedGroup->location);
        }
    }

    ProjectFileGroupInserter groupInserter(currentProduct, groupName);
    groupInserter.apply();

    m_projectData.d.detach(); // The data we already gave out must stay as it is.

    updateInternalCodeLocations(internalProject, groupInserter.itemPosition(),
                                groupInserter.lineOffset());
    updateExternalCodeLocations(m_projectData, groupInserter.itemPosition(),
                                groupInserter.lineOffset());

    GroupPtr resolvedGroup = ResolvedGroup::create();
    resolvedGroup->location = groupInserter.itemPosition();
    resolvedGroup->enabled = true;
    resolvedGroup->name = groupName;
    resolvedGroup->properties = resolvedProduct->properties;
    resolvedGroup->overrideTags = false;
    resolvedProduct->groups << resolvedGroup;
    foreach (const ProductData &newProduct, m_projectData.allProducts()) {
        if (newProduct.name() == product.name()) {
            newProduct.d->groups << createGroupDataFromGroup(resolvedGroup);
            qSort(newProduct.d->groups);
            break;
        }
    }
}

void ProjectPrivate::addFiles(const ProductData &product, const GroupData &group,
                              const QStringList &filePaths)
{
    if (!product.isValid())
        throw ErrorInfo(Tr::tr("Product is invalid."));
    const ResolvedProductPtr resolvedProduct = internalProduct(product);
    if (!resolvedProduct)
        throw ErrorInfo(Tr::tr("Product '%1' does not exist.").arg(product.name()));

    const ProductData currentProduct = findProductData(product.name());
    QBS_CHECK(currentProduct.isValid());

    GroupPtr resolvedGroup;
    GroupData currentGroup;
    const QString groupName = group.isValid() ? group.name() : product.name();
    foreach (const GroupPtr &g, resolvedProduct->groups) {
        if (g->name == groupName) {
            resolvedGroup = g;
            break;
        }
    }
    if (!resolvedGroup)
        throw ErrorInfo(Tr::tr("Group '%1' does not exist.").arg(groupName));
    currentGroup = findGroupData(currentProduct, groupName);
    QBS_CHECK(currentGroup.isValid());

    if (filePaths.isEmpty())
        return; // "No-op". Could also be defined to be an error, I guess.

    if (!resolvedGroup->prefix.isEmpty())
        throw ErrorInfo(Tr::tr("Cannot add files to a group with a prefix."));
    QStringList absoluteFilePaths;
    QString projectFileDir = QFileInfo(product.location().fileName()).dir().absolutePath();
    foreach (const QString &filePath, filePaths) {
        const QString absPath = QDir::cleanPath(FileInfo::resolvePath(projectFileDir,
                                                                      filePath));
        if (absoluteFilePaths.contains(absPath))
            throw ErrorInfo(Tr::tr("File '%1' appears more than once.").arg(absPath));
        if (!FileInfo(absPath).exists())
            throw ErrorInfo(Tr::tr("File '%1' does not exist.").arg(absPath));
        absoluteFilePaths << absPath;
    }

    // We do not check for entries in other groups, because such doublettes might be legitimate
    // due to conditions.
    foreach (const QString &filePath, absoluteFilePaths) {
        foreach (const SourceArtifactConstPtr &sa, resolvedGroup->files) {
            if (sa->absoluteFilePath == filePath) {
                throw ErrorInfo(Tr::tr("File '%1' already exists in group '%2'.")
                                .arg(filePath, group.name()));
            }
        }
    }

    ProjectFileFilesAdder adder(currentProduct, group.isValid() ? currentGroup : GroupData(),
                                absoluteFilePaths);
    adder.apply();

    m_projectData.d.detach();
    updateInternalCodeLocations(internalProject, adder.itemPosition(), adder.lineOffset());
    updateExternalCodeLocations(m_projectData, adder.itemPosition(), adder.lineOffset());

    QList<SourceArtifactPtr> addedSourceArtifacts;
    foreach (const QString &file, absoluteFilePaths) {
        const SourceArtifactPtr artifact = SourceArtifact::create();
        artifact->absoluteFilePath = file;
        artifact->properties = resolvedGroup->properties;
        artifact->fileTags = resolvedGroup->fileTags;
        artifact->overrideFileTags = resolvedGroup->overrideTags;
        ProjectResolver::applyFileTaggers(artifact, resolvedProduct, logger);
        addedSourceArtifacts << artifact;
        resolvedGroup->files << artifact;
    }
    if (resolvedProduct->enabled) {
        ArtifactsPerFileTagMap artifactsPerFileTag;
        foreach (const SourceArtifactConstPtr &sa, addedSourceArtifacts) {
            Artifact * const artifact = createArtifact(resolvedProduct, sa, logger);
            foreach (const FileTag &ft, artifact->fileTags)
                artifactsPerFileTag[ft] += artifact;
        }
        RulesEvaluationContextPtr &evalContext
                = resolvedProduct->topLevelProject()->buildData->evaluationContext;
        evalContext.reset(new RulesEvaluationContext(logger));
        RulesApplicator(resolvedProduct, artifactsPerFileTag, logger).applyAllRules();
        addTargetArtifacts(resolvedProduct, artifactsPerFileTag, logger);
        evalContext.clear();
    }
    doSanityChecks(internalProject, logger);
    currentGroup.d->filePaths << absoluteFilePaths;
    qSort(currentGroup.d->filePaths);
}

static void updateLocationIfNecessary(CodeLocation &location, const CodeLocation &changeLocation,
                                      int lineOffset)
{
    if (location.fileName() == changeLocation.fileName()
            && location.line() >= changeLocation.line()) {
        location = CodeLocation(location.fileName(), location.line() + lineOffset,
                                location.column());
    }
}

void ProjectPrivate::updateInternalCodeLocations(const ResolvedProjectPtr &project,
        const CodeLocation &changeLocation, int lineOffset)
{
    updateLocationIfNecessary(project->location, changeLocation, lineOffset);
    foreach (const ResolvedProjectPtr &subProject, project->subProjects)
        updateInternalCodeLocations(subProject, changeLocation, lineOffset);
    foreach (const ResolvedProductPtr &product, project->products) {
        updateLocationIfNecessary(product->location, changeLocation, lineOffset);
        foreach (const GroupPtr &group, product->groups)
            updateLocationIfNecessary(group->location, changeLocation, lineOffset);
        foreach (const RulePtr &rule, product->rules) {
            updateLocationIfNecessary(rule->script->location, changeLocation, lineOffset);
            foreach (const RuleArtifactPtr &artifact, rule->artifacts) {
                for (int i = 0; i < artifact->bindings.count(); ++i) {
                    updateLocationIfNecessary(artifact->bindings[i].location, changeLocation,
                                              lineOffset);
                }
            }
        }
        foreach (const ResolvedTransformerConstPtr &transformer, product->transformers)
            updateLocationIfNecessary(transformer->transform->location, changeLocation, lineOffset);
        foreach (const ResolvedModuleConstPtr &module, product->modules) {
            updateLocationIfNecessary(module->setupBuildEnvironmentScript->location,
                                      changeLocation, lineOffset);
            updateLocationIfNecessary(module->setupRunEnvironmentScript->location,
                                      changeLocation, lineOffset);
        }
    }
}

void ProjectPrivate::updateExternalCodeLocations(const ProjectData &project,
        const CodeLocation &changeLocation, int lineOffset)
{
    updateLocationIfNecessary(project.d->location, changeLocation, lineOffset);
    foreach (const ProjectData &subProject, project.subProjects())
        updateExternalCodeLocations(subProject, changeLocation, lineOffset);
    foreach (const ProductData &product, project.products()) {
        updateLocationIfNecessary(product.d->location, changeLocation, lineOffset);
        foreach (const GroupData &group, product.groups())
            updateLocationIfNecessary(group.d->location, changeLocation, lineOffset);
    }
}

void ProjectPrivate::retrieveProjectData(ProjectData &projectData,
                                         const ResolvedProjectConstPtr &internalProject)
{
    projectData.d->name = internalProject->name;
    projectData.d->location = internalProject->location;
    projectData.d->enabled = internalProject->enabled;
    foreach (const ResolvedProductConstPtr &resolvedProduct, internalProject->products) {
        ProductData product;
        product.d->name = resolvedProduct->name;
        product.d->location = resolvedProduct->location;
        product.d->isEnabled = resolvedProduct->enabled;
        foreach (const GroupPtr &resolvedGroup, resolvedProduct->groups)
            product.d->groups << createGroupDataFromGroup(resolvedGroup);
        if (resolvedProduct->enabled) {
            QBS_CHECK(resolvedProduct->buildData);
            foreach (const Artifact * const a, resolvedProduct->buildData->targetArtifacts) {
                TargetArtifact ta;
                ta.d->filePath = a->filePath();
                ta.d->fileTags = a->fileTags.toStringList();
                ta.d->properties.d->m_map = a->properties;
                ta.d->isValid = true;
                product.d->targetArtifacts << ta;
            }
        }
        qSort(product.d->groups);
        qSort(product.d->targetArtifacts);
        product.d->isValid = true;
        projectData.d->products << product;
    }
    foreach (const ResolvedProjectConstPtr &internalSubProject, internalProject->subProjects) {
        ProjectData subProject;
        retrieveProjectData(subProject, internalSubProject);
        projectData.d->subProjects << subProject;
    }
    projectData.d->isValid = true;
    qSort(projectData.d->products);
    qSort(projectData.d->subProjects);
    m_projectDataRetrieved = true;
}

} // namespace Internal

using namespace Internal;

 /*!
  * \class Project
  * \brief The \c Project class provides services related to a qbs project.
  */

Project::Project(const TopLevelProjectPtr &internalProject, const Logger &logger)
    : d(new ProjectPrivate(internalProject, logger))
{
}

Project::Project(const Project &other) : d(other.d)
{
}

Project::~Project()
{
}

/*!
 * \brief Returns true if and only if this object was retrieved from a successful \c SetupProjectJob.
 * \sa SetupProjectJob
 */
bool Project::isValid() const
{
    return d && d->internalProject;
}

Project &Project::operator=(const Project &other)
{
    d = other.d;
    return *this;
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
SetupProjectJob *Project::setupProject(const SetupProjectParameters &parameters,
                                       ILogSink *logSink, QObject *jobOwner)
{
    Logger logger(logSink);
    SetupProjectJob * const job = new SetupProjectJob(logger, jobOwner);
    try {
        loadPlugins(parameters.pluginPaths(), logger);
        job->resolve(parameters);
    } catch (const ErrorInfo &error) {
        // Throwing from here would complicate the API, so let's report the error the same way
        // as all others, via AbstractJob::error().
        job->reportError(error);
    }
    return job;
}

Project::Project()
{
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
 * The \a installOptions parameter is used to look up the executable in case it is installable;
 * otherwise the parameter is ignored and the returned path will point to where the file is built.
 */
QString Project::targetExecutable(const ProductData &product,
                                  const InstallOptions &installOptions) const
{
    if (!product.isEnabled())
        return QString();
    foreach (const TargetArtifact &ta, product.targetArtifacts()) {
        if (ta.isExecutable()) {
            const QList<InstallableFile> &installables
                    = installableFilesForProduct(product, installOptions);
            foreach (const InstallableFile &file, installables) {
                if (file.sourceFilePath() == ta.filePath())
                    return file.targetFilePath();
            }
            return ta.filePath();
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
 * \brief All files in the product for which "qbs.install" is true.
 * This includes source files as well as generated files.
 */
QList<InstallableFile> Project::installableFilesForProduct(const ProductData &product,
                                                           const InstallOptions &options) const
{
    QList<InstallableFile> installableFiles;
    const ResolvedProductConstPtr internalProduct = d->internalProduct(product);
    if (!internalProduct)
        return installableFiles;
    InstallOptions mutableOptions = options;
    foreach (const GroupConstPtr &group, internalProduct->groups) {
        foreach (const SourceArtifactConstPtr &artifact, group->allFiles()) {
            InstallableFile f;
            const QString &targetFilePath = ProductInstaller::targetFilePath(internalProduct->topLevelProject(),
                    artifact->absoluteFilePath, artifact->properties, mutableOptions,
                    &f.d->targetDirectory);
            if (targetFilePath.isEmpty())
                continue;
            f.d->sourceFilePath = artifact->absoluteFilePath;
            f.d->fileTags = artifact->fileTags.toStringList();
            f.d->isValid = true;
            installableFiles << f;
        }
    }
    if (internalProduct->enabled) {
        QBS_CHECK(internalProduct->buildData);
        foreach (const Artifact * const artifact, internalProduct->buildData->artifacts) {
            if (artifact->artifactType == Artifact::SourceFile)
                continue;
            InstallableFile f;
            const QString &targetFilePath = ProductInstaller::targetFilePath(internalProduct->topLevelProject(),
                    artifact->filePath(), artifact->properties, mutableOptions,
                    &f.d->targetDirectory);
            if (targetFilePath.isEmpty())
                continue;
            f.d->sourceFilePath = artifact->filePath();
            f.d->fileTags = artifact->fileTags.toStringList();
            f.d->isValid = true;
            installableFiles << f;
        }
    }
    qSort(installableFiles);
    return installableFiles;
}

/*!
 * \brief All files in the project for which "qbs.install" is true.
 * This includes all sub-projects.
 * \sa Project::installableFilesForProduct()
 */
QList<InstallableFile> Project::installableFilesForProject(const ProjectData &project,
                                                           const InstallOptions &options) const
{
    QList<InstallableFile> installableFiles;
    foreach (const ProductData &p, project.allProducts())
        installableFiles << installableFilesForProduct(p, options);
    qSort(installableFiles);
    return installableFiles;
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

/*!
 * \brief Finds files generated from the given file in the given product.
 * The function returns files generated from the given file and the given product. To do so it will
 * traverse the graph of generated files and the files generated from those files.
 *
 * If an empty list of tags is given, then all directly and indirectly generated files will be
 * returned. If there are tags, then processing will stop once matching files were found.
 */
QStringList Project::generatedFiles(const ProductData &product, const QString &file,
                                    const QStringList &tags) const
{
    const ResolvedProductConstPtr internalProduct = d->internalProduct(product);
    return internalProduct->generatedFiles(file, FileTags::fromStringList(tags));
}

QVariantMap Project::projectConfiguration() const
{
    return d->internalProject->buildConfiguration();
}

QHash<QString, QString> Project::usedEnvironment() const
{
    return d->internalProject->usedEnvironment;
}

QSet<QString> Project::buildSystemFiles() const
{
    return d->internalProject->buildSystemFiles;
}

/*!
 * \brief Adds a new empty group to the given product.
 * Returns an \c ErrorInfo object for which \c hasError() is false in case of a success
 * and true otherwise. In the latter case, the object will have a sensible description.
 * After calling this function, it is recommended to re-fetch the project data, as other
 * items can be affected.
 * \sa qbs::Project::projectData()
 */
ErrorInfo Project::addGroup(const ProductData &product, const QString &groupName)
{
    try {
        d->addGroup(product, groupName);
        return ErrorInfo();
    } catch (ErrorInfo errorInfo) {
        errorInfo.prepend(Tr::tr("Failure adding group to product."));
        return errorInfo;
    }
}

/*!
 * \brief Adds the given files to the given product.
 * If \c group is a default-constructed object, the files will be added to the product's
 * "files" property, otherwise to the one of \c group.
 * The file paths can be absolute or relative to the location of \c product. The project file
 * will always contain relative paths.
 * After calling this function, it is recommended to re-fetch the project data, as other
 * items can be affected.
 * \sa qbs::Project::projectData()
 */
ErrorInfo Project::addFiles(const ProductData &product, const GroupData &group,
                            const QStringList &filePaths)
{
    try {
        d->addFiles(product, group, filePaths);
        return ErrorInfo();
    } catch (ErrorInfo errorInfo) {
        errorInfo.prepend(Tr::tr("Failure adding files to product."));
        return errorInfo;
    }
}

} // namespace qbs
