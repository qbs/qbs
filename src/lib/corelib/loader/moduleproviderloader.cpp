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

#include <QtCore/qtemporaryfile.h>

namespace qbs {
namespace Internal {

ModuleProviderLoader::ModuleProviderLoader(LoaderState &loaderState)
    : m_loaderState(loaderState) {}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::executeModuleProviders(
        const ProductContext &productContext,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName,
        FallbackMode fallbackMode)
{
    ModuleProviderLoader::ModuleProviderResult result;
    std::vector<Provider> providersToRun;
    qCDebug(lcModuleLoader) << "Module" << moduleName.toString()
                            << "not found, checking for module providers";
    const auto providerNames = getModuleProviders(productContext.productItem);
    if (providerNames) {
        providersToRun = transformed<std::vector<Provider>>(*providerNames, [](const auto &name) {
            return Provider{name, ModuleProviderLookup::Named}; });
    } else {
        for (QualifiedId providerName = moduleName; !providerName.empty();
            providerName.pop_back()) {
                providersToRun.push_back({providerName, ModuleProviderLookup::Scoped});
        }
    }
    result = executeModuleProvidersHelper(productContext, dependsItemLocation, providersToRun);

    if (fallbackMode == FallbackMode::Enabled
            && !result.providerFound
            && !providerNames) {
            qCDebug(lcModuleLoader) << "Specific module provider not found for"
                                << moduleName.toString()  << ", setting up fallback.";
        result = executeModuleProvidersHelper(
                productContext,
                dependsItemLocation,
                {{moduleName, ModuleProviderLookup::Fallback}});
    }

    return result;
}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::executeModuleProvidersHelper(
        const ProductContext &product,
        const CodeLocation &dependsItemLocation,
        const std::vector<Provider> &providers)
{
    if (providers.empty())
        return {};
    QStringList allSearchPaths;
    ModuleProviderResult result;
    result.providerConfig = product.providerConfig ? *product.providerConfig
                                                   : getModuleProviderConfig(product);
    const auto qbsModule = evaluateQbsModule(product);
    for (const auto &[name, lookupType] : providers) {
        const QVariantMap config = result.providerConfig.value(name.toString()).toMap();
        ModuleProviderInfo &info = m_storedModuleProviderInfo.providers[
            {name.toString(), config, qbsModule, int(lookupType)}];
        const bool fromCache = !info.name.isEmpty();
        if (!fromCache) {
            info.name = name;
            info.config = config;
            info.providerFile = findModuleProviderFile(name, lookupType);
            if (!info.providerFile.isEmpty()) {
                qCDebug(lcModuleLoader) << "Running provider" << name << "at" << info.providerFile;
                const auto evalResult = evaluateModuleProvider(
                    product, dependsItemLocation, name, info.providerFile, config, qbsModule);
                info.searchPaths = evalResult.first;
                result.probes << evalResult.second;
                info.transientOutput = m_loaderState.parameters().dryRun();
            }
        }
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

QVariantMap ModuleProviderLoader::getModuleProviderConfig(const ProductContext &product)
{
    QVariantMap providerConfig;
    const ItemValueConstPtr configItemValue =
        product.productItem->itemProperty(StringConstants::moduleProviders());
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
        configItemValue->item()->setScope(product.productItem);
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
    return providerConfig;
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

QVariantMap ModuleProviderLoader::evaluateQbsModule(const ProductContext &product) const
{
    const QString properties[] = {
        QStringLiteral("sysroot"),
        QStringLiteral("toolchain"),
    };
    const auto qbsItemValue = std::static_pointer_cast<ItemValue>(
        product.productItem->property(StringConstants::qbsModule()));
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
    return result;
}

Item *ModuleProviderLoader::createProviderScope(const ProductContext &product, const QVariantMap &qbsModule)
{
    const auto qbsItemValue = std::static_pointer_cast<ItemValue>(
        product.productItem->property(StringConstants::qbsModule()));

    Item *fakeQbsModule = Item::create(product.productItem->pool(), ItemType::Scope);

    for (auto it = qbsModule.begin(), end = qbsModule.end(); it != end; ++it) {
        fakeQbsModule->setProperty(it.key(), VariantValue::create(it.value()));
    }

    Item *scope = Item::create(product.productItem->pool(), ItemType::Scope);
    scope->setFile(qbsItemValue->item()->file());
    scope->setProperty(StringConstants::qbsModule(), ItemValue::create(fakeQbsModule));
    return scope;
}

std::pair<QStringList, std::vector<ProbeConstPtr> > ModuleProviderLoader::evaluateModuleProvider(const ProductContext &product,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &name,
        const QString &providerFile,
        const QVariantMap &moduleConfig,
        const QVariantMap &qbsModule)
{
    QTemporaryFile dummyItemFile;
    if (!dummyItemFile.open()) {
        throw ErrorInfo(Tr::tr("Failed to create temporary file for running module provider "
                               "for dependency '%1': %2").arg(name.toString(),
                                                              dummyItemFile.errorString()));
    }
    m_tempQbsFiles << dummyItemFile.fileName();
    qCDebug(lcModuleLoader) << "Instantiating module provider at" << providerFile;
    const QString projectBuildDir = product.projectItem->variantProperty(
                StringConstants::buildDirectoryProperty())->value().toString();
    const QString searchPathBaseDir = ModuleProviderInfo::outputDirPath(projectBuildDir, name);

    // include qbs module into hash
    auto jsConfig = moduleConfig;
    jsConfig[StringConstants::qbsModule()] = qbsModule;

    QTextStream stream(&dummyItemFile);
    using Qt::endl;
    setupDefaultCodec(stream);
    stream << "import qbs.FileInfo" << endl;
    stream << "import qbs.Utilities" << endl;
    stream << "import '" << providerFile << "' as Provider" << endl;
    stream << "Provider {" << endl;
    stream << "    name: " << toJSLiteral(name.toString()) << endl;
    stream << "    property var config: (" << toJSLiteral(jsConfig) << ')' << endl;
    stream << "    outputBaseDir: FileInfo.joinPaths(baseDirPrefix, "
              "        Utilities.getHash(JSON.stringify(config)))" << endl;
    stream << "    property string baseDirPrefix: " << toJSLiteral(searchPathBaseDir) << endl;
    stream << "    property stringList searchPaths: (relativeSearchPaths || [])"
              "        .map(function(p) { return FileInfo.joinPaths(outputBaseDir, p); })"
           << endl;
    stream << "}" << endl;
    stream.flush();
    Item * const providerItem = m_loaderState.itemReader().setupItemFromFile(
                dummyItemFile.fileName(), dependsItemLocation);
    if (providerItem->type() != ItemType::ModuleProvider) {
        throw ErrorInfo(Tr::tr("File '%1' declares an item of type '%2', "
                               "but '%3' was expected.")
            .arg(providerFile, providerItem->typeName(),
                 BuiltinDeclarations::instance().nameForType(ItemType::ModuleProvider)));
    }

    providerItem->setScope(createProviderScope(product, qbsModule));
    providerItem->overrideProperties(moduleConfig, name, m_loaderState.parameters(),
                                     m_loaderState.logger());
    std::vector<ProbeConstPtr> probes = m_loaderState.probesResolver().resolveProbes(
        {product.name, product.uniqueName}, providerItem);

    EvalContextSwitcher contextSwitcher(m_loaderState.evaluator().engine(),
                                        EvalContext::ModuleProvider);
    return std::make_pair(m_loaderState.evaluator().stringListValue(
                              providerItem, QStringLiteral("searchPaths")),
                          std::move(probes));
}

} // namespace Internal
} // namespace qbs
