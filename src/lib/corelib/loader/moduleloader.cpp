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

#include "itemreader.h"
#include "loaderutils.h"
#include "moduleproviderloader.h"

#include <api/languageinfo.h>
#include <language/evaluator.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/codelocation.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/profiling.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <QDirIterator>
#include <QHash>

#include <unordered_map>
#include <utility>

namespace qbs::Internal {

class ModuleLoader
{
public:
    ModuleLoader(LoaderState &loaderState, ProductContext &product,
                 const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
                 FallbackMode fallbackMode)
        : m_loaderState(loaderState), m_product(product),
          m_dependsItemLocation(dependsItemLocation), m_moduleName(moduleName),
          m_fallbackMode(fallbackMode)
    {}

    Item *load();

private:
    std::pair<Item *, bool> loadModuleFile(const QString &moduleName, const QString &filePath);
    Item *getModulePrototype(const QString &moduleName, const QString &filePath);
    Item *createAndInitModuleItem(const QString &moduleName, const QString &filePath);
    bool evaluateModuleCondition(Item *module, const QString &fullModuleName);
    void checkForUnknownProfileProperties(const Item *module);
    QString findModuleDirectory(const QString &searchPath);
    QStringList findModuleDirectories();
    QStringList getModuleFilePaths(const QString &dir);

    LoaderState &m_loaderState;
    ProductContext &m_product;
    const CodeLocation &m_dependsItemLocation;
    const QualifiedId &m_moduleName;
    const FallbackMode m_fallbackMode;
};

struct PrioritizedItem
{
    PrioritizedItem(Item *item, int priority, int searchPathIndex)
        : item(item), priority(priority), searchPathIndex(searchPathIndex) { }

    Item * const item;
    int priority = 0;
    const int searchPathIndex;
};

static Item *chooseModuleCandidate(const std::vector<PrioritizedItem> &candidates,
                                   const QString &moduleName)
{
    // TODO: This should also consider the version requirement.

    auto maxIt = std::max_element(
        candidates.begin(), candidates.end(),
        [] (const PrioritizedItem &a, const PrioritizedItem &b) {
            if (a.priority < b.priority)
                return true;
            if (a.priority > b.priority)
                return false;
            return a.searchPathIndex > b.searchPathIndex;
        });

    size_t nmax = std::count_if(
        candidates.begin(), candidates.end(),
        [maxIt] (const PrioritizedItem &i) {
            return i.priority == maxIt->priority && i.searchPathIndex == maxIt->searchPathIndex;
        });

    if (nmax > 1) {
        ErrorInfo e(Tr::tr("There is more than one equally prioritized candidate for module '%1'.")
                        .arg(moduleName));
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto candidate = candidates.at(i);
            if (candidate.priority == maxIt->priority) {
                //: The %1 denotes the number of the candidate.
                e.append(Tr::tr("candidate %1").arg(i + 1), candidates.at(i).item->location());
            }
        }
        throw e;
    }

    return maxIt->item;
}

Item *searchAndLoadModuleFile(LoaderState &loaderState, ProductContext &product,
        const CodeLocation &dependsItemLocation, const QualifiedId &moduleName,
        FallbackMode fallbackMode)
{
    return ModuleLoader(loaderState, product, dependsItemLocation, moduleName, fallbackMode).load();
}

Item *ModuleLoader::load()
{
    SearchPathsManager searchPathsManager(m_loaderState.itemReader());

    QStringList existingPaths = findModuleDirectories();
    if (existingPaths.isEmpty()) { // no suitable names found, try to use providers
        AccumulatingTimer providersTimer(m_loaderState.parameters().logElapsedTime()
                                         ? &m_product.timingData.moduleProviders : nullptr);
        auto result = ModuleProviderLoader(m_loaderState).executeModuleProviders(
                    m_product, m_dependsItemLocation, m_moduleName, m_fallbackMode);
        if (result.searchPaths) {
            qCDebug(lcModuleLoader) << "Re-checking for module" << m_moduleName.toString()
                                    << "with newly added search paths from module provider";
            m_loaderState.itemReader().pushExtraSearchPaths(*result.searchPaths);
            existingPaths = findModuleDirectories();
        }
    }

    const QString fullName = m_moduleName.toString();
    bool triedToLoadModule = false;
    std::vector<PrioritizedItem> candidates;
    candidates.reserve(size_t(existingPaths.size()));
    for (int i = 0; i < existingPaths.size(); ++i) {
        const QStringList &moduleFileNames = getModuleFilePaths(existingPaths.at(i));
        for (const QString &filePath : moduleFileNames) {
            const auto [module, triedToLoad] = loadModuleFile(fullName, filePath);
            if (module)
                candidates.emplace_back(module, 0, i);
            if (!triedToLoad)
                m_loaderState.topLevelProject().removeModuleFileFromDirectoryCache(filePath);
            triedToLoadModule = triedToLoadModule || triedToLoad;
        }
    }

    if (candidates.empty())
        return nullptr;

    Item *moduleItem = nullptr;
    if (candidates.size() == 1) {
        moduleItem = candidates.at(0).item;
    } else {
        for (auto &candidate : candidates) {
            ModuleItemLocker lock(*candidate.item);
            candidate.priority = m_loaderState.evaluator()
                    .intValue(candidate.item, StringConstants::priorityProperty(),
                              candidate.priority);
        }
        moduleItem = chooseModuleCandidate(candidates, fullName);
    }

    checkForUnknownProfileProperties(moduleItem);
    return moduleItem;
}

std::pair<Item *, bool> ModuleLoader::loadModuleFile(const QString &moduleName,
                                                     const QString &filePath)
{
    qCDebug(lcModuleLoader) << "loadModuleFile" << moduleName << "from" << filePath;

    Item * const module = getModulePrototype(moduleName, filePath);
    if (!module)
        return {nullptr, false};

    const auto it = m_product.modulePrototypeEnabledInfo.find(module);
    if (it != m_product.modulePrototypeEnabledInfo.end()) {
        qCDebug(lcModuleLoader) << "prototype cache hit (level 2)";
        return {it->second ? module : nullptr, true};
    }

    if (!evaluateModuleCondition(module, moduleName)) {
        qCDebug(lcModuleLoader) << "condition of module" << moduleName << "is false";
        m_product.modulePrototypeEnabledInfo.insert({module, false});
        return {nullptr, true};
    }

    m_product.modulePrototypeEnabledInfo.insert({module, true});
    return {module, true};
}

Item * ModuleLoader::getModulePrototype(const QString &moduleName, const QString &filePath)
{
    bool fromCache = true;
    Item * const module = m_loaderState.topLevelProject().getModulePrototype(
                filePath, m_product.profileName, [&] {
        fromCache = false;
        return createAndInitModuleItem(moduleName, filePath);
    });

    if (fromCache)
        qCDebug(lcModuleLoader) << "prototype cache hit (level 1)";
    return module;
}

Item *ModuleLoader::createAndInitModuleItem(const QString &moduleName, const QString &filePath)
{
    Item * const module = m_loaderState.itemReader().setupItemFromFile(filePath, {});
    if (module->type() != ItemType::Module) {
        qCDebug(lcModuleLoader).nospace()
            << "Alleged module " << moduleName << " has type '"
            << module->typeName() << "', so it's not a module after all.";
        return nullptr;
    }

    // Not technically needed, but we want to keep the invariant in item.cpp.
    ModuleItemLocker locker(*module);

    module->setProperty(StringConstants::nameProperty(), VariantValue::create(moduleName));
    if (moduleName == StringConstants::qbsModule()) {
        module->setProperty(QStringLiteral("hostPlatform"),
                            VariantValue::create(HostOsInfo::hostOSIdentifier()));
        module->setProperty(QStringLiteral("hostArchitecture"),
                            VariantValue::create(HostOsInfo::hostOSArchitecture()));
        module->setProperty(QStringLiteral("libexecPath"),
                            VariantValue::create(m_loaderState.parameters().libexecPath()));

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
            if (param->type() == ItemType::Parameter) {
                const auto &paramDecls = param->propertyDeclarations();
                for (auto it = paramDecls.begin(); it != paramDecls.end(); ++it)
                    decls.insert(it.key(), it.value());
            } else if (param->type() == ItemType::Parameters) {
                adjustParametersScopes(param, param);
                Evaluator &evaluator = m_loaderState.evaluator();
                QVariantMap parameters = getJsVariant(evaluator.engine()->context(),
                                                      evaluator.scriptValue(param)).toMap();
                m_loaderState.topLevelProject().setParameters(module, parameters);
            }
        }
        m_loaderState.topLevelProject().addParameterDeclarations(module, decls);
    }

    // Module properties that are defined in the profile are used as default values.
    // This is the reason we need to have different items per profile.
    const QVariantMap profileModuleProperties
        = m_product.profileModuleProperties.value(moduleName).toMap();
    for (auto it = profileModuleProperties.cbegin(); it != profileModuleProperties.cend(); ++it) {
        if (Q_UNLIKELY(!module->hasProperty(it.key()))) {
            m_loaderState.topLevelProject().addUnknownProfilePropertyError(
                module, {Tr::tr("Unknown property: %1.%2").arg(moduleName, it.key())});
            continue;
        }
        const PropertyDeclaration decl = module->propertyDeclaration(it.key());
        VariantValuePtr v = VariantValue::create(
            PropertyDeclaration::convertToPropertyType(it.value(), decl.type(),
                                                       QStringList(moduleName), it.key()));
        v->markAsSetByProfile();
        module->setProperty(it.key(), v);
    }

    return module;
}

bool ModuleLoader::evaluateModuleCondition(Item *module, const QString &fullModuleName)
{
    ModuleItemLocker locker(*module);

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

    const TempQbsModuleProvider tempQbs(m_product, module, fullModuleName);
    return m_loaderState.evaluator().boolValue(module, StringConstants::conditionProperty());
}

void ModuleLoader::checkForUnknownProfileProperties(const Item *module)
{
    const std::vector<ErrorInfo> &errors
            = m_loaderState.topLevelProject().unknownProfilePropertyErrors(module);
    if (errors.empty())
        return;

    ErrorInfo error(Tr::tr("Loading module '%1' for product '%2' failed due to invalid values "
                           "in profile '%3':")
                    .arg(m_moduleName.toString(), m_product.displayName(), m_product.profileName));
    for (const ErrorInfo &e : errors)
        error.append(e.toString());
    handlePropertyError(error, m_loaderState.parameters(), m_loaderState.logger());
}

QString ModuleLoader::findModuleDirectory(const QString &searchPath)
{
    // isFileCaseCorrect is a very expensive call on macOS, so we cache the value for the
    // modules and search paths we've already processed
    return m_loaderState.topLevelProject().findModuleDirectory(m_moduleName, searchPath,
                                                             [&] {
        QString dirPath = searchPath + QStringLiteral("/modules");
        for (const QString &moduleNamePart : m_moduleName) {
            dirPath = FileInfo::resolvePath(dirPath, moduleNamePart);
            if (!FileInfo::exists(dirPath) || !FileInfo::isFileCaseCorrect(dirPath))
                return QString();
        }
        return dirPath;
    });
}

QStringList ModuleLoader::findModuleDirectories()
{
    const QStringList &searchPaths = m_loaderState.itemReader().allSearchPaths();
    QStringList result;
    result.reserve(searchPaths.size());
    for (const auto &path: searchPaths) {
        const QString dirPath = findModuleDirectory(path);
        if (!dirPath.isEmpty())
            result.append(dirPath);
    }
    return result;
}

QStringList ModuleLoader::getModuleFilePaths(const QString &dir)
{
    return m_loaderState.topLevelProject().getModuleFilesForDirectory(dir, [&] {
        QStringList moduleFiles;
        QDirIterator dirIter(dir, StringConstants::qbsFileWildcards());
        while (dirIter.hasNext())
            moduleFiles += dirIter.next();
        return moduleFiles;
    });
}

} // namespace qbs::Internal
