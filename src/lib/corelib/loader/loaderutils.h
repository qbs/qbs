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
#include <language/propertydeclaration.h>
#include <language/qualifiedid.h>
#include <tools/joblimits.h>
#include <tools/pimpl.h>
#include <tools/set.h>
#include <tools/version.h>

#include <QStringList>
#include <QVariant>

#include <memory>
#include <optional>
#include <vector>

namespace qbs {
class SetupProjectParameters;
namespace Internal {
class DependenciesResolver;
class Evaluator;
class Item;
class ItemPool;
class ItemReader;
class LocalProfiles;
class Logger;
class ModuleInstantiator;
class ModulePropertyMerger;
class ProbesResolver;
class ProductContext;
class ProductItemMultiplexer;
class ProgressObserver;
class ProjectContext;

using ModulePropertiesPerGroup = std::unordered_map<const Item *, QualifiedIdSet>;
using FileLocations = QHash<std::pair<QString, QString>, CodeLocation>;

enum class FallbackMode { Enabled, Disabled };
enum class Deferral { Allowed, NotAllowed };

class CancelException { };

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
    qint64 probes = 0;
    qint64 propertyEvaluation = 0;
    qint64 propertyChecking = 0;
};

class DependenciesContext
{
public:
    virtual ~DependenciesContext();

    bool dependenciesResolved = false;
};

class ProductContext
{
public:
    QString uniqueName() const;
    QString displayName() const;
    void handleError(const ErrorInfo &error);
    bool dependenciesResolvingPending() const;

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
    QVariantMap defaultParameters; // In Export item.
    QStringList searchPaths;
    ResolvedProductPtr product;
    using ArtifactPropertiesInfo = std::pair<ArtifactPropertiesPtr, std::vector<CodeLocation>>;
    QHash<QStringList, ArtifactPropertiesInfo> artifactPropertiesPerFilter;
    FileLocations sourceArtifactLocations;
    GroupConstPtr currentGroup;
    TimingData timingData;
    std::unique_ptr<DependenciesContext> dependenciesContext;

    // The keys are module prototypes, the values specify whether the module's
    // condition is true for this product.
    std::unordered_map<Item *, bool> modulePrototypeEnabledInfo;
};

class TopLevelProjectContext
{
public:
    TopLevelProjectContext() = default;
    TopLevelProjectContext(const TopLevelProjectContext &) = delete;
    TopLevelProjectContext &operator=(const TopLevelProjectContext &) = delete;
    ~TopLevelProjectContext() { qDeleteAll(projects); }

    bool checkItemCondition(Item *item, Evaluator &evaluator);
    void checkCancelation();
    QString sourceCodeForEvaluation(const JSSourceValueConstPtr &value);
    ScriptFunctionPtr scriptFunctionValue(Item *item, const QString &name);
    QString sourceCodeAsFunction(const JSSourceValueConstPtr &value,
                                 const PropertyDeclaration &decl);
    const ResolvedFileContextPtr &resolvedFileContext(const FileContextConstPtr &ctx);

    std::vector<ProjectContext *> projects;
    std::list<std::pair<ProductContext *, int>> productsToHandle;
    std::multimap<QString, ProductContext *> productsByName;
    std::unordered_map<Item *, ProductContext *> productsByItem;
    std::unordered_map<QStringView, QString> sourceCode;
    QHash<CodeLocation, ScriptFunctionPtr> scriptFunctionMap;
    std::unordered_map<std::pair<QStringView, QStringList>, QString> scriptFunctions;
    std::unordered_map<FileContextConstPtr, ResolvedFileContextPtr> fileContextMap;
    Set<QString> projectNamesUsedInOverrides;
    Set<QString> productNamesUsedInOverrides;
    Set<Item *> disabledItems;
    Set<QString> erroneousProducts;
    std::vector<ProbeConstPtr> probes;
    std::vector<ErrorInfo> queuedErrors;
    QString buildDirectory;
    QVariantMap profileConfigs;
    ProgressObserver *progressObserver = nullptr;
    TimingData timingData;

    // For fast look-up when resolving Depends.productTypes.
    // The contract is that it contains fully handled, error-free, enabled products.
    std::multimap<FileTag, ProductContext *> productsByType;

    bool canceled = false;
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
    LoaderState(const SetupProjectParameters &parameters, ItemPool &itemPool, Evaluator &evaluator,
                Logger &logger);
    ~LoaderState();

    DependenciesResolver &dependenciesResolver();
    Evaluator &evaluator();
    ItemPool &itemPool();
    ItemReader &itemReader();
    LocalProfiles &localProfiles();
    Logger &logger();
    ModuleInstantiator &moduleInstantiator();
    ProductItemMultiplexer &multiplexer();
    const SetupProjectParameters &parameters() const;
    ProbesResolver &probesResolver();
    ModulePropertyMerger &propertyMerger();
    TopLevelProjectContext &topLevelProject();

private:
    class Private;
    Pimpl<Private> d;
};

void mergeParameters(QVariantMap &dst, const QVariantMap &src);
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
