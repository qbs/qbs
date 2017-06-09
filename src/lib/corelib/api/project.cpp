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
#include "project.h"
#include "project_p.h"

#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
#include "projectfileupdater.h"
#endif

#include "internaljobs.h"
#include "jobs.h"
#include "projectdata_p.h"
#include "propertymap_p.h"
#include "rulecommand_p.h"
#include "runenvironment.h"
#include <buildgraph/artifact.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/buildgraphloader.h>
#include <buildgraph/emptydirectoriesremover.h>
#include <buildgraph/nodetreedumper.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/productinstaller.h>
#include <buildgraph/projectbuilddata.h>
#include <buildgraph/rulecommands.h>
#include <buildgraph/rulesevaluationcontext.h>
#include <buildgraph/timestampsupdater.h>
#include <buildgraph/transformer.h>
#include <language/language.h>
#include <language/projectresolver.h>
#include <language/propertymapinternal.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/cleanoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/processresult.h>
#include <tools/qbspluginmanager.h>
#include <tools/scripttools.h>
#include <tools/setupprojectparameters.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qregexp.h>
#include <QtCore/qshareddata.h>

#include <mutex>
#include <utility>
#include <vector>

namespace qbs {
namespace Internal {

static bool pluginsLoaded = false;
static std::mutex pluginsLoadedMutex;

static void loadPlugins(const QStringList &_pluginPaths, const Logger &logger)
{
    std::lock_guard<std::mutex> locker(pluginsLoadedMutex);
    if (pluginsLoaded)
        return;

    QStringList pluginPaths;
    for (const QString &pluginPath : _pluginPaths) {
        if (!FileInfo::exists(pluginPath)) {
#ifndef QBS_STATIC_LIB
            logger.qbsWarning() << Tr::tr("Plugin path '%1' does not exist.")
                                    .arg(QDir::toNativeSeparators(pluginPath));
#endif
        } else {
            pluginPaths << pluginPath;
        }
    }
    auto pluginManager = QbsPluginManager::instance();
    pluginManager->loadStaticPlugins();
    pluginManager->loadPlugins(pluginPaths, logger);

    qRegisterMetaType<ErrorInfo>("qbs::ErrorInfo");
    qRegisterMetaType<ProcessResult>("qbs::ProcessResult");
    qRegisterMetaType<InternalJob *>("Internal::InternalJob *");
    qRegisterMetaType<AbstractJob *>("qbs::AbstractJob *");
    pluginsLoaded = true;
}

ProjectData ProjectPrivate::projectData()
{
    m_projectData = ProjectData();
    retrieveProjectData(m_projectData, internalProject);
    m_projectData.d->buildDir = internalProject->buildDirectory;
    return m_projectData;
}

static void addDependencies(QList<ResolvedProductPtr> &products)
{
    for (int i = 0; i < products.count(); ++i) {
        const ResolvedProductPtr &product = products.at(i);
        for (const ResolvedProductPtr &dependency : qAsConst(product->dependencies)) {
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
    QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
    return job;
}

CleanJob *ProjectPrivate::cleanProducts(const QList<ResolvedProductPtr> &products,
        const CleanOptions &options, QObject *jobOwner)
{
    CleanJob * const job = new CleanJob(logger, jobOwner);
    job->clean(internalProject, products, options);
    QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
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
    QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
    return job;
}

QList<ResolvedProductPtr> ProjectPrivate::internalProducts(const QList<ProductData> &products) const
{
    QList<ResolvedProductPtr> internalProducts;
    for (const ProductData &product : products) {
        if (product.isEnabled())
            internalProducts << internalProduct(product);
    }
    return internalProducts;
}

static QList<ResolvedProductPtr> enabledInternalProducts(const ResolvedProjectConstPtr &project,
                                                         bool includingNonDefault)
{
    QList<ResolvedProductPtr> products;
    for (const ResolvedProductPtr &p : qAsConst(project->products)) {
        if (p->enabled && (includingNonDefault || p->builtByDefault()))
            products << p;
    }
    for (const ResolvedProjectConstPtr &subProject : qAsConst(project->subProjects))
        products << enabledInternalProducts(subProject, includingNonDefault);
    return products;
}

QList<ResolvedProductPtr> ProjectPrivate::allEnabledInternalProducts(bool includingNonDefault) const
{
    return enabledInternalProducts(internalProject, includingNonDefault);
}

static bool matches(const ProductData &product, const ResolvedProductConstPtr &rproduct)
{
    return product.name() == rproduct->name
            && product.profile() == rproduct->profile
            && product.multiplexConfigurationId() == rproduct->multiplexConfigurationId;
}

static ResolvedProductPtr internalProductForProject(const ResolvedProjectConstPtr &project,
                                                    const ProductData &product)
{
    for (const ResolvedProductPtr &resolvedProduct : qAsConst(project->products)) {
        if (matches(product, resolvedProduct))
            return resolvedProduct;
    }
    for (const ResolvedProjectConstPtr &subProject : qAsConst(project->subProjects)) {
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

ProductData ProjectPrivate::findProductData(const ProductData &product) const
{
    for (const ProductData &p : m_projectData.allProducts()) {
        if (p.name() == product.name()
                && p.profile() == product.profile()
                && p.multiplexConfigurationId() == product.multiplexConfigurationId()) {
            return p;
        }
    }
    return ProductData();
}

QList<ProductData> ProjectPrivate::findProductsByName(const QString &name) const
{
    QList<ProductData> list;
    for (const ProductData &p : m_projectData.allProducts()) {
        if (p.name() == name)
            list << p;
    }
    return list;
}

GroupData ProjectPrivate::findGroupData(const ProductData &product, const QString &groupName) const
{
    for (const GroupData &g : product.groups()) {
        if (g.name() == groupName)
            return g;
    }
    return GroupData();
}

GroupData ProjectPrivate::createGroupDataFromGroup(const GroupPtr &resolvedGroup,
                                                   const ResolvedProductConstPtr &product)
{
    GroupData group;
    group.d->name = resolvedGroup->name;
    group.d->prefix = resolvedGroup->prefix;
    group.d->location = resolvedGroup->location;
    for (const SourceArtifactConstPtr &sa : qAsConst(resolvedGroup->files)) {
        ArtifactData artifact = createApiSourceArtifact(sa);
        setupInstallData(artifact, product);
        group.d->sourceArtifacts << artifact;
    }
    if (resolvedGroup->wildcards) {
        for (const SourceArtifactConstPtr &sa : qAsConst(resolvedGroup->wildcards->files)) {
            ArtifactData artifact = createApiSourceArtifact(sa);
            setupInstallData(artifact, product);
            group.d->sourceArtifactsFromWildcards << artifact;
        }
    }
    qSort(group.d->sourceArtifacts);
    qSort(group.d->sourceArtifactsFromWildcards);
    group.d->properties.d->m_map = resolvedGroup->properties;
    group.d->isEnabled = resolvedGroup->enabled;
    group.d->isValid = true;
    return group;
}

ArtifactData ProjectPrivate::createApiSourceArtifact(const SourceArtifactConstPtr &sa)
{
    ArtifactData saApi;
    saApi.d->isValid = true;
    saApi.d->filePath = sa->absoluteFilePath;
    saApi.d->fileTags = sa->fileTags.toStringList();
    saApi.d->isGenerated = false;
    saApi.d->isTargetArtifact = false;
    saApi.d->properties.d->m_map = sa->properties;
    return saApi;
}

void ProjectPrivate::setupInstallData(ArtifactData &artifact,
                                      const ResolvedProductConstPtr &product)
{
    artifact.d->installData.d->isValid = true;
    artifact.d->installData.d->isInstallable = artifact.properties().getModuleProperty(
                QLatin1String("qbs"), QLatin1String("install")).toBool();
    if (!artifact.d->installData.d->isInstallable)
        return;
    const QString installRoot = artifact.properties().getModuleProperty(
                QLatin1String("qbs"), QLatin1String("installRoot")).toString();
    InstallOptions options;
    options.setInstallRoot(installRoot);
    artifact.d->installData.d->installRoot = installRoot;
    try {
        QString installFilePath = ProductInstaller::targetFilePath(product->topLevelProject(),
                product->sourceDirectory, artifact.filePath(), artifact.properties().d->m_map,
                options);
        if (!installRoot.isEmpty())
            installFilePath.remove(0, installRoot.count());
        artifact.d->installData.d->installFilePath = installFilePath;
    } catch (const ErrorInfo &e) {
        logger.printWarning(e);
    }
}

#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
void ProjectPrivate::addGroup(const ProductData &product, const QString &groupName)
{
    if (groupName.isEmpty())
        throw ErrorInfo(Tr::tr("Group has an empty name."));
    if (!product.isValid())
        throw ErrorInfo(Tr::tr("Product is invalid."));
    QList<ProductData> products = findProductsByName(product.name());
    if (products.isEmpty())
        throw ErrorInfo(Tr::tr("Product '%1' does not exist.").arg(product.name()));
    const QList<ResolvedProductPtr> resolvedProducts = internalProducts(products);
    QBS_CHECK(products.count() == resolvedProducts.count());

    for (const GroupPtr &resolvedGroup : qAsConst(resolvedProducts.first()->groups)) {
        if (resolvedGroup->name == groupName) {
            throw ErrorInfo(Tr::tr("Group '%1' already exists in product '%2'.")
                            .arg(groupName, product.name()), resolvedGroup->location);
        }
    }

    ProjectFileGroupInserter groupInserter(products.first(), groupName);
    groupInserter.apply();

    m_projectData.d.detach(); // The data we already gave out must stay as it is.

    updateInternalCodeLocations(internalProject, groupInserter.itemPosition(),
                                groupInserter.lineOffset());
    updateExternalCodeLocations(m_projectData, groupInserter.itemPosition(),
                                groupInserter.lineOffset());

    products = findProductsByName(products.first().name()); // These are new objects.
    QBS_CHECK(products.count() == resolvedProducts.count());
    for (int i = 0; i < products.count(); ++i) {
        const GroupPtr resolvedGroup = ResolvedGroup::create();
        resolvedGroup->location = groupInserter.itemPosition();
        resolvedGroup->enabled = true;
        resolvedGroup->name = groupName;
        resolvedGroup->properties = resolvedProducts[i]->moduleProperties;
        resolvedGroup->overrideTags = false;
        resolvedProducts.at(i)->groups << resolvedGroup;
        products.at(i).d->groups << createGroupDataFromGroup(resolvedGroup, resolvedProducts.at(i));
        qSort(products.at(i).d->groups);
    }
}

ProjectPrivate::GroupUpdateContext ProjectPrivate::getGroupContext(const ProductData &product,
                                                                   const GroupData &group)
{
    GroupUpdateContext context;
    if (!product.isValid())
        throw ErrorInfo(Tr::tr("Product is invalid."));
    context.products = findProductsByName(product.name());
    if (context.products.isEmpty())
        throw ErrorInfo(Tr::tr("Product '%1' does not exist.").arg(product.name()));
    context.resolvedProducts = internalProducts(context.products);

    const QString groupName = group.isValid() ? group.name() : product.name();
    for (const ResolvedProductPtr &p : qAsConst(context.resolvedProducts)) {
        for (const GroupPtr &g : qAsConst(p->groups)) {
            if (g->name == groupName) {
                context.resolvedGroups << g;
                break;
            }
        }
    }
    if (context.resolvedGroups.isEmpty())
        throw ErrorInfo(Tr::tr("Group '%1' does not exist.").arg(groupName));
    for (const ProductData &p : qAsConst(context.products)) {
        const GroupData &g = findGroupData(p, groupName);
        QBS_CHECK(p.isValid());
        context.groups << g;
    }
    QBS_CHECK(context.resolvedProducts.count() == context.products.count());
    QBS_CHECK(context.resolvedProducts.count() == context.resolvedGroups.count());
    QBS_CHECK(context.products.count() == context.groups.count());
    return context;
}

static bool matchesWildcard(const QString &filePath, const GroupConstPtr &group)
{
    if (!group->wildcards)
        return false;
    for (const QString &pattern : qAsConst(group->wildcards->patterns)) {
        QString fullPattern;
        if (QFileInfo(group->prefix).isAbsolute()) {
            fullPattern = group->prefix;
        } else {
            fullPattern = QFileInfo(group->location.filePath()).absolutePath()
                    + QLatin1Char('/') + group->prefix;
        }
        fullPattern.append(QLatin1Char('/')).append(pattern);
        fullPattern = QDir::cleanPath(fullPattern);
        if (QRegExp(fullPattern, Qt::CaseSensitive, QRegExp::Wildcard).exactMatch(filePath))
            return true;
    }
    return false;
}

ProjectPrivate::FileListUpdateContext ProjectPrivate::getFileListContext(const ProductData &product,
        const GroupData &group, const QStringList &filePaths, bool forAdding)
{
    FileListUpdateContext filesContext;
    GroupUpdateContext &groupContext = filesContext.groupContext;
    groupContext = getGroupContext(product, group);

    if (filePaths.isEmpty())
        throw ErrorInfo(Tr::tr("No files supplied."));

    QString prefix;
    for (int i = 0; i < groupContext.resolvedGroups.count(); ++i) {
        const GroupPtr &g = groupContext.resolvedGroups.at(i);
        if (!g->prefix.isEmpty() && !g->prefix.endsWith(QLatin1Char('/')))
            throw ErrorInfo(Tr::tr("Group has non-directory prefix."));
        if (i == 0)
            prefix = g->prefix;
        else if (prefix != g->prefix)
            throw ErrorInfo(Tr::tr("Cannot update: Group prefix depends on properties."));
    }
    QString baseDirPath = QFileInfo(product.location().filePath()).dir().absolutePath()
            + QLatin1Char('/') + prefix;
    QDir baseDir(baseDirPath);
    for (const QString &filePath : filePaths) {
        const QString absPath = QDir::cleanPath(FileInfo::resolvePath(baseDirPath, filePath));
        if (filesContext.absoluteFilePaths.contains(absPath))
            throw ErrorInfo(Tr::tr("File '%1' appears more than once.").arg(absPath));
        if (forAdding && !FileInfo(absPath).exists())
            throw ErrorInfo(Tr::tr("File '%1' does not exist.").arg(absPath));
        if (matchesWildcard(absPath, groupContext.resolvedGroups.first())) {
            filesContext.absoluteFilePathsFromWildcards << absPath;
        } else {
            filesContext.absoluteFilePaths << absPath;
            filesContext.relativeFilePaths << baseDir.relativeFilePath(absPath);
        }
    }

    return filesContext;
}

static SourceArtifactPtr createSourceArtifact(const QString &filePath,
        const ResolvedProductPtr &product, const GroupPtr &group, bool wildcard, Logger &logger)
{
    const SourceArtifactPtr artifact
            = ProjectResolver::createSourceArtifact(product, filePath, group, wildcard);
    ProjectResolver::applyFileTaggers(artifact, product, logger);
    return artifact;
}

void ProjectPrivate::addFiles(const ProductData &product, const GroupData &group,
                              const QStringList &filePaths)
{
    FileListUpdateContext filesContext = getFileListContext(product, group, filePaths, true);
    GroupUpdateContext &groupContext = filesContext.groupContext;

    // We do not check for entries in other groups, because such doublettes might be legitimate
    // due to conditions.
    for (const GroupPtr &group : qAsConst(groupContext.resolvedGroups)) {
        for (const QString &filePath : qAsConst(filesContext.absoluteFilePaths)) {
            for (const SourceArtifactConstPtr &sa : qAsConst(group->files)) {
                if (sa->absoluteFilePath == filePath) {
                    throw ErrorInfo(Tr::tr("File '%1' already exists in group '%2'.")
                                    .arg(filePath, group->name));
                }
            }
        }
    }

    ProjectFileFilesAdder adder(groupContext.products.first(),
            group.isValid() ? groupContext.groups.first() : GroupData(),
            filesContext.relativeFilePaths);
    adder.apply();

    m_projectData.d.detach();
    updateInternalCodeLocations(internalProject, adder.itemPosition(), adder.lineOffset());
    updateExternalCodeLocations(m_projectData, adder.itemPosition(), adder.lineOffset());

    QHash<QString, std::pair<SourceArtifactPtr, ResolvedProductPtr>> addedSourceArtifacts;
    for (int i = 0; i < groupContext.resolvedGroups.count(); ++i) {
        const ResolvedProductPtr &resolvedProduct = groupContext.resolvedProducts.at(i);
        const GroupPtr &resolvedGroup = groupContext.resolvedGroups.at(i);
        for (const QString &file : qAsConst(filesContext.absoluteFilePaths)) {
            const SourceArtifactPtr sa = createSourceArtifact(file, resolvedProduct, resolvedGroup,
                                                              false, logger);
            addedSourceArtifacts.insert(file, std::make_pair(sa, resolvedProduct));
        }
        for (const QString &file : qAsConst(filesContext.absoluteFilePathsFromWildcards)) {
            QBS_CHECK(resolvedGroup->wildcards);
            const SourceArtifactPtr sa = createSourceArtifact(file, resolvedProduct, resolvedGroup,
                    true, logger);
            addedSourceArtifacts.insert(file, std::make_pair(sa, resolvedProduct));
        }
        if (resolvedProduct->enabled) {
            for (const auto &pair : qAsConst(addedSourceArtifacts))
                createArtifact(resolvedProduct, pair.first, logger);
        }
    }
    doSanityChecks(internalProject, logger);
    QList<ArtifactData> sourceArtifacts;
    QList<ArtifactData> sourceArtifactsFromWildcards;
    for (const QString &fp : qAsConst(filesContext.absoluteFilePaths)) {
        const auto pair = addedSourceArtifacts.value(fp);
        const SourceArtifactConstPtr sa = pair.first;
        QBS_CHECK(sa);
        ArtifactData artifactData = createApiSourceArtifact(sa);
        setupInstallData(artifactData, pair.second);
        sourceArtifacts << artifactData;
    }
    for (const QString &fp : qAsConst(filesContext.absoluteFilePathsFromWildcards)) {
        const auto pair = addedSourceArtifacts.value(fp);
        const SourceArtifactConstPtr sa = pair.first;
        QBS_CHECK(sa);
        ArtifactData artifactData = createApiSourceArtifact(sa);
        setupInstallData(artifactData, pair.second);
        sourceArtifactsFromWildcards << artifactData;
    }
    for (const GroupData &g : qAsConst(groupContext.groups)) {
        g.d->sourceArtifacts << sourceArtifacts;
        qSort(g.d->sourceArtifacts);
        g.d->sourceArtifactsFromWildcards << sourceArtifactsFromWildcards;
        qSort(g.d->sourceArtifactsFromWildcards);
    }
}

void ProjectPrivate::removeFiles(const ProductData &product, const GroupData &group,
                                 const QStringList &filePaths)
{
    FileListUpdateContext filesContext = getFileListContext(product, group, filePaths, false);
    GroupUpdateContext &groupContext = filesContext.groupContext;

    if (!filesContext.absoluteFilePathsFromWildcards.isEmpty()) {
        throw ErrorInfo(Tr::tr("The following files cannot be removed from the project file, "
                               "because they match wildcard patterns: %1")
                .arg(filesContext.absoluteFilePathsFromWildcards.join(QLatin1String(", "))));
    }
    QStringList filesNotFound = filesContext.absoluteFilePaths;
    QList<SourceArtifactPtr> sourceArtifacts;
    for (const SourceArtifactPtr &sa : qAsConst(groupContext.resolvedGroups.first()->files)) {
        if (filesNotFound.removeOne(sa->absoluteFilePath))
            sourceArtifacts << sa;
    }
    if (!filesNotFound.isEmpty()) {
        throw ErrorInfo(Tr::tr("The following files are not known to qbs: %1")
                        .arg(filesNotFound.join(QLatin1String(", "))));
    }

    ProjectFileFilesRemover remover(groupContext.products.first(),
            group.isValid() ? groupContext.groups.first() : GroupData(),
            filesContext.relativeFilePaths);
    remover.apply();

    for (int i = 0; i < groupContext.resolvedProducts.count(); ++i) {
        removeFilesFromBuildGraph(groupContext.resolvedProducts.at(i), sourceArtifacts);
        for (const SourceArtifactPtr &sa : qAsConst(sourceArtifacts))
            groupContext.resolvedGroups.at(i)->files.removeOne(sa);
    }
    doSanityChecks(internalProject, logger);

    m_projectData.d.detach();
    updateInternalCodeLocations(internalProject, remover.itemPosition(), remover.lineOffset());
    updateExternalCodeLocations(m_projectData, remover.itemPosition(), remover.lineOffset());
    for (const GroupData &g : qAsConst(groupContext.groups)) {
        for (int i = g.d->sourceArtifacts.count() - 1; i >= 0; --i) {
            if (filesContext.absoluteFilePaths.contains(g.d->sourceArtifacts.at(i).filePath()))
                g.d->sourceArtifacts.removeAt(i);
        }
    }
}

void ProjectPrivate::removeGroup(const ProductData &product, const GroupData &group)
{
    GroupUpdateContext context = getGroupContext(product, group);

    ProjectFileGroupRemover remover(context.products.first(), context.groups.first());
    remover.apply();

    for (int i = 0; i < context.resolvedProducts.count(); ++i) {
        const ResolvedProductPtr &product = context.resolvedProducts.at(i);
        const GroupPtr &group = context.resolvedGroups.at(i);
        removeFilesFromBuildGraph(product, group->allFiles());
        const bool removed = product->groups.removeOne(group);
        QBS_CHECK(removed);
    }
    doSanityChecks(internalProject, logger);

    m_projectData.d.detach();
    updateInternalCodeLocations(internalProject, remover.itemPosition(), remover.lineOffset());
    updateExternalCodeLocations(m_projectData, remover.itemPosition(), remover.lineOffset());
    for (int i = 0; i < context.products.count(); ++i) {
        const bool removed = context.products.at(i).d->groups.removeOne(context.groups.at(i));
        QBS_CHECK(removed);
    }
}
#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

void ProjectPrivate::removeFilesFromBuildGraph(const ResolvedProductConstPtr &product,
                                               const QList<SourceArtifactPtr> &files)
{
    if (!product->enabled)
        return;
    QBS_CHECK(internalProject->buildData);
    ArtifactSet allRemovedArtifacts;
    for (const SourceArtifactPtr &sa : files) {
        ArtifactSet removedArtifacts;
        Artifact * const artifact = lookupArtifact(product, sa->absoluteFilePath);
        if (artifact) { // Can be null if the executor has not yet applied the respective rule.
            internalProject->buildData->removeArtifactAndExclusiveDependents(artifact, logger,
                    true, &removedArtifacts);
        }
        allRemovedArtifacts.unite(removedArtifacts);
    }
    EmptyDirectoriesRemover(product->topLevelProject(), logger)
            .removeEmptyParentDirectories(allRemovedArtifacts);
    qDeleteAll(allRemovedArtifacts);
}

static void updateLocationIfNecessary(CodeLocation &location, const CodeLocation &changeLocation,
                                      int lineOffset)
{
    if (location.filePath() == changeLocation.filePath()
            && location.line() >= changeLocation.line()) {
        location = CodeLocation(location.filePath(), location.line() + lineOffset,
                                location.column());
    }
}

void ProjectPrivate::updateInternalCodeLocations(const ResolvedProjectPtr &project,
        const CodeLocation &changeLocation, int lineOffset)
{
    if (lineOffset == 0)
        return;
    updateLocationIfNecessary(project->location, changeLocation, lineOffset);
    for (const ResolvedProjectPtr &subProject : qAsConst(project->subProjects))
        updateInternalCodeLocations(subProject, changeLocation, lineOffset);
    for (const ResolvedProductPtr &product : qAsConst(project->products)) {
        updateLocationIfNecessary(product->location, changeLocation, lineOffset);
        for (const GroupPtr &group : qAsConst(product->groups))
            updateLocationIfNecessary(group->location, changeLocation, lineOffset);
        for (const RulePtr &rule : qAsConst(product->rules)) {
            updateLocationIfNecessary(rule->prepareScript->location, changeLocation, lineOffset);
            for (const RuleArtifactPtr &artifact : qAsConst(rule->artifacts)) {
                for (auto &binding : artifact->bindings) {
                    updateLocationIfNecessary(binding.location, changeLocation, lineOffset);
                }
            }
        }
        for (const ResolvedScannerConstPtr &scanner : qAsConst(product->scanners)) {
            updateLocationIfNecessary(scanner->searchPathsScript->location, changeLocation, lineOffset);
            updateLocationIfNecessary(scanner->scanScript->location, changeLocation, lineOffset);
        }
        for (const ResolvedModuleConstPtr &module : qAsConst(product->modules)) {
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
    if (lineOffset == 0)
        return;
    updateLocationIfNecessary(project.d->location, changeLocation, lineOffset);
    for (const ProjectData &subProject : project.subProjects())
        updateExternalCodeLocations(subProject, changeLocation, lineOffset);
    for (const ProductData &product : project.products()) {
        updateLocationIfNecessary(product.d->location, changeLocation, lineOffset);
        for (const GroupData &group : product.groups())
            updateLocationIfNecessary(group.d->location, changeLocation, lineOffset);
    }
}

void ProjectPrivate::prepareChangeToProject()
{
    if (internalProject->locked)
        throw ErrorInfo(Tr::tr("A job is currently in process."));
    if (!m_projectData.isValid())
        retrieveProjectData(m_projectData, internalProject);
}

RuleCommandList ProjectPrivate::ruleCommands(const ProductData &product,
        const QString &inputFilePath, const QString &outputFileTag) const
{
    if (internalProject->locked)
        throw ErrorInfo(Tr::tr("A job is currently in process."));
    const ResolvedProductConstPtr resolvedProduct = internalProduct(product);
    if (!resolvedProduct)
        throw ErrorInfo(Tr::tr("No such product '%1'.").arg(product.name()));
    if (!resolvedProduct->enabled)
        throw ErrorInfo(Tr::tr("Product '%1' is disabled.").arg(product.name()));
    QBS_CHECK(resolvedProduct->buildData);
    const ArtifactSet &outputArtifacts = resolvedProduct->buildData->artifactsByFileTag
            .value(FileTag(outputFileTag.toLocal8Bit()));
    for (const Artifact * const outputArtifact : qAsConst(outputArtifacts)) {
        const TransformerConstPtr transformer = outputArtifact->transformer;
        if (!transformer)
            continue;
        for (const Artifact * const inputArtifact : qAsConst(transformer->inputs)) {
            if (inputArtifact->filePath() == inputFilePath) {
                RuleCommandList list;
                for (const AbstractCommandPtr &internalCommand : qAsConst(transformer->commands)) {
                    RuleCommand externalCommand;
                    externalCommand.d->description = internalCommand->description();
                    externalCommand.d->extendedDescription = internalCommand->extendedDescription();
                    switch (internalCommand->type()) {
                    case AbstractCommand::JavaScriptCommandType: {
                        externalCommand.d->type = RuleCommand::JavaScriptCommandType;
                        const JavaScriptCommandPtr &jsCmd
                                = std::static_pointer_cast<JavaScriptCommand>(internalCommand);
                        externalCommand.d->sourceCode = jsCmd->sourceCode();
                        break;
                    }
                    case AbstractCommand::ProcessCommandType: {
                        externalCommand.d->type = RuleCommand::ProcessCommandType;
                        const ProcessCommandPtr &procCmd
                                = std::static_pointer_cast<ProcessCommand>(internalCommand);
                        externalCommand.d->executable = procCmd->program();
                        externalCommand.d->arguments = procCmd->arguments();
                        externalCommand.d->workingDir = procCmd->workingDir();
                        externalCommand.d->environment = procCmd->environment();
                        break;
                    }
                    }
                    list << externalCommand;
                }
                return list;
            }
        }
    }

    throw ErrorInfo(Tr::tr("No rule was found that produces an artifact tagged '%1' "
                           "from input file '%2'.").arg(outputFileTag, inputFilePath));
}

static bool productIsRunnable(const ResolvedProductConstPtr &product)
{
    return product->fileTags.contains("application");
}

static bool productIsMultiplexed(const ResolvedProductConstPtr &product)
{
    return product->productProperties.value(QStringLiteral("multiplexed")).toBool();
}

void ProjectPrivate::retrieveProjectData(ProjectData &projectData,
                                         const ResolvedProjectConstPtr &internalProject)
{
    projectData.d->name = internalProject->name;
    projectData.d->location = internalProject->location;
    projectData.d->enabled = internalProject->enabled;
    for (const ResolvedProductConstPtr &resolvedProduct : qAsConst(internalProject->products)) {
        ProductData product;
        product.d->type = resolvedProduct->fileTags.toStringList();
        product.d->name = resolvedProduct->name;
        product.d->targetName = resolvedProduct->targetName;
        product.d->version = resolvedProduct->productProperties
                                                        .value(QLatin1String("version")).toString();
        product.d->profile = resolvedProduct->profile;
        product.d->multiplexConfigurationId = resolvedProduct->multiplexConfigurationId;
        product.d->location = resolvedProduct->location;
        product.d->buildDirectory = resolvedProduct->buildDirectory();
        product.d->isEnabled = resolvedProduct->enabled;
        product.d->isRunnable = productIsRunnable(resolvedProduct);
        product.d->isMultiplexed = productIsMultiplexed(resolvedProduct);
        product.d->properties = resolvedProduct->productProperties;
        product.d->moduleProperties.d->m_map = resolvedProduct->moduleProperties;
        for (const GroupPtr &resolvedGroup : qAsConst(resolvedProduct->groups))
            product.d->groups << createGroupDataFromGroup(resolvedGroup, resolvedProduct);
        if (resolvedProduct->enabled) {
            QBS_CHECK(resolvedProduct->buildData);
            const ArtifactSet targetArtifacts = resolvedProduct->targetArtifacts();
            for (Artifact * const a : filterByType<Artifact>(resolvedProduct->buildData->nodes)) {
                if (a->artifactType != Artifact::Generated)
                    continue;
                ArtifactData ta;
                ta.d->filePath = a->filePath();
                ta.d->fileTags = a->fileTags().toStringList();
                ta.d->properties.d->m_map = a->properties;
                ta.d->isGenerated = true;
                ta.d->isTargetArtifact = targetArtifacts.contains(a);
                ta.d->isValid = true;
                setupInstallData(ta, resolvedProduct);
                product.d->generatedArtifacts << ta;
            }
            for (auto it = resolvedProduct->buildData->rescuableArtifactData.constBegin();
                 it != resolvedProduct->buildData->rescuableArtifactData.constEnd(); ++it) {
                ArtifactData ta;
                ta.d->filePath = it.key();
                ta.d->fileTags = it.value().fileTags.toStringList();
                ta.d->properties.d->m_map = it.value().properties;
                ta.d->isGenerated = true;
                ta.d->isTargetArtifact = resolvedProduct->fileTags.intersects(it.value().fileTags);
                ta.d->isValid = true;
                setupInstallData(ta, resolvedProduct);
                product.d->generatedArtifacts << ta;
            }
        }
        for (const ResolvedProductPtr &resolvedDependentProduct
             : qAsConst(resolvedProduct->dependencies)) {
            product.d->dependencies << resolvedDependentProduct->name;
        }
        qSort(product.d->type);
        qSort(product.d->groups);
        qSort(product.d->generatedArtifacts);
        qSort(product.d->dependencies);
        product.d->isValid = true;
        projectData.d->products << product;
    }
    for (const ResolvedProjectConstPtr &internalSubProject
         : qAsConst(internalProject->subProjects)) {
        if (!internalSubProject->enabled)
            continue;
        ProjectData subProject;
        retrieveProjectData(subProject, internalSubProject);
        projectData.d->subProjects << subProject;
    }
    projectData.d->isValid = true;
    qSort(projectData.d->products);
    qSort(projectData.d->subProjects);
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

/*!
 * \brief The top-level profile for building this project.
 */
QString Project::profile() const
{
    QBS_ASSERT(isValid(), return QString());
    return d->internalProject->profile();
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
 * If the function is called on a valid \c Project object, the build graph will not be loaded
 * from a file, but will be taken from the existing project. In that case, if resolving
 * finishes successfully, the existing project will be invalidated. If resolving fails, qbs will
 * try to keep the existing project valid. However, under certain circumstances, resolving the new
 * project will fail at a time where existing project data has already been touched, in which case
 * the existing project has to be invalidated (this could be avoided, but it would hurt performance).
 * So after an unsuccessful re-resolve job, the existing project may or may not be valid anymore.
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
        job->resolve(*this, parameters);
        QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
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
    QBS_ASSERT(isValid(), return ProjectData());
    return d->projectData();
}

RunEnvironment Project::getRunEnvironment(const ProductData &product,
        const InstallOptions &installOptions,
        const QProcessEnvironment &environment, Settings *settings) const
{
    const ResolvedProductPtr resolvedProduct = d->internalProduct(product);
    return RunEnvironment(resolvedProduct, installOptions, environment, settings, d->logger);
}

/*!
 * \enum Project::ProductSelection
 * This enum type specifies which products to include if "all" products are to be built.
 * \value Project::ProdProductSelectionDefaultOnly Indicates that only those products should be
 *                                                 built whose \c builtByDefault property
 *                                                 is \c true.
 * \value Project::ProdProductSelectionWithNonDefault Indicates that products whose
 *                                                    \c builtByDefault property is \c false should
 *                                                    also be built.
 */

/*!
 * \brief Causes all products of this project to be built, if necessary.
 * If and only if \c producSelection is \c Project::ProductSelectionWithNonDefault, products with
 * the \c builtByDefault property set to \c false will be built too.
 * The function will finish immediately, returning a \c BuildJob identifiying the operation.
 */
BuildJob *Project::buildAllProducts(const BuildOptions &options, ProductSelection productSelection,
                                    QObject *jobOwner) const
{
    QBS_ASSERT(isValid(), return 0);
    const bool includingNonDefault = productSelection == ProductSelectionWithNonDefault;
    return d->buildProducts(d->allEnabledInternalProducts(includingNonDefault), options,
                            !includingNonDefault, jobOwner);
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
    QBS_ASSERT(isValid(), return 0);
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
    QBS_ASSERT(isValid(), return 0);
    return d->cleanProducts(d->allEnabledInternalProducts(true), options, jobOwner);
}

/*!
 * \brief Removes the build artifacts of the given products.
 * The function will finish immediately, returning a \c CleanJob identifiying this operation.
 */
CleanJob *Project::cleanSomeProducts(const QList<ProductData> &products,
        const CleanOptions &options, QObject *jobOwner) const
{
    QBS_ASSERT(isValid(), return 0);
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
 * If and only if \c producSelection is \c Project::ProductSelectionWithNonDefault, products with
 * the \c builtByDefault property set to \c false will be installed too.
 * The function will finish immediately, returning an \c InstallJob identifiying this operation.
 */
InstallJob *Project::installAllProducts(const InstallOptions &options,
                                        ProductSelection productSelection, QObject *jobOwner) const
{
    QBS_ASSERT(isValid(), return 0);
    const bool includingNonDefault = productSelection == ProductSelectionWithNonDefault;
    return d->installProducts(d->allEnabledInternalProducts(includingNonDefault), options,
                              !includingNonDefault, jobOwner);
}

/*!
 * \brief Installs the installable files of the given products.
 * The function will finish immediately, returning an \c InstallJob identifiying this operation.
 */
InstallJob *Project::installSomeProducts(const QList<ProductData> &products,
                                         const InstallOptions &options, QObject *jobOwner) const
{
    QBS_ASSERT(isValid(), return 0);
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
    QBS_ASSERT(isValid(), return);
    TimestampsUpdater().updateTimestamps(d->internalProject, d->internalProducts(products),
                                         d->logger);
}

/*!
 * \brief Finds files generated from the given file in the given product.
 * If \a recursive is \c false, only files generated directly from \a file will be considered,
 * otherwise the generated files are collected recursively.
 * If \a tags is not empty, only generated files matching at least one of these tags will
 * be considered.
 */
QStringList Project::generatedFiles(const ProductData &product, const QString &file,
                                    bool recursive, const QStringList &tags) const
{
    QBS_ASSERT(isValid(), return QStringList());
    const ResolvedProductConstPtr internalProduct = d->internalProduct(product);
    return internalProduct->generatedFiles(file, recursive, FileTags::fromStringList(tags));
}

QVariantMap Project::projectConfiguration() const
{
    QBS_ASSERT(isValid(), return QVariantMap());
    return d->internalProject->buildConfiguration();
}

QHash<QString, QString> Project::usedEnvironment() const
{
    typedef QHash<QString, QString> EnvType;
    QBS_ASSERT(isValid(), return EnvType());
    return d->internalProject->usedEnvironment;
}

std::set<QString> Project::buildSystemFiles() const
{
    QBS_ASSERT(isValid(), return std::set<QString>());
    return d->internalProject->buildSystemFiles.toStdSet();
}

RuleCommandList Project::ruleCommands(const ProductData &product,
        const QString &inputFilePath, const QString &outputFileTag, ErrorInfo *error) const
{
    QBS_ASSERT(isValid(), return RuleCommandList());
    QBS_ASSERT(product.isValid(), return RuleCommandList());

    try {
        return d->ruleCommands(product, inputFilePath, outputFileTag);
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return RuleCommandList();
    }
}

ErrorInfo Project::dumpNodesTree(QIODevice &outDevice, const QList<ProductData> &products)
{
    try {
        NodeTreeDumper(outDevice).start(d->internalProducts(products));
    } catch (const ErrorInfo &e) {
        return e;
    }
    return ErrorInfo();
}

Project::BuildGraphInfo Project::getBuildGraphInfo(const QString &bgFilePath,
                                                   const QStringList &requestedProperties)
{
    BuildGraphInfo info;
    try {
        const Internal::TopLevelProjectConstPtr project = BuildGraphLoader::loadProject(bgFilePath);
        info.bgFilePath = bgFilePath;
        info.overriddenProperties = project->overriddenValues;
        info.profileData = project->profileConfigs;
        std::vector<std::pair<QString, QString>> props;
        for (const QString &prop : requestedProperties) {
            QStringList components = prop.split(QLatin1Char('.'));
            const QString propName = components.takeLast();
            props.push_back(std::make_pair(components.join(QLatin1Char('.')), propName));
        }
        for (const ResolvedProductConstPtr &product : project->allProducts()) {
            if (props.empty())
                break;
            if (product->profile != project->profile())
                continue;
            for (auto it = props.begin(); it != props.end();) {
                const QVariant value
                        = product->moduleProperties->moduleProperty(it->first, it->second);
                if (value.isValid()) {
                    info.requestedProperties.insert(it->first + QLatin1Char('.') + it->second,
                                                    value);
                    it = props.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } catch (const ErrorInfo &e) {
        info.error = e;
    }
    return info;
}

#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
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
        QBS_CHECK(isValid());
        d->prepareChangeToProject();
        d->addGroup(product, groupName);
        d->internalProject->lastResolveTime = FileTime::currentTime();
        d->internalProject->store(d->logger);
        return ErrorInfo();
    } catch (ErrorInfo errorInfo) {
        errorInfo.prepend(Tr::tr("Failure adding group '%1' to product '%2'.")
                          .arg(groupName, product.name()));
        return errorInfo;
    }
}

/*!
 * \brief Adds the given files to the given product.
 * If \c group is a default-constructed object, the files will be added to the product's
 * "files" property, otherwise to the one of \c group.
 * The file paths can be absolute or relative to the location of \c product (including a possible
 * prefix in the group). The project file will always contain relative paths.
 * After calling this function, it is recommended to re-fetch the project data, as other
 * items can be affected.
 * \sa qbs::Project::projectData()
 */
ErrorInfo Project::addFiles(const ProductData &product, const GroupData &group,
                            const QStringList &filePaths)
{
    try {
        QBS_CHECK(isValid());
        d->prepareChangeToProject();
        d->addFiles(product, group, filePaths);
        d->internalProject->lastResolveTime = FileTime::currentTime();
        d->internalProject->store(d->logger);
        return ErrorInfo();
    } catch (ErrorInfo errorInfo) {
        errorInfo.prepend(Tr::tr("Failure adding files to product."));
        return errorInfo;
    }
}

/*!
 * \brief Removes the given files from the given product.
 * If \c group is a default-constructed object, the files will be removed from the product's
 * "files" property, otherwise from the one of \c group.
 * The file paths can be absolute or relative to the location of \c product (including a possible
 * prefix in the group).
 * After calling this function, it is recommended to re-fetch the project data, as other
 * items can be affected.
 * \sa qbs::Project::projectData()
 */
ErrorInfo Project::removeFiles(const ProductData &product, const GroupData &group,
                               const QStringList &filePaths)
{
    try {
        QBS_CHECK(isValid());
        d->prepareChangeToProject();
        d->removeFiles(product, group, filePaths);
        d->internalProject->lastResolveTime = FileTime::currentTime();
        d->internalProject->store(d->logger);
        return ErrorInfo();
    } catch (ErrorInfo errorInfo) {
        errorInfo.prepend(Tr::tr("Failure removing files from product '%1'.").arg(product.name()));
        return errorInfo;
    }
}

/*!
 * \brief Removes the given group from the given product.
 * After calling this function, it is recommended to re-fetch the project data, as other
 * items can be affected.
 * \sa qbs::Project::projectData()
 */
ErrorInfo Project::removeGroup(const ProductData &product, const GroupData &group)
{
    try {
        QBS_CHECK(isValid());
        d->prepareChangeToProject();
        d->removeGroup(product, group);
        d->internalProject->lastResolveTime = FileTime::currentTime();
        d->internalProject->store(d->logger);
        return ErrorInfo();
    } catch (ErrorInfo errorInfo) {
        errorInfo.prepend(Tr::tr("Failure removing group '%1' from product '%2'.")
                          .arg(group.name(), product.name()));
        return errorInfo;
    }
}
#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

} // namespace qbs
