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

#include "moduleloader.h"

#include "evaluator.h"
#include "itemreader.h"
#include "value.h"

#include <api/languageinfo.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <unordered_map>
#include <utility>

namespace qbs::Internal {

class ModuleLoader::Private
{
public:
    Private(const SetupProjectParameters &setupParameters, ItemReader &itemReader,
            Evaluator &evaluator, Logger &logger)
        : setupParameters(setupParameters), itemReader(itemReader), evaluator(evaluator),
        logger(logger) {}

    std::pair<Item *, bool> getModulePrototype(const ModuleLoader::ProductContext &product,
                                               const QString &moduleName, const QString &filePath);
    bool evaluateModuleCondition(const ModuleLoader::ProductContext &product, Item *module,
                                 const QString &fullModuleName);
    void forwardParameterDeclarations(const QualifiedId &moduleName, Item *item,
                                      const Item::Modules &modules);

    const SetupProjectParameters &setupParameters;
    ItemReader &itemReader;
    Evaluator &evaluator;
    Logger &logger;

    // The keys are file paths, the values are module prototype items accompanied by a profile.
    std::unordered_map<QString, std::vector<std::pair<Item *, QString>>> modulePrototypes;

    // The keys are module prototypes and products, the values specify whether the module's
    // condition is true for that product.
    std::unordered_map<std::pair<Item *, const Item *>, bool> modulePrototypeEnabledInfo;

    std::unordered_map<const Item *, std::vector<ErrorInfo>> unknownProfilePropertyErrors;
    std::unordered_map<const Item *, Item::PropertyDeclarationMap> parameterDeclarations;
};

ModuleLoader::ModuleLoader(const SetupProjectParameters &setupParameters, ItemReader &itemReader,
                           Evaluator &evaluator, Logger &logger)
    : d(new Private(setupParameters, itemReader, evaluator, logger)) { }

ModuleLoader::~ModuleLoader() { delete d; }

std::pair<Item *, bool> ModuleLoader::loadModuleFile(
    const ProductContext &product, const QString &moduleName, const QString &filePath)
{
    qCDebug(lcModuleLoader) << "loadModuleFile" << moduleName << "from" << filePath;

    const auto [module, triedToLoad] =
        d->getModulePrototype(product, moduleName, filePath);
    if (!module)
        return {nullptr, triedToLoad};

    const auto key = std::make_pair(module, product.item);
    const auto it = d->modulePrototypeEnabledInfo.find(key);
    if (it != d->modulePrototypeEnabledInfo.end()) {
        qCDebug(lcModuleLoader) << "prototype cache hit (level 2)";
        return {it->second ? module : nullptr, triedToLoad};
    }

    if (!d->evaluateModuleCondition(product, module, moduleName)) {
        qCDebug(lcModuleLoader) << "condition of module" << moduleName << "is false";
        d->modulePrototypeEnabledInfo.insert({key, false});
        return {nullptr, triedToLoad};
    }

    if (moduleName == StringConstants::qbsModule()) {
        module->setProperty(QStringLiteral("hostPlatform"),
                            VariantValue::create(HostOsInfo::hostOSIdentifier()));
        module->setProperty(QStringLiteral("hostArchitecture"),
                            VariantValue::create(HostOsInfo::hostOSArchitecture()));
        module->setProperty(QStringLiteral("libexecPath"),
                            VariantValue::create(d->setupParameters.libexecPath()));

        const Version qbsVersion = LanguageInfo::qbsVersion();
        module->setProperty(QStringLiteral("versionMajor"),
                            VariantValue::create(qbsVersion.majorVersion()));
        module->setProperty(QStringLiteral("versionMinor"),
                            VariantValue::create(qbsVersion.minorVersion()));
        module->setProperty(QStringLiteral("versionPatch"),
                            VariantValue::create(qbsVersion.patchLevel()));
    } else {
        Item::PropertyDeclarationMap decls;
        const auto &moduleChildren = module->children();
        for (Item *param : moduleChildren) {
            if (param->type() != ItemType::Parameter)
                continue;
            const auto &paramDecls = param->propertyDeclarations();
            for (auto it = paramDecls.begin(); it != paramDecls.end(); ++it)
                decls.insert(it.key(), it.value());
        }
        d->parameterDeclarations.insert({module, decls});
    }

    d->modulePrototypeEnabledInfo.insert({key, true});
    return {module, triedToLoad};
}

void ModuleLoader::checkProfileErrorsForModule(
    Item *module, const QString &moduleName, const QString &productName, const QString &profileName)
{
    const auto it = d->unknownProfilePropertyErrors.find(module);
    if (it != d->unknownProfilePropertyErrors.cend()) {
        ErrorInfo error(Tr::tr("Loading module '%1' for product '%2' failed due to invalid values "
                               "in profile '%3':").arg(moduleName, productName, profileName));
        for (const ErrorInfo &e : it->second)
            error.append(e.toString());
        handlePropertyError(error, d->setupParameters, d->logger);
    }
}

std::pair<Item *, bool> ModuleLoader::Private::getModulePrototype(
    const ProductContext &product, const QString &moduleName, const QString &filePath)
{
    auto &prototypeList = modulePrototypes[filePath];
    for (const auto &prototype : prototypeList) {
        if (prototype.second == product.profile) {
            qCDebug(lcModuleLoader) << "prototype cache hit (level 1)";
            return {prototype.first, true};
        }
    }

    Item * const module = itemReader.setupItemFromFile(filePath, CodeLocation(), evaluator);
    if (module->type() != ItemType::Module) {
        qCDebug(lcModuleLoader).nospace()
            << "Alleged module " << moduleName << " has type '"
            << module->typeName() << "', so it's not a module after all.";
        return {nullptr, false};
    }
    prototypeList.emplace_back(module, product.profile);

    // Module properties that are defined in the profile are used as default values.
    // This is the reason we need to have different items per profile.
    const QVariantMap profileModuleProperties
        = product.profileModuleProperties.value(moduleName).toMap();
    for (auto it = profileModuleProperties.cbegin(); it != profileModuleProperties.cend(); ++it) {
        if (Q_UNLIKELY(!module->hasProperty(it.key()))) {
            unknownProfilePropertyErrors[module].emplace_back(Tr::tr("Unknown property: %1.%2")
                                                                  .arg(moduleName, it.key()));
            continue;
        }
        const PropertyDeclaration decl = module->propertyDeclaration(it.key());
        VariantValuePtr v = VariantValue::create(
            PropertyDeclaration::convertToPropertyType(it.value(), decl.type(),
                                                       QStringList(moduleName), it.key()));
        v->markAsSetByProfile();
        module->setProperty(it.key(), v);
    }

    return {module, true};
}

bool ModuleLoader::Private::evaluateModuleCondition(const ProductContext &product,
                                                    Item *module, const QString &fullModuleName)
{
    // Evaluator reqires module name to be set.
    module->setProperty(StringConstants::nameProperty(), VariantValue::create(fullModuleName));

    // Temporarily make the product's qbs module instance available, so the condition
    // can use qbs.targetOS etc.
    class TempQbsModuleProvider {
    public:
        TempQbsModuleProvider(const ProductContext &product,
                              Item *module, const QString &moduleName)
            : m_module(module), m_needsQbsItem(moduleName != StringConstants::qbsModule())
        {
            if (m_needsQbsItem) {
                m_prevQbsItemValue = module->property(StringConstants::qbsModule());
                module->setProperty(StringConstants::qbsModule(),
                                    product.item->property(StringConstants::qbsModule()));
            }
        }
        ~TempQbsModuleProvider()
        {
            if (!m_needsQbsItem)
                return;
            if (m_prevQbsItemValue)
                m_module->setProperty(StringConstants::qbsModule(), m_prevQbsItemValue);
            else
                m_module->removeProperty(StringConstants::qbsModule());
        }
    private:
        Item * const m_module;
        ValuePtr m_prevQbsItemValue;
        const bool m_needsQbsItem;
    };

    const TempQbsModuleProvider tempQbs(product, module, fullModuleName);
    return evaluator.boolValue(module, StringConstants::conditionProperty());
}

class DependencyParameterDeclarationCheck
{
public:
    DependencyParameterDeclarationCheck(
            const QString &productName, const Item *productItem,
            const std::unordered_map<const Item *, Item::PropertyDeclarationMap> &decls)
        : m_productName(productName), m_productItem(productItem), m_parameterDeclarations(decls)
    {}

    void operator()(const QVariantMap &parameters) const { check(parameters, QualifiedId()); }

private:
    void check(const QVariantMap &parameters, const QualifiedId &moduleName) const
    {
        for (auto it = parameters.begin(); it != parameters.end(); ++it) {
            if (it.value().userType() == QMetaType::QVariantMap) {
                check(it.value().toMap(), QualifiedId(moduleName) << it.key());
            } else {
                const auto &deps = m_productItem->modules();
                auto m = std::find_if(deps.begin(), deps.end(),
                                      [&moduleName] (const Item::Module &module) {
                                          return module.name == moduleName;
                                      });

                if (m == deps.end()) {
                    const QualifiedId fullName = QualifiedId(moduleName) << it.key();
                    throw ErrorInfo(Tr::tr("Cannot set parameter '%1', "
                                           "because '%2' does not have a dependency on '%3'.")
                                        .arg(fullName.toString(), m_productName, moduleName.toString()),
                                    m_productItem->location());
                }

                const auto decls = m_parameterDeclarations.find(m->item->rootPrototype());
                if (decls == m_parameterDeclarations.end() || !decls->second.contains(it.key())) {
                    const QualifiedId fullName = QualifiedId(moduleName) << it.key();
                    throw ErrorInfo(Tr::tr("Parameter '%1' is not declared.")
                                        .arg(fullName.toString()), m_productItem->location());
                }
            }
        }
    }

    bool moduleExists(const QualifiedId &name) const
    {
        const auto &deps = m_productItem->modules();
        return any_of(deps, [&name](const Item::Module &module) {
            return module.name == name;
        });
    }

    const QString &m_productName;
    const Item * const m_productItem;
    const std::unordered_map<const Item *, Item::PropertyDeclarationMap> &m_parameterDeclarations;
};

void ModuleLoader::checkDependencyParameterDeclarations(const ProductContext &product) const
{
    DependencyParameterDeclarationCheck dpdc(product.name, product.item, d->parameterDeclarations);
    for (const Item::Module &dep : product.item->modules()) {
        if (!dep.parameters.empty())
            dpdc(dep.parameters);
    }
}

void ModuleLoader::forwardParameterDeclarations(const Item *dependsItem,
                                                const Item::Modules &modules)
{
    for (auto it = dependsItem->properties().begin(); it != dependsItem->properties().end(); ++it) {
        if (it.value()->type() != Value::ItemValueType)
            continue;
        d->forwardParameterDeclarations(it.key(),
                                        std::static_pointer_cast<ItemValue>(it.value())->item(),
                                        modules);
    }
}

void ModuleLoader::Private::forwardParameterDeclarations(const QualifiedId &moduleName, Item *item,
                                                         const Item::Modules &modules)
{
    auto it = std::find_if(modules.begin(), modules.end(), [&moduleName] (const Item::Module &m) {
        return m.name == moduleName;
    });
    if (it != modules.end()) {
        item->setPropertyDeclarations(parameterDeclarations[it->item->rootPrototype()]);
    } else {
        for (auto it = item->properties().begin(); it != item->properties().end(); ++it) {
            if (it.value()->type() != Value::ItemValueType)
                continue;
            forwardParameterDeclarations(QualifiedId(moduleName) << it.key(),
                                         std::static_pointer_cast<ItemValue>(it.value())->item(),
                                         modules);
        }
    }
}

} // namespace qbs::Internal
