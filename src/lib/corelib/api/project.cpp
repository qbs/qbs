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
#include "transformerdata_p.h"
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
#include <buildgraph/rulesapplicator.h>
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
#include <tools/stringconstants.h>
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

    std::vector<std::string> pluginPaths;
    for (const QString &pluginPath : _pluginPaths) {
        if (!FileInfo::exists(pluginPath)) {
#ifndef QBS_STATIC_LIB
            logger.qbsWarning() << Tr::tr("Plugin path '%1' does not exist.")
                                    .arg(QDir::toNativeSeparators(pluginPath));
#endif
        } else {
            pluginPaths.push_back(pluginPath.toStdString());
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

static void addDependencies(QVector<ResolvedProductPtr> &products)
{
    for (int i = 0; i < products.size(); ++i) {
        const ResolvedProductPtr &product = products.at(i);
        for (const ResolvedProductPtr &dependency : qAsConst(product->dependencies)) {
            if (!products.contains(dependency))
                products.push_back(dependency);
        }
    }
}

BuildJob *ProjectPrivate::buildProducts(const QVector<ResolvedProductPtr> &products,
                                        const BuildOptions &options, bool needsDepencencyResolving,
                                        QObject *jobOwner)
{
    QVector<ResolvedProductPtr> productsToBuild = products;
    if (needsDepencencyResolving)
        addDependencies(productsToBuild);

    const auto job = new BuildJob(logger, jobOwner);
    job->build(internalProject, productsToBuild, options);
    QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
    return job;
}

CleanJob *ProjectPrivate::cleanProducts(const QVector<ResolvedProductPtr> &products,
        const CleanOptions &options, QObject *jobOwner)
{
    const auto job = new CleanJob(logger, jobOwner);
    job->clean(internalProject, products, options);
    QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
    return job;
}

InstallJob *ProjectPrivate::installProducts(const QVector<ResolvedProductPtr> &products,
        const InstallOptions &options, bool needsDepencencyResolving, QObject *jobOwner)
{
    QVector<ResolvedProductPtr> productsToInstall = products;
    if (needsDepencencyResolving)
        addDependencies(productsToInstall);
    const auto job = new InstallJob(logger, jobOwner);
    job->install(internalProject, productsToInstall, options);
    QBS_ASSERT(job->state() == AbstractJob::StateRunning,);
    return job;
}

QVector<ResolvedProductPtr> ProjectPrivate::internalProducts(const QList<ProductData> &products) const
{
    QVector<ResolvedProductPtr> internalProducts;
    for (const ProductData &product : products) {
        if (product.isEnabled())
            internalProducts.push_back(internalProduct(product));
    }
    return internalProducts;
}

static QVector<ResolvedProductPtr> enabledInternalProducts(const ResolvedProjectConstPtr &project,
                                                         bool includingNonDefault)
{
    QVector<ResolvedProductPtr> products;
    for (const ResolvedProductPtr &p : project->products) {
        if (p->enabled && (includingNonDefault || p->builtByDefault()))
            products.push_back(p);
    }
    for (const auto &subProject : qAsConst(project->subProjects))
        products << enabledInternalProducts(subProject, includingNonDefault);
    return products;
}

QVector<ResolvedProductPtr> ProjectPrivate::allEnabledInternalProducts(
        bool includingNonDefault) const
{
    return enabledInternalProducts(internalProject, includingNonDefault);
}

static bool matches(const ProductData &product, const ResolvedProductConstPtr &rproduct)
{
    return product.name() == rproduct->name
            && product.multiplexConfigurationId() == rproduct->multiplexConfigurationId;
}

static ResolvedProductPtr internalProductForProject(const ResolvedProjectConstPtr &project,
                                                    const ProductData &product)
{
    for (const ResolvedProductPtr &resolvedProduct : project->products) {
        if (matches(product, resolvedProduct))
            return resolvedProduct;
    }
    for (const auto &subProject : qAsConst(project->subProjects)) {
        const ResolvedProductPtr &p = internalProductForProject(subProject, product);
        if (p)
            return p;
    }
    return {};
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
    return {};
}

QList<ProductData> ProjectPrivate::findProductsByName(const QString &name) const
{
    QList<ProductData> list;
    for (const ProductData &p : m_projectData.allProducts()) {
        if (p.name() == name)
            list.push_back(p);
    }
    return list;
}

GroupData ProjectPrivate::findGroupData(const ProductData &product, const QString &groupName) const
{
    for (const GroupData &g : product.groups()) {
        if (g.name() == groupName)
            return g;
    }
    return {};
}

GroupData ProjectPrivate::createGroupDataFromGroup(const GroupPtr &resolvedGroup,
                                                   const ResolvedProductConstPtr &product)
{
    GroupData group;
    group.d->name = resolvedGroup->name;
    group.d->prefix = resolvedGroup->prefix;
    group.d->location = resolvedGroup->location;
    for (const auto &sa : resolvedGroup->files) {
        ArtifactData artifact = createApiSourceArtifact(sa);
        setupInstallData(artifact, product);
        group.d->sourceArtifacts.push_back(artifact);
    }
    if (resolvedGroup->wildcards) {
        for (const auto &sa : resolvedGroup->wildcards->files) {
            ArtifactData artifact = createApiSourceArtifact(sa);
            setupInstallData(artifact, product);
            group.d->sourceArtifactsFromWildcards.push_back(artifact);
        }
    }
    std::sort(group.d->sourceArtifacts.begin(),
              group.d->sourceArtifacts.end());
    std::sort(group.d->sourceArtifactsFromWildcards.begin(),
              group.d->sourceArtifactsFromWildcards.end());
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

ArtifactData ProjectPrivate::createArtifactData(const Artifact *artifact,
        const ResolvedProductConstPtr &product, const ArtifactSet &targetArtifacts)
{
    ArtifactData ta;
    ta.d->filePath = artifact->filePath();
    ta.d->fileTags = artifact->fileTags().toStringList();
    ta.d->properties.d->m_map = artifact->properties;
    ta.d->isGenerated = artifact->artifactType == Artifact::Generated;
    ta.d->isTargetArtifact = targetArtifacts.contains(const_cast<Artifact *>(artifact));
    ta.d->isValid = true;
    setupInstallData(ta, product);
    return ta;
}

void ProjectPrivate::setupInstallData(ArtifactData &artifact,
                                      const ResolvedProductConstPtr &product)
{
    artifact.d->installData.d->isValid = true;
    artifact.d->installData.d->isInstallable = artifact.properties().getModuleProperty(
                StringConstants::qbsModule(), StringConstants::installProperty()).toBool();
    if (!artifact.d->installData.d->isInstallable)
        return;
    const QString installRoot = artifact.properties().getModuleProperty(
                StringConstants::qbsModule(), StringConstants::installRootProperty()).toString();
    InstallOptions options;
    options.setInstallRoot(installRoot);
    artifact.d->installData.d->installRoot = installRoot;
    try {
        QString installFilePath = ProductInstaller::targetFilePath(product->topLevelProject(),
                product->sourceDirectory, artifact.filePath(), artifact.properties().d->m_map,
                options);
        if (!installRoot.isEmpty())
            installFilePath.remove(0, installRoot.size());
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
    if (products.empty())
        throw ErrorInfo(Tr::tr("Product '%1' does not exist.").arg(product.name()));
    const auto resolvedProducts = internalProducts(products);
    QBS_CHECK(products.size() == resolvedProducts.size());

    for (const GroupPtr &resolvedGroup : resolvedProducts.front()->groups) {
        if (resolvedGroup->name == groupName) {
            throw ErrorInfo(Tr::tr("Group '%1' already exists in product '%2'.")
                            .arg(groupName, product.name()), resolvedGroup->location);
        }
    }

    ProjectFileGroupInserter groupInserter(products.front(), groupName);
    groupInserter.apply();
}

ProjectPrivate::GroupUpdateContext ProjectPrivate::getGroupContext(const ProductData &product,
                                                                   const GroupData &group)
{
    GroupUpdateContext context;
    if (!product.isValid())
        throw ErrorInfo(Tr::tr("Product is invalid."));
    context.products = findProductsByName(product.name());
    if (context.products.empty())
        throw ErrorInfo(Tr::tr("Product '%1' does not exist.").arg(product.name()));
    context.resolvedProducts = internalProducts(context.products);

    const QString groupName = group.isValid() ? group.name() : product.name();
    for (const ResolvedProductPtr &p : qAsConst(context.resolvedProducts)) {
        for (const GroupPtr &g : p->groups) {
            if (g->name == groupName) {
                context.resolvedGroups << g;
                break;
            }
        }
    }
    if (context.resolvedGroups.empty())
        throw ErrorInfo(Tr::tr("Group '%1' does not exist.").arg(groupName));
    for (const ProductData &p : qAsConst(context.products)) {
        const GroupData &g = findGroupData(p, groupName);
        QBS_CHECK(p.isValid());
        context.groups << g;
    }
    QBS_CHECK(context.resolvedProducts.size() == context.products.size());
    QBS_CHECK(context.resolvedProducts.size() == context.resolvedGroups.size());
    QBS_CHECK(context.products.size() == context.groups.size());
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

    if (filePaths.empty())
        throw ErrorInfo(Tr::tr("No files supplied."));

    QString prefix;
    for (int i = 0; i < groupContext.resolvedGroups.size(); ++i) {
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
        if (matchesWildcard(absPath, groupContext.resolvedGroups.front())) {
            filesContext.absoluteFilePathsFromWildcards << absPath;
        } else {
            filesContext.absoluteFilePaths << absPath;
            filesContext.relativeFilePaths << baseDir.relativeFilePath(absPath);
        }
    }

    return filesContext;
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
            for (const auto &sa : group->files) {
                if (sa->absoluteFilePath == filePath) {
                    throw ErrorInfo(Tr::tr("File '%1' already exists in group '%2'.")
                                    .arg(filePath, group->name));
                }
            }
        }
    }

    ProjectFileFilesAdder adder(groupContext.products.front(),
            group.isValid() ? groupContext.groups.front() : GroupData(),
            filesContext.relativeFilePaths);
    adder.apply();
}

void ProjectPrivate::removeFiles(const ProductData &product, const GroupData &group,
                                 const QStringList &filePaths)
{
    FileListUpdateContext filesContext = getFileListContext(product, group, filePaths, false);
    GroupUpdateContext &groupContext = filesContext.groupContext;

    if (!filesContext.absoluteFilePathsFromWildcards.empty()) {
        throw ErrorInfo(Tr::tr("The following files cannot be removed from the project file, "
                               "because they match wildcard patterns: %1")
                .arg(filesContext.absoluteFilePathsFromWildcards.join(QLatin1String(", "))));
    }
    QStringList filesNotFound = filesContext.absoluteFilePaths;
    std::vector<SourceArtifactPtr> sourceArtifacts;
    for (const SourceArtifactPtr &sa : groupContext.resolvedGroups.front()->files) {
        if (filesNotFound.removeOne(sa->absoluteFilePath))
            sourceArtifacts << sa;
    }
    if (!filesNotFound.empty()) {
        throw ErrorInfo(Tr::tr("The following files are not known to qbs: %1")
                        .arg(filesNotFound.join(QLatin1String(", "))));
    }

    ProjectFileFilesRemover remover(groupContext.products.front(),
            group.isValid() ? groupContext.groups.front() : GroupData(),
            filesContext.relativeFilePaths);
    remover.apply();
}

void ProjectPrivate::removeGroup(const ProductData &product, const GroupData &group)
{
    GroupUpdateContext context = getGroupContext(product, group);
    ProjectFileGroupRemover remover(context.products.front(), context.groups.front());
    remover.apply();

}
#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

void ProjectPrivate::prepareChangeToProject()
{
    if (internalProject->locked)
        throw ErrorInfo(Tr::tr("A job is currently in progress."));
    if (!m_projectData.isValid())
        retrieveProjectData(m_projectData, internalProject);
}

RuleCommandList ProjectPrivate::ruleCommandListForTransformer(const Transformer *transformer)
{
    RuleCommandList list;
    for (const AbstractCommandPtr &internalCommand : qAsConst(transformer->commands.commands())) {
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

RuleCommandList ProjectPrivate::ruleCommands(const ProductData &product,
        const QString &inputFilePath, const QString &outputFileTag)
{
    if (internalProject->locked)
        throw ErrorInfo(Tr::tr("A job is currently in progress."));
    const ResolvedProductConstPtr resolvedProduct = internalProduct(product);
    if (!resolvedProduct)
        throw ErrorInfo(Tr::tr("No such product '%1'.").arg(product.name()));
    if (!resolvedProduct->enabled)
        throw ErrorInfo(Tr::tr("Product '%1' is disabled.").arg(product.name()));
    QBS_CHECK(resolvedProduct->buildData);
    const ArtifactSet &outputArtifacts = resolvedProduct->buildData->artifactsByFileTag()
            .value(FileTag(outputFileTag.toLocal8Bit()));
    for (const Artifact * const outputArtifact : qAsConst(outputArtifacts)) {
        const TransformerConstPtr transformer = outputArtifact->transformer;
        if (!transformer)
            continue;
        for (const Artifact * const inputArtifact : qAsConst(transformer->inputs)) {
            if (inputArtifact->filePath() == inputFilePath)
                return ruleCommandListForTransformer(transformer.get());
        }
    }

    throw ErrorInfo(Tr::tr("No rule was found that produces an artifact tagged '%1' "
                           "from input file '%2'.").arg(outputFileTag, inputFilePath));
}

ProjectTransformerData ProjectPrivate::transformerData()
{
    if (!m_projectData.isValid())
        retrieveProjectData(m_projectData, internalProject);
    ProjectTransformerData projectTransformerData;
    for (const ProductData &productData : m_projectData.allProducts()) {
        if (!productData.isEnabled())
            continue;
        const ResolvedProductConstPtr product = internalProduct(productData);
        QBS_ASSERT(!!product, continue);
        QBS_ASSERT(!!product->buildData, continue);
        const ArtifactSet targetArtifacts = product->targetArtifacts();
        Set<const Transformer *> allTransformers;
        for (const Artifact * const a : TypeFilter<Artifact>(product->buildData->allNodes())) {
            if (a->artifactType == Artifact::Generated)
                allTransformers.insert(a->transformer.get());
        }
        if (allTransformers.empty())
            continue;
        ProductTransformerData productTransformerData;
        for (const Transformer * const t : allTransformers) {
            TransformerData tData;
            Set<const Artifact *> allInputs;
            for (Artifact * const a : t->outputs) {
                tData.d->outputs << createArtifactData(a, product, targetArtifacts);
                for (const Artifact * const child : filterByType<Artifact>(a->children))
                    allInputs << child;
                for (Artifact * const a
                     : RulesApplicator::collectAuxiliaryInputs(t->rule.get(), product.get())) {
                    if (a->artifactType == Artifact::Generated)
                        tData.d->inputs << createArtifactData(a, product, targetArtifacts);
                }
            }
            for (const Artifact * const input : allInputs)
                tData.d->inputs << createArtifactData(input, product, targetArtifacts);
            tData.d->commands = ruleCommandListForTransformer(t);
            productTransformerData << tData;
        }
        projectTransformerData << qMakePair(productData, productTransformerData);
    }
    return projectTransformerData;
}

static bool productIsRunnable(const ResolvedProductConstPtr &product)
{
    const bool isBundle = product->moduleProperties->moduleProperty(
                QStringLiteral("bundle"), QStringLiteral("isBundle")).toBool();
    return isRunnableArtifact(product->fileTags, isBundle);
}

static bool productIsMultiplexed(const ResolvedProductConstPtr &product)
{
    return product->productProperties.value(StringConstants::multiplexedProperty()).toBool();
}

void ProjectPrivate::retrieveProjectData(ProjectData &projectData,
                                         const ResolvedProjectConstPtr &internalProject)
{
    projectData.d->name = internalProject->name;
    projectData.d->location = internalProject->location;
    projectData.d->enabled = internalProject->enabled;
    for (const auto &resolvedProduct : internalProject->products) {
        ProductData product;
        product.d->type = resolvedProduct->fileTags.toStringList();
        product.d->name = resolvedProduct->name;
        product.d->targetName = resolvedProduct->targetName;
        product.d->version = resolvedProduct
                ->productProperties.value(StringConstants::versionProperty()).toString();
        product.d->multiplexConfigurationId = resolvedProduct->multiplexConfigurationId;
        product.d->location = resolvedProduct->location;
        product.d->buildDirectory = resolvedProduct->buildDirectory();
        product.d->isEnabled = resolvedProduct->enabled;
        product.d->isRunnable = productIsRunnable(resolvedProduct);
        product.d->isMultiplexed = productIsMultiplexed(resolvedProduct);
        product.d->properties = resolvedProduct->productProperties;
        product.d->moduleProperties.d->m_map = resolvedProduct->moduleProperties;
        for (const GroupPtr &resolvedGroup : resolvedProduct->groups) {
            if (resolvedGroup->targetOfModule.isEmpty())
                product.d->groups << createGroupDataFromGroup(resolvedGroup, resolvedProduct);
        }
        if (resolvedProduct->enabled) {
            QBS_CHECK(resolvedProduct->buildData);
            const ArtifactSet targetArtifacts = resolvedProduct->targetArtifacts();
            for (Artifact * const a
                 : filterByType<Artifact>(resolvedProduct->buildData->allNodes())) {
                if (a->artifactType != Artifact::Generated)
                    continue;
                product.d->generatedArtifacts << createArtifactData(a, resolvedProduct,
                                                                    targetArtifacts);
            }
            const AllRescuableArtifactData &rad
                    = resolvedProduct->buildData->rescuableArtifactData();
            for (auto it = rad.begin(); it != rad.end(); ++it) {
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
            product.d->dependencies << resolvedDependentProduct->name; // FIXME: Shouldn't this be a unique name?
        }
        std::sort(product.d->type.begin(), product.d->type.end());
        std::sort(product.d->groups.begin(), product.d->groups.end());
        std::sort(product.d->generatedArtifacts.begin(), product.d->generatedArtifacts.end());
        product.d->isValid = true;
        projectData.d->products << product;
    }
    for (const auto &internalSubProject : qAsConst(internalProject->subProjects)) {
        if (!internalSubProject->enabled)
            continue;
        ProjectData subProject;
        retrieveProjectData(subProject, internalSubProject);
        projectData.d->subProjects << subProject;
    }
    projectData.d->isValid = true;
    std::sort(projectData.d->products.begin(), projectData.d->products.end());
    std::sort(projectData.d->subProjects.begin(), projectData.d->subProjects.end());
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

Project::Project(const Project &other) = default;

Project::~Project() = default;

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
    QBS_ASSERT(isValid(), return {});
    return d->internalProject->profile();
}

Project &Project::operator=(const Project &other) = default;

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
    const auto job = new SetupProjectJob(logger, jobOwner);
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

Project::Project() = default;

/*!
 * \brief Retrieves information for this project.
 * Call this function if you need insight into the project structure, e.g. because you want to know
 * which products or files are in it.
 */
ProjectData Project::projectData() const
{
    QBS_ASSERT(isValid(), return {});
    return d->projectData();
}

RunEnvironment Project::getRunEnvironment(const ProductData &product,
        const InstallOptions &installOptions,
        const QProcessEnvironment &environment,
        const QStringList &setupRunEnvConfig, Settings *settings) const
{
    const ResolvedProductPtr resolvedProduct = d->internalProduct(product);
    return RunEnvironment(resolvedProduct, d->internalProject, installOptions, environment,
                          setupRunEnvConfig, settings, d->logger);
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
    QBS_ASSERT(isValid(), return nullptr);
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
    QBS_ASSERT(isValid(), return nullptr);
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
    QBS_ASSERT(isValid(), return nullptr);
    return d->cleanProducts(d->allEnabledInternalProducts(true), options, jobOwner);
}

/*!
 * \brief Removes the build artifacts of the given products.
 * The function will finish immediately, returning a \c CleanJob identifiying this operation.
 */
CleanJob *Project::cleanSomeProducts(const QList<ProductData> &products,
        const CleanOptions &options, QObject *jobOwner) const
{
    QBS_ASSERT(isValid(), return nullptr);
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
    QBS_ASSERT(isValid(), return nullptr);
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
    QBS_ASSERT(isValid(), return nullptr);
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
    QBS_ASSERT(isValid(), return {});
    const ResolvedProductConstPtr internalProduct = d->internalProduct(product);
    return internalProduct->generatedFiles(file, recursive, FileTags::fromStringList(tags));
}

QVariantMap Project::projectConfiguration() const
{
    QBS_ASSERT(isValid(), return {});
    return d->internalProject->buildConfiguration();
}

std::set<QString> Project::buildSystemFiles() const
{
    QBS_ASSERT(isValid(), return {});
    return d->internalProject->buildSystemFiles.toStdSet();
}

RuleCommandList Project::ruleCommands(const ProductData &product,
        const QString &inputFilePath, const QString &outputFileTag, ErrorInfo *error) const
{
    QBS_ASSERT(isValid(), return {});
    QBS_ASSERT(product.isValid(), return {});

    try {
        return d->ruleCommands(product, inputFilePath, outputFileTag);
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return {};
    }
}

ProjectTransformerData Project::transformerData(ErrorInfo *error) const
{
    QBS_ASSERT(isValid(), return {});
    try {
        return d->transformerData();
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return {};
    }
}

ErrorInfo Project::dumpNodesTree(QIODevice &outDevice, const QList<ProductData> &products)
{
    try {
        NodeTreeDumper(outDevice).start(d->internalProducts(products));
    } catch (const ErrorInfo &e) {
        return e;
    }
    return {};
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
            props.emplace_back(components.join(QLatin1Char('.')), propName);
        }
        for (const auto &product : project->allProducts()) {
            if (props.empty())
                break;
            if (product->profile() != project->profile())
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

Project::BuildGraphInfo Project::getBuildGraphInfo() const
{
    QBS_ASSERT(isValid(), return {});
    BuildGraphInfo info;
    try {
        if (d->internalProject->locked)
            throw ErrorInfo(Tr::tr("A job is currently in progress."));
        info.bgFilePath = d->internalProject->buildGraphFilePath();
        info.overriddenProperties = d->internalProject->overriddenValues;
        info.profileData = d->internalProject->profileConfigs;
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
        d->internalProject->store(d->logger);
        return {};
    } catch (const ErrorInfo &exception) {
        auto errorInfo = exception;
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
        d->internalProject->store(d->logger);
        return {};
    } catch (const ErrorInfo &exception) {
        auto errorInfo = exception;
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
        d->internalProject->store(d->logger);
        return {};
    } catch (const ErrorInfo &exception) {
        auto errorInfo = exception;
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
        d->internalProject->store(d->logger);
        return {};
    } catch (const ErrorInfo &exception) {
        auto errorInfo = exception;
        errorInfo.prepend(Tr::tr("Failure removing group '%1' from product '%2'.")
                          .arg(group.name(), product.name()));
        return errorInfo;
    }
}
#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

} // namespace qbs
