/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

#include "moduleproviderloader.h"

#include "itemreader.h"
#include "probesresolver.h"

#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/item.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/scripttools.h>
#include <tools/setupprojectparameters.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qcryptographichash.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

namespace qbs {
namespace Internal {

static QString getConfigHash(const QVariantMap& config)
{
    QJsonDocument doc;
    doc.setObject(QJsonObject::fromVariantMap(config));
    return QString::fromLatin1(
        QCryptographicHash::hash(doc.toJson(), QCryptographicHash::Sha1).toHex().left(16));
}

ModuleProviderLoader::ModuleProviderLoader(LoaderState &loaderState)
    : m_loaderState(loaderState) {}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::executeModuleProviders(
        ProductContext &productContext,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName,
        FallbackMode fallbackMode)
{
    ModuleProviderLoader::ModuleProviderResult result;
    try {
        std::vector<Provider> providersToRun;
        qCDebug(lcModuleLoader) << "Module" << moduleName.toString()
                                << "not found, checking for module providers";
        const auto providerNames = getModuleProviders(productContext.item);
        if (providerNames) {
            providersToRun = transformed<std::vector<Provider>>(*providerNames, [](const auto &name) {
                return Provider{name, ModuleProviderLookup::Named}; });
        } else {
            for (QualifiedId providerName = moduleName; !providerName.empty();
                providerName.pop_back()) {
                    providersToRun.push_back({providerName, ModuleProviderLookup::Scoped});
            }
        }
        result = executeModuleProvidersHelper(
            productContext, dependsItemLocation, moduleName, providersToRun);

        if (fallbackMode == FallbackMode::Enabled
                && !result.providerFound
                && !providerNames) {
                qCDebug(lcModuleLoader) << "Specific module provider not found for"
                                    << moduleName.toString()  << ", setting up fallback.";
            result = executeModuleProvidersHelper(
                    productContext,
                    dependsItemLocation,
                    moduleName,
                    {{moduleName, ModuleProviderLookup::Fallback}});
        }
    } catch (const ErrorInfo &error) {
        auto ei = error;
        ei.prepend(
            Tr::tr("Error executing provider for module '%1':").arg(moduleName.toString()),
            dependsItemLocation);
        productContext.handleError(ei);
    }
    return result;
}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::executeModuleProvidersHelper(
        ProductContext &product,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName,
        const std::vector<Provider> &providers)
{
    if (providers.empty())
        return {};
    QStringList allSearchPaths;
    ModuleProviderResult result;
    setupModuleProviderConfig(product);
    const auto qbsModule = evaluateQbsModule(product);
    for (const auto &[name, lookupType] : providers) {
        const auto &[info, fromCache] = findOrCreateProviderInfo(
                    product, dependsItemLocation, moduleName, name, lookupType, qbsModule);
        if (info.providerFile.isEmpty()) {
            if (lookupType == ModuleProviderLookup::Named)
                throw ErrorInfo(Tr::tr("Unknown provider '%1'").arg(name.toString()));
            continue;
        }
        if (fromCache)
            qCDebug(lcModuleLoader) << "Re-using provider" << name.toString() << "from cache";

        result.providerFound = true;
        if (info.searchPaths.empty()) {
            qCDebug(lcModuleLoader)
                    << "Module provider did run, but did not set up any modules.";
            continue;
        }
        qCDebug(lcModuleLoader) << "Module provider added" << info.searchPaths.size()
                                << "new search path(s)";

        allSearchPaths << info.searchPaths;
    }
    if (allSearchPaths.isEmpty())
        return result;

    result.searchPaths = std::move(allSearchPaths);

    return result;
}

std::pair<const ModuleProviderInfo &, bool /*fromCache*/>
ModuleProviderLoader::findOrCreateProviderInfo(
        ProductContext &product, const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName, const QualifiedId &name, ModuleProviderLookup lookupType,
        const QVariantMap &qbsModule)
{
    const QVariantMap config = product.providerConfig->value(name.toString()).toMap();
    std::lock_guard lock(m_loaderState.topLevelProject().moduleProvidersCacheLock());
    ModuleProvidersCacheKey cacheKey{name.toString(), {}, config, qbsModule, int(lookupType)};
    // TODO: get rid of non-eager providers and eliminate following if-logic
    // first, try to find eager provider (stored with an empty module name)
    if (ModuleProviderInfo *provider = m_loaderState.topLevelProject().moduleProvider(cacheKey))
        return {*provider, true};
    // second, try to find non-eager provider for a specific module name
    std::get<1>(cacheKey) = moduleName.toString(); // override moduleName
    if (ModuleProviderInfo *provider = m_loaderState.topLevelProject().moduleProvider(cacheKey))
        return {*provider, true};
    bool isEager = false;
    ModuleProviderInfo info;
    info.name = name;
    info.config = config;
    info.providerFile = findModuleProviderFile(name, lookupType);
    if (!info.providerFile.isEmpty()) {
        qCDebug(lcModuleLoader) << "Running provider" << name << "at" << info.providerFile;
        std::tie(info.searchPaths, isEager) = evaluateModuleProvider(
                    product,
                    dependsItemLocation,
                    moduleName,
                    name,
                    info.providerFile,
                    config,
                    qbsModule);
        info.transientOutput = m_loaderState.parameters().dryRun();
    }
    std::get<1>(cacheKey) = isEager ? QString() : moduleName.toString();
    return {m_loaderState.topLevelProject().addModuleProvider(cacheKey, info), false};
}

void ModuleProviderLoader::setupModuleProviderConfig(ProductContext &product)
{
    if (product.providerConfig)
        return;
    QVariantMap providerConfig;
    const ItemValueConstPtr configItemValue =
        product.item->itemProperty(StringConstants::moduleProviders(), m_loaderState.itemPool());
    if (configItemValue) {
        const std::function<void(const Item *, QualifiedId)> collectMap
                = [this, &providerConfig, &collectMap](const Item *item, const QualifiedId &name) {
            const Item::PropertyMap &props = item->properties();
            for (auto it = props.begin(); it != props.end(); ++it) {
                QVariant value;
                switch (it.value()->type()) {
                case Value::ItemValueType: {
                    const auto childItem = static_cast<ItemValue *>(it.value().get())->item();
                    childItem->setScope(item->scope());
                    collectMap(childItem, QualifiedId(name) << it.key());
                    continue;
                }
                case Value::JSSourceValueType: {
                    it.value()->setScope(item->scope(), {});
                    const ScopedJsValue sv(m_loaderState.evaluator().engine()->context(),
                                           m_loaderState.evaluator().value(item, it.key()));
                    value = getJsVariant(m_loaderState.evaluator().engine()->context(), sv);
                    break;
                }
                case Value::VariantValueType:
                    value = static_cast<VariantValue *>(it.value().get())->value();
                    break;
                }
                QVariantMap m = providerConfig.value(name.toString()).toMap();
                m.insert(it.key(), value);
                providerConfig.insert(name.toString(), m);
            }
        };
        configItemValue->item()->setScope(product.item);
        collectMap(configItemValue->item(), QualifiedId());
    }
    for (auto it = product.moduleProperties.begin(); it != product.moduleProperties.end(); ++it) {
        if (!it.key().startsWith(QStringLiteral("moduleProviders.")))
            continue;
        const QString provider = it.key().mid(QStringLiteral("moduleProviders.").size());
        const QVariantMap providerConfigFromBuildConfig = it.value().toMap();
        if (providerConfigFromBuildConfig.empty())
            continue;
        QVariantMap currentMapForProvider = providerConfig.value(provider).toMap();
        for (auto propIt = providerConfigFromBuildConfig.begin();
             propIt != providerConfigFromBuildConfig.end(); ++propIt) {
            currentMapForProvider.insert(propIt.key(), propIt.value());
        }
        providerConfig.insert(provider, currentMapForProvider);
    }
    product.providerConfig = providerConfig;
}

std::optional<std::vector<QualifiedId>> ModuleProviderLoader::getModuleProviders(Item *item)
{
    while (item) {
        const auto providers = m_loaderState.evaluator().optionalStringListValue(
                    item, StringConstants::qbsModuleProviders());
        if (providers) {
            return transformed<std::vector<QualifiedId>>(*providers, [](const auto &provider) {
                return QualifiedId::fromString(provider); });
        }
        item = item->parent();
    }
    return std::nullopt;
}

QString ModuleProviderLoader::findModuleProviderFile(
        const QualifiedId &name, ModuleProviderLookup lookupType)
{
    for (const QString &path : m_loaderState.itemReader().allSearchPaths()) {
        QString fullPath = FileInfo::resolvePath(path, QStringLiteral("module-providers"));
        switch (lookupType) {
        case ModuleProviderLookup::Named: {
            const auto result =
                    FileInfo::resolvePath(fullPath, name.toString() + QStringLiteral(".qbs"));
            if (FileInfo::exists(result)) {
                fullPath = result;
                break;
            }
            [[fallthrough]];
        }
        case ModuleProviderLookup::Scoped:
            for (const QString &component : name)
                fullPath = FileInfo::resolvePath(fullPath, component);
            fullPath = FileInfo::resolvePath(fullPath, QStringLiteral("provider.qbs"));
            break;
        case ModuleProviderLookup::Fallback:
            fullPath = FileInfo::resolvePath(fullPath, QStringLiteral("__fallback/provider.qbs"));
            break;
        }
        if (!FileInfo::exists(fullPath)) {
            qCDebug(lcModuleLoader) << "No module provider found at" << fullPath;
            continue;
        }
        return fullPath;
    }
    return {};
}

QVariantMap ModuleProviderLoader::evaluateQbsModule(ProductContext &product) const
{
    if (product.providerQbsModule)
        return *product.providerQbsModule;
    const QString properties[] = {
        QStringLiteral("sysroot"),
        QStringLiteral("toolchain"),
    };
    const auto qbsItemValue = std::static_pointer_cast<ItemValue>(
        product.item->property(StringConstants::qbsModule()));
    QVariantMap result;
    for (const auto &property : properties) {
        const ScopedJsValue val(m_loaderState.evaluator().engine()->context(),
                                m_loaderState.evaluator().value(qbsItemValue->item(), property));
        auto value = getJsVariant(m_loaderState.evaluator().engine()->context(), val);
        if (!value.isValid())
            continue;

        // The xcode module sets qbs.sysroot; the resulting value is bogus before the probes
        // have run.
        if (property == QLatin1String("sysroot") && !FileInfo::isAbsolute(value.toString()))
            continue;

        result[property] = std::move(value);
    }
    return *(product.providerQbsModule = result);
}

Item *ModuleProviderLoader::createProviderScope(
    const ProductContext &product, const QVariantMap &qbsModule)
{
    const auto qbsItemValue = std::static_pointer_cast<ItemValue>(
        product.item->property(StringConstants::qbsModule()));

    Item *fakeQbsModule = Item::create(&m_loaderState.itemPool(), ItemType::Scope);

    for (auto it = qbsModule.begin(), end = qbsModule.end(); it != end; ++it) {
        fakeQbsModule->setProperty(it.key(), VariantValue::create(it.value()));
    }

    Item *scope = Item::create(&m_loaderState.itemPool(), ItemType::Scope);
    scope->setFile(qbsItemValue->item()->file());
    scope->setProperty(StringConstants::qbsModule(), ItemValue::create(fakeQbsModule));
    return scope;
}

ModuleProviderLoader::EvaluationResult ModuleProviderLoader::evaluateModuleProvider(
        ProductContext &product,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName,
        const QualifiedId &name,
        const QString &providerFile,
        const QVariantMap &moduleConfig,
        const QVariantMap &qbsModule)
{
    qCDebug(lcModuleLoader) << "Instantiating module provider at" << providerFile;
    const QString projectBuildDir = product.project->item->variantProperty(
                StringConstants::buildDirectoryProperty())->value().toString();
    const QString searchPathBaseDir = ModuleProviderInfo::outputDirPath(projectBuildDir, name);

    // include qbs module into hash
    auto jsConfig = moduleConfig;
    jsConfig[StringConstants::qbsModule()] = qbsModule;

    QString outputBaseDir = searchPathBaseDir + QLatin1Char('/') + getConfigHash(jsConfig);
    Item * const providerItem = m_loaderState.itemReader().setupItemFromFile(
                providerFile, dependsItemLocation);
    if (providerItem->type() != ItemType::ModuleProvider) {
        throw ErrorInfo(Tr::tr("File '%1' declares an item of type '%2', "
                               "but '%3' was expected.")
            .arg(providerFile, providerItem->typeName(),
                 BuiltinDeclarations::instance().nameForType(ItemType::ModuleProvider)));
    }

    providerItem->setScope(createProviderScope(product, qbsModule));
    providerItem->setProperty(
        StringConstants::nameProperty(),
        VariantValue::create(name.toString()));
    providerItem->setProperty(
        QStringLiteral("outputBaseDir"),
        VariantValue::create(outputBaseDir));
    providerItem->overrideProperties(moduleConfig, name,
                                     m_loaderState.parameters(), m_loaderState.logger());

    const bool isEager = m_loaderState.evaluator().boolValue(
        providerItem, StringConstants::isEagerProperty());
    if (!isEager) {
        providerItem->setProperty(
            StringConstants::moduleNameProperty(),
            VariantValue::create(moduleName.toString()));
    }

    ProbesResolver(m_loaderState).resolveProbes(product, providerItem);

    EvalContextSwitcher contextSwitcher(m_loaderState.evaluator().engine(),
                                        EvalContext::ModuleProvider);
    auto searchPaths = m_loaderState.evaluator().stringListValue(
        providerItem, QStringLiteral("relativeSearchPaths"));
    auto prependBaseDir = [&outputBaseDir](const auto &path) {
        return outputBaseDir + QLatin1Char('/') + path;
    };
    std::transform(searchPaths.begin(), searchPaths.end(), searchPaths.begin(), prependBaseDir);
    return {searchPaths, isEager};
}

} // namespace Internal
} // namespace qbs
