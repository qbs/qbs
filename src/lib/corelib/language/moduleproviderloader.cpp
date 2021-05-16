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

#include <language/scriptengine.h>
#include <language/value.h>

#include <logging/categories.h>
#include <logging/translator.h>

#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/stringconstants.h>

#include <QtCore/qtemporaryfile.h>

namespace qbs {
namespace Internal {

ModuleProviderLoader::ModuleProviderLoader(ItemReader *reader, Evaluator *evaluator)
    : m_reader(reader)
    , m_evaluator(evaluator)
{
}

void ModuleProviderLoader::setupKnownModuleProviders(ProductContext &product)
{
    // Existing module provider search paths are re-used if and only if the provider configuration
    // at setup time was the same as the current one for the respective module provider.
    if (!m_moduleProviderInfo.empty()) {
        const QVariantMap configForProduct = moduleProviderConfig(product);
        for (const ModuleProviderInfo &c : m_moduleProviderInfo) {
            if (configForProduct.value(c.name.toString()).toMap() == c.config) {
                qCDebug(lcModuleLoader) << "re-using search paths" << c.searchPaths
                                        << "from module provider" << c.name
                                        << "for product" << product.name;
                product.knownModuleProviders.insert(c.name);
                product.searchPaths << c.searchPaths;
            }
        }
    }
}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::executeModuleProvider(
        ProductContext &productContext,
        const CodeLocation &dependsItemLocation,
        const QualifiedId &moduleName,
        FallbackMode fallbackMode)
{
    bool moduleAlreadyKnown = false;
    ModuleProviderResult result;
    for (QualifiedId providerName = moduleName; !providerName.empty();
         providerName.pop_back()) {
        if (!productContext.knownModuleProviders.insert(providerName).second) {
            moduleAlreadyKnown = true;
            break;
        }
        qCDebug(lcModuleLoader) << "Module" << moduleName.toString()
                                << "not found, checking for module providers";
        result = findModuleProvider(providerName, productContext,
                                    ModuleProviderLookup::Regular, dependsItemLocation);
        if (result.providerFound)
            break;
    }
    if (fallbackMode == FallbackMode::Enabled && !result.providerFound
            && !moduleAlreadyKnown) {
        qCDebug(lcModuleLoader) << "Specific module provider not found for"
                                << moduleName.toString()  << ", setting up fallback.";
        result = findModuleProvider(moduleName, productContext,
                                    ModuleProviderLookup::Fallback, dependsItemLocation);
    }
    return result;
}

ModuleProviderLoader::ModuleProviderResult ModuleProviderLoader::findModuleProvider(
        const QualifiedId &name,
        ProductContext &product,
        ModuleProviderLookup lookupType,
        const CodeLocation &dependsItemLocation)
{
    for (const QString &path : m_reader->allSearchPaths()) {
        QString fullPath = FileInfo::resolvePath(path, QStringLiteral("module-providers"));
        switch (lookupType) {
        case ModuleProviderLookup::Regular:
            for (const QString &component : name)
                fullPath = FileInfo::resolvePath(fullPath, component);
            break;
        case ModuleProviderLookup::Fallback:
            fullPath = FileInfo::resolvePath(fullPath, QStringLiteral("__fallback"));
            break;
        }
        const QString providerFile = FileInfo::resolvePath(fullPath,
                                                           QStringLiteral("provider.qbs"));
        if (!FileInfo::exists(providerFile)) {
            qCDebug(lcModuleLoader) << "No module provider found at" << providerFile;
            continue;
        }
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
        const QVariant moduleConfig = moduleProviderConfig(product).value(name.toString());
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
                m_reader->readFile(dummyItemFile.fileName(), dependsItemLocation);
        if (providerItem->type() != ItemType::ModuleProvider) {
            throw ErrorInfo(Tr::tr("File '%1' declares an item of type '%2', "
                                   "but '%3' was expected.")
                .arg(providerFile, providerItem->typeName(),
                     BuiltinDeclarations::instance().nameForType(ItemType::ModuleProvider)));
        }
        providerItem->setParent(product.item);
        const QVariantMap configMap = moduleConfig.toMap();
        for (auto it = configMap.begin(); it != configMap.end(); ++it) {
            const PropertyDeclaration decl = providerItem->propertyDeclaration(it.key());
            if (!decl.isValid()) {
                throw ErrorInfo(Tr::tr("No such property '%1' in module provider '%2'.")
                                .arg(it.key(), name.toString()));
            }
            providerItem->setProperty(it.key(), VariantValue::create(it.value()));
        }
        EvalContextSwitcher contextSwitcher(m_evaluator->engine(), EvalContext::ModuleProvider);
        const QStringList searchPaths
                = m_evaluator->stringListValue(providerItem, QStringLiteral("searchPaths"));
        const auto addToGlobalInfo = [=] {
            m_moduleProviderInfo.emplace_back(ModuleProviderInfo(name, moduleConfig.toMap(),
                                                         searchPaths, m_parameters.dryRun()));
        };
        if (searchPaths.empty()) {
            qCDebug(lcModuleLoader) << "Module provider did run, but did not set up "
                                       "any modules.";
            addToGlobalInfo();
            return {true, false};
        }
        qCDebug(lcModuleLoader) << "Module provider added" << searchPaths.size()
                                << "new search path(s)";

        // (1) is needed so the immediate new look-up works.
        // (2) is needed so the next use of SearchPathManager considers the new paths.
        // (3) is needed for possible re-use in subsequent products and builds.
        m_reader->pushExtraSearchPaths(searchPaths); // (1)
        product.searchPaths << searchPaths; // (2)
        addToGlobalInfo(); // (3)
        return {true, true};
    }
    return {};
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

} // namespace Internal
} // namespace qbs
