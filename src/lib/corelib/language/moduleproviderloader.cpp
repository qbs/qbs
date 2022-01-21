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

#include "builtindeclarations.h"
#include "evaluator.h"
#include "itemreader.h"
#include "moduleloader.h"
#include "probesresolver.h"

#include <language/scriptengine.h>
#include <language/value.h>

#include <logging/categories.h>
#include <logging/translator.h>

#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qtemporaryfile.h>

namespace qbs {
namespace Internal {

ModuleProviderLoader::ModuleProviderLoader(ItemReader *reader, Evaluator *evaluator,
                                           ProbesResolver *probesResolver, Logger &logger)
    : m_reader(reader)
    , m_evaluator(evaluator)
    , m_probesResolver(probesResolver)
    , m_logger(logger)
{
}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::executeModuleProvider(
        ProductContext &productContext,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName,
        FallbackMode fallbackMode)
{
    ModuleProviderLoader::ModuleProviderResult result;
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
    result = findModuleProvider(providersToRun, productContext, dependsItemLocation);

    if (fallbackMode == FallbackMode::Enabled
            && !result.providerFound
            && !providerNames) {
            qCDebug(lcModuleLoader) << "Specific module provider not found for"
                                << moduleName.toString()  << ", setting up fallback.";
        result = findModuleProvider(
                {{moduleName, ModuleProviderLookup::Fallback}},
                productContext,
                dependsItemLocation);
    }

    return result;
}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::findModuleProvider(
        const std::vector<Provider> &providers,
        ProductContext &product,
        const CodeLocation &dependsItemLocation)
{
    if (providers.empty())
        return {};
    QStringList allSearchPaths;
    ModuleProviderResult result;
    for (const auto &[name, lookupType] : providers) {
        const QVariantMap config = moduleProviderConfig(product).value(name.toString()).toMap();
        ModuleProviderInfo &info =
                m_storedModuleProviderInfo.providers[{name.toString(), config, int(lookupType)}];
        const bool fromCache = !info.name.isEmpty();
        if (!fromCache) {
            info.name = name;
            info.config = config;
            info.providerFile = findModuleProviderFile(name, lookupType);
            if (!info.providerFile.isEmpty()) {
                qCDebug(lcModuleLoader) << "Running provider" << name << "at" << info.providerFile;
                info.searchPaths = getProviderSearchPaths(
                        name, info.providerFile, product, config, dependsItemLocation);
                info.transientOutput = m_parameters.dryRun();
            }
        }
        if (info.providerFile.isEmpty()) {
            if (lookupType == ModuleProviderLookup::Named)
                throw ErrorInfo(Tr::tr("Unknown provider '%1'").arg(name.toString()));
            continue;
        }
        if (fromCache)
            qCDebug(lcModuleLoader) << "Re-using provider" << name << "from cache";

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

    m_reader->pushExtraSearchPaths(allSearchPaths);
    result.providerAddedSearchPaths = true;

    return result;
}

QVariantMap ModuleProviderLoader::moduleProviderConfig(
        ProductContext &product)
{
    if (product.theModuleProviderConfig)
        return *product.theModuleProviderConfig;
    QVariantMap providerConfig;
    const ItemValueConstPtr configItemValue =
            product.item->itemProperty(StringConstants::moduleProviders());
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
                case Value::JSSourceValueType:
                    value = m_evaluator->value(item, it.key()).toVariant();
                    break;
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
    return *(product.theModuleProviderConfig = std::move(providerConfig));
}

std::optional<std::vector<QualifiedId>> ModuleProviderLoader::getModuleProviders(Item *item)
{
    while (item) {
        const auto providers =
                m_evaluator->optionalStringListValue(item, StringConstants::qbsModuleProviders());
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
    for (const QString &path : m_reader->allSearchPaths()) {
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

QStringList ModuleProviderLoader::getProviderSearchPaths(
        const QualifiedId &name,
        const QString &providerFile,
        ProductContext &product,
        const QVariantMap &moduleConfig,
        const CodeLocation &location)
{
    QTemporaryFile dummyItemFile;
    if (!dummyItemFile.open()) {
        throw ErrorInfo(Tr::tr("Failed to create temporary file for running module provider "
                               "for dependency '%1': %2").arg(name.toString(),
                                                              dummyItemFile.errorString()));
    }
    m_tempQbsFiles << dummyItemFile.fileName();
    qCDebug(lcModuleLoader) << "Instantiating module provider at" << providerFile;
    const QString projectBuildDir = product.project->item->variantProperty(
                StringConstants::buildDirectoryProperty())->value().toString();
    const QString searchPathBaseDir = ModuleProviderInfo::outputDirPath(projectBuildDir, name);
    QTextStream stream(&dummyItemFile);
    using Qt::endl;
    setupDefaultCodec(stream);
    stream << "import qbs.FileInfo" << endl;
    stream << "import qbs.Utilities" << endl;
    stream << "import '" << providerFile << "' as Provider" << endl;
    stream << "Provider {" << endl;
    stream << "    name: " << toJSLiteral(name.toString()) << endl;
    stream << "    property var config: (" << toJSLiteral(moduleConfig) << ')' << endl;
    stream << "    outputBaseDir: FileInfo.joinPaths(baseDirPrefix, "
              "        Utilities.getHash(JSON.stringify(config)))" << endl;
    stream << "    property string baseDirPrefix: " << toJSLiteral(searchPathBaseDir) << endl;
    stream << "    property stringList searchPaths: (relativeSearchPaths || [])"
              "        .map(function(p) { return FileInfo.joinPaths(outputBaseDir, p); })"
           << endl;
    stream << "}" << endl;
    stream.flush();
    Item * const providerItem =
            m_reader->readFile(dummyItemFile.fileName(), location);
    if (providerItem->type() != ItemType::ModuleProvider) {
        throw ErrorInfo(Tr::tr("File '%1' declares an item of type '%2', "
                               "but '%3' was expected.")
            .arg(providerFile, providerItem->typeName(),
                 BuiltinDeclarations::instance().nameForType(ItemType::ModuleProvider)));
    }
    providerItem->setParent(product.item);
    providerItem->overrideProperties(moduleConfig, name.toString(), m_parameters, m_logger);

    m_probesResolver->resolveProbes(&product, providerItem);

    EvalContextSwitcher contextSwitcher(m_evaluator->engine(), EvalContext::ModuleProvider);
    return m_evaluator->stringListValue(providerItem, QStringLiteral("searchPaths"));
}

} // namespace Internal
} // namespace qbs
