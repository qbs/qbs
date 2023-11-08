/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#pragma once

#include <language/filetags.h>
#include <language/forward_decls.h>
#include <language/item.h>
#include <language/moduleproviderinfo.h>
#include <language/propertydeclaration.h>
#include <language/qualifiedid.h>
#include <parser/qmljsengine_p.h>
#include <tools/filetime.h>
#include <tools/joblimits.h>
#include <tools/pimpl.h>
#include <tools/set.h>
#include <tools/version.h>

#include <QHash>
#include <QStringList>
#include <QVariant>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

namespace qbs {
class SetupProjectParameters;
namespace Internal {
class Evaluator;
class ItemPool;
class ItemReader;
class Logger;
class ProductContext;
class ProgressObserver;
class ProjectContext;
class ScriptEngine;

using ModulePropertiesPerGroup = std::unordered_map<const Item *, QualifiedIdSet>;
using FileLocations = QHash<std::pair<QString, QString>, CodeLocation>;

enum class FallbackMode { Enabled, Disabled };
enum class Deferral { Allowed, NotAllowed };
enum class ProductDependency { None, Single, Bulk };

class CancelException { };

template<typename DataType, typename MutexType = std::shared_mutex>
struct GuardedData {
    DataType data;
    mutable MutexType mutex;
};

class TimingData
{
public:
    TimingData &operator+=(const TimingData &other);

    qint64 dependenciesResolving = 0;
    qint64 moduleProviders = 0;
    qint64 moduleInstantiation = 0;
    qint64 propertyMerging = 0;
    qint64 groupsSetup = 0;
    qint64 groupsResolving = 0;
    qint64 preparingProducts = 0;
    qint64 resolvingProducts = 0;
    qint64 schedulingProducts = 0;
    qint64 probes = 0;
    qint64 propertyEvaluation = 0;
    qint64 propertyChecking = 0;
};

class ItemReaderCache
{
public:
    class AstCacheEntry
    {
    public:
        QString code;
        QbsQmlJS::Engine engine;
        QbsQmlJS::AST::UiProgram *ast = nullptr;

        bool addProcessingThread();
        void removeProcessingThread();

    private:
        GuardedData<Set<std::thread::id>, std::recursive_mutex> m_processingThreads;
    };

    const Set<QString> &filesRead() const { return m_filesRead; }
    AstCacheEntry &retrieveOrSetupCacheEntry(const QString &filePath,
                                             const std::function<void(AstCacheEntry &)> &setup);
    const QStringList &retrieveOrSetDirectoryEntries(
        const QString &dir, const std::function<QStringList()> &findOnDisk);

private:
    Set<QString> m_filesRead;
    GuardedData<std::unordered_map<QString, std::optional<QStringList>>, std::mutex> m_directoryEntries; // TODO: Merge with module dir entries cache?
    GuardedData<std::unordered_map<QString, AstCacheEntry>, std::mutex> m_astCache;
};

class DependenciesContext
{
public:
    virtual ~DependenciesContext();
    virtual std::pair<ProductDependency, ProductContext *> pendingDependency() const = 0;

    bool dependenciesResolved = false;
};

class ProductContext
{
public:
    QString uniqueName() const;
    QString displayName() const;
    void handleError(const ErrorInfo &error);
    bool dependenciesResolvingPending() const;
    std::pair<ProductDependency, ProductContext *> pendingDependency() const;

    QString name;
    QString buildDirectory;
    Item *item = nullptr;
    Item *scope = nullptr;
    ProjectContext *project = nullptr;
    std::unique_ptr<ProductContext> shadowProduct;
    Item *mergedExportItem = nullptr;
    std::vector<ProbeConstPtr> probes;
    ModulePropertiesPerGroup modulePropertiesSetInGroups;
    ErrorInfo delayedError;
    QString profileName;
    QString multiplexConfigurationId;
    QVariantMap profileModuleProperties; // Tree-ified module properties from profile.
    QVariantMap moduleProperties;        // Tree-ified module properties from profile + overridden values.
    std::optional<QVariantMap> providerConfig;
    std::optional<QVariantMap> providerQbsModule;
    QVariantMap defaultParameters; // In Export item.
    QStringList searchPaths;
    ResolvedProductPtr product;
    TimingData timingData;
    std::unique_ptr<DependenciesContext> dependenciesContext;

    // This is needed because complex cyclic dependencies that involve Depends.productTypes
    // may only be detected after a product has already been fully resolved.
    std::vector<std::pair<FileTags, CodeLocation>> bulkDependencies;

    // The keys are module prototypes, the values specify whether the module's
    // condition is true for this product.
    std::unordered_map<Item *, bool> modulePrototypeEnabledInfo;

    int dependsItemCount = -1;
};

class TopLevelProjectContext
{
public:
    TopLevelProjectContext() = default;
    TopLevelProjectContext(const TopLevelProjectContext &) = delete;
    TopLevelProjectContext &operator=(const TopLevelProjectContext &) = delete;
    ~TopLevelProjectContext();

    bool checkItemCondition(Item *item, Evaluator &evaluator);
    QString sourceCodeForEvaluation(const JSSourceValueConstPtr &value);
    ScriptFunctionPtr scriptFunctionValue(Item *item, const QString &name);
    QString sourceCodeAsFunction(const JSSourceValueConstPtr &value,
                                 const PropertyDeclaration &decl);

    void setCanceled() { m_canceled = true; }
    void checkCancelation();
    bool isCanceled() const { return m_canceled; }

    int productCount() const { return m_productsByName.size(); }

    void addProductToHandle(const ProductContext &product) { m_productsToHandle.data << &product; }
    void removeProductToHandle(const ProductContext &product);
    bool isProductQueuedForHandling(const ProductContext &product) const;
    int productsToHandleCount() const { return m_productsToHandle.data.size(); }

    void addDisabledItem(Item *item);
    bool isDisabledItem(const Item *item) const;

    void setProgressObserver(ProgressObserver *observer);
    ProgressObserver *progressObserver() const;

    void addProject(ProjectContext *project) { m_projects.push_back(project); }
    const std::vector<ProjectContext *> &projects() const { return m_projects; }

    void addQueuedError(const ErrorInfo &error);
    const std::vector<ErrorInfo> &queuedErrors() const { return m_queuedErrors.data; }

    void setProfileConfigs(const QVariantMap &profileConfigs) { m_profileConfigs = profileConfigs; }
    void addProfileConfig(const QString &profileName, const QVariantMap &profileConfig);
    const QVariantMap &profileConfigs() const { return m_profileConfigs; }
    std::optional<QVariantMap> profileConfig(const QString &profileName) const;

    void addProduct(ProductContext &product);
    void addProductByType(ProductContext &product, const FileTags &tags);
    ProductContext *productWithNameAndConstraint(
            const QString &name, const std::function<bool(ProductContext &)> &constraint);
    std::vector<ProductContext *> productsWithNameAndConstraint(
            const QString &name, const std::function<bool(ProductContext &)> &constraint);
    std::vector<ProductContext *> productsWithTypeAndConstraint(
            const FileTags &tags, const std::function<bool(ProductContext &)> &constraint);
    std::vector<std::pair<ProductContext *, CodeLocation>>
    finishedProductsWithBulkDependency(const FileTag &tag) const;
    void registerBulkDependencies(ProductContext &product);

    void addProjectNameUsedInOverrides(const QString &name);
    const Set<QString> &projectNamesUsedInOverrides() const;

    void addProductNameUsedInOverrides(const QString &name);
    const Set<QString> &productNamesUsedInOverrides() const;

    void setBuildDirectory(const QString &buildDir) { m_buildDirectory = buildDir; }
    const QString &buildDirectory() const { return m_buildDirectory; }

    void addMultiplexConfiguration(const QString &id, const QVariantMap &config);
    QVariantMap multiplexConfiguration(const QString &id) const;

    void setLastResolveTime(const FileTime &time) { m_lastResolveTime = time; }
    const FileTime &lastResolveTime() const { return m_lastResolveTime; }

    Set<QString> buildSystemFiles() const { return m_itemReaderCache.filesRead(); }

    std::lock_guard<std::mutex> moduleProvidersCacheLock();
    void setModuleProvidersCache(const ModuleProvidersCache &cache);
    const ModuleProvidersCache &moduleProvidersCache() const { return m_moduleProvidersCache; }
    ModuleProviderInfo *moduleProvider(const ModuleProvidersCacheKey &key);
    ModuleProviderInfo &addModuleProvider(const ModuleProvidersCacheKey &key,
                                          const ModuleProviderInfo &provider);

    void addParameterDeclarations(const Item *moduleProto,
                                  const Item::PropertyDeclarationMap &decls);
    Item::PropertyDeclarationMap parameterDeclarations(Item *moduleProto) const;

    void setParameters(const Item *moduleProto, const QVariantMap &parameters);
    QVariantMap parameters(Item *moduleProto) const;

    // An empty string means no matching module directory was found.
    QString findModuleDirectory(const QualifiedId &module, const QString &searchPath,
                                const std::function<QString()> &findOnDisk);

    QStringList getModuleFilesForDirectory(const QString &dir,
                                           const std::function<QStringList()> &findOnDisk);
    void removeModuleFileFromDirectoryCache(const QString &filePath);

    void addUnknownProfilePropertyError(const Item *moduleProto, const ErrorInfo &error);
    const std::vector<ErrorInfo> &unknownProfilePropertyErrors(const Item *moduleProto) const;

    Item *getModulePrototype(const QString &filePath, const QString &profile,
                             const std::function<Item *()> &produce);

    void addLocalProfile(const QString &name, const QVariantMap &values,
                         const CodeLocation &location);
    const QVariantMap localProfiles() { return m_localProfiles; }
    void checkForLocalProfileAsTopLevelProfile(const QString &topLevelProfile);

    using ProbeFilter = std::function<bool(const ProbeConstPtr &)>;
    std::lock_guard<std::mutex> probesCacheLock();
    void setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes);
    void setOldProductProbes(const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes);
    void addNewlyResolvedProbe(const ProbeConstPtr &probe);
    void addProjectLevelProbe(const ProbeConstPtr &probe);
    const std::vector<ProbeConstPtr> projectLevelProbes() const;
    ProbeConstPtr findOldProjectProbe(const QString &id, const ProbeFilter &filter) const;
    ProbeConstPtr findOldProductProbe(const QString &productName, const ProbeFilter &filter) const;
    ProbeConstPtr findCurrentProbe(const CodeLocation &location, const ProbeFilter &filter) const;
    void incrementProbesCount() { ++m_probesInfo.probesEncountered; }
    void incrementReusedCurrentProbesCount() { ++m_probesInfo.probesCachedCurrent; }
    void incrementReusedOldProbesCount() { ++m_probesInfo.probesCachedOld; }
    void incrementRunProbesCount() { ++m_probesInfo.probesRun; }
    int probesEncounteredCount() const { return m_probesInfo.probesEncountered; }
    int probesRunCount() const { return m_probesInfo.probesRun; }
    int reusedOldProbesCount() const { return m_probesInfo.probesCachedOld; }
    int reusedCurrentProbesCount() const { return m_probesInfo.probesCachedCurrent; }

    TimingData &timingData() { return m_timingData; }
    ItemReaderCache &itemReaderCache() { return m_itemReaderCache; }

    void incProductDeferrals() { ++m_productDeferrals; }
    int productDeferrals() const { return m_productDeferrals; }

    void collectDataFromEngine(const ScriptEngine &engine);

    ItemPool &createItemPool();

private:
    const ResolvedFileContextPtr &resolvedFileContext(const FileContextConstPtr &ctx);

    std::vector<ProjectContext *> m_projects;
    GuardedData<Set<const ProductContext *>> m_productsToHandle;
    std::multimap<QString, ProductContext *> m_productsByName;
    GuardedData<std::unordered_map<QStringView, QString>, std::mutex> m_sourceCode;
    std::unordered_map<QString, QVariantMap> m_multiplexConfigsById;
    GuardedData<QHash<CodeLocation, ScriptFunctionPtr>, std::mutex> m_scriptFunctionMap;
    GuardedData<std::unordered_map<std::pair<QStringView, QStringList>, QString>,
                std::mutex> m_scriptFunctions;
    std::unordered_map<FileContextConstPtr, ResolvedFileContextPtr> m_fileContextMap;
    Set<QString> m_projectNamesUsedInOverrides;
    Set<QString> m_productNamesUsedInOverrides;
    GuardedData<Set<const Item *>> m_disabledItems;
    GuardedData<std::vector<ErrorInfo>, std::mutex> m_queuedErrors;
    QString m_buildDirectory;
    QVariantMap m_profileConfigs;
    ProgressObserver *m_progressObserver = nullptr;
    TimingData m_timingData;
    ModuleProvidersCache m_moduleProvidersCache;
    std::mutex m_moduleProvidersCacheMutex;
    QVariantMap m_localProfiles;
    ItemReaderCache m_itemReaderCache;
    QHash<FileTag, std::vector<std::pair<ProductContext *, CodeLocation>>> m_reverseBulkDependencies;

    // For fast look-up when resolving Depends.productTypes.
    // The contract is that it contains fully handled, error-free, enabled products.
    GuardedData<std::multimap<FileTag, ProductContext *>> m_productsByType;

    // The keys are module prototypes.
    GuardedData<std::unordered_map<const Item *,
                                   Item::PropertyDeclarationMap>> m_parameterDeclarations;
    GuardedData<std::unordered_map<const Item *, QVariantMap>> m_parameters;
    GuardedData<std::unordered_map<const Item *,
                                   std::vector<ErrorInfo>>> m_unknownProfilePropertyErrors;

    // The keys are search path + module name, the values are directories.
    GuardedData<QHash<std::pair<QString, QualifiedId>, std::optional<QString>>,
                std::mutex> m_modulePathCache;

    // The keys are file paths, the values are module prototype items accompanied by a profile.
    GuardedData<std::unordered_map<QString, std::vector<std::pair<Item *, QString>>>,
                std::mutex> m_modulePrototypes;

    GuardedData<std::map<QString, std::optional<QStringList>>,
                std::mutex> m_moduleFilesPerDirectory;

    struct {
        QHash<QString, std::vector<ProbeConstPtr>> oldProjectProbes;
        QHash<QString, std::vector<ProbeConstPtr>> oldProductProbes;
        QHash<CodeLocation, std::vector<ProbeConstPtr>> currentProbes;
        std::vector<ProbeConstPtr> projectLevelProbes;

        quint64 probesEncountered = 0;
        quint64 probesRun = 0;
        quint64 probesCachedCurrent = 0;
        quint64 probesCachedOld = 0;
    } m_probesInfo;
    std::mutex m_probesMutex;

    std::vector<std::unique_ptr<ItemPool>> m_itemPools;

    FileTime m_lastResolveTime;

    std::atomic_bool m_canceled = false;
    int m_productDeferrals = 0;
};

class ProjectContext
{
public:
    QString name;
    Item *item = nullptr;
    Item *scope = nullptr;
    TopLevelProjectContext *topLevelProject = nullptr;
    ProjectContext *parent = nullptr;
    std::vector<ProjectContext *> children;
    std::vector<ProductContext> products;
    std::vector<QStringList> searchPathsStack;
    ResolvedProjectPtr project;
    std::vector<FileTaggerConstPtr> fileTaggers;
    std::vector<RulePtr> rules;
    JobLimits jobLimits;
    ResolvedModulePtr dummyModule;
};

class ModuleContext
{
public:
    ResolvedModulePtr module;
    JobLimits jobLimits;
};

class LoaderState
{
public:
    LoaderState(const SetupProjectParameters &parameters, TopLevelProjectContext &topLevelProject,
                ItemPool &itemPool, ScriptEngine &engine, Logger logger);
    ~LoaderState();

    Evaluator &evaluator();
    ItemPool &itemPool();
    ItemReader &itemReader();
    Logger &logger();
    const SetupProjectParameters &parameters() const;
    TopLevelProjectContext &topLevelProject();

private:
    class Private;
    Pimpl<Private> d;
};

// List must be sorted by priority in ascending order.
[[nodiscard]] QVariantMap mergeDependencyParameters(
        std::vector<Item::Module::ParametersWithPriority> &&candidates);
[[nodiscard]] QVariantMap mergeDependencyParameters(const QVariantMap &m1, const QVariantMap &m2);

QString fullProductDisplayName(const QString &name, const QString &multiplexId);
void adjustParametersScopes(Item *item, Item *scope);
void resolveRule(LoaderState &state, Item *item, ProjectContext *projectContext,
                 ProductContext *productContext, ModuleContext *moduleContext);
void resolveJobLimit(LoaderState &state, Item *item, ProjectContext *projectContext,
                     ProductContext *productContext, ModuleContext *moduleContext);
void resolveFileTagger(LoaderState &state, Item *item, ProjectContext *projectContext,
                       ProductContext *productContext);
const FileTag unknownFileTag();

} // namespace Internal
} // namespace qbs
