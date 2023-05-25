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

#include "projecttreebuilder.h"

#include "dependenciesresolver.h"
#include "itemreader.h"
#include "localprofiles.h"
#include "moduleinstantiator.h"
#include "modulepropertymerger.h"
#include "probesresolver.h"
#include "productitemmultiplexer.h"
#include "productscollector.h"
#include "productshandler.h"

#include <language/builtindeclarations.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/filetags.h>
#include <language/item.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/filetime.h>
#include <tools/preferences.h>
#include <tools/progressobserver.h>
#include <tools/profile.h>
#include <tools/profiling.h>
#include <tools/scripttools.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>
#include <tools/version.h>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <utility>
#include <vector>

namespace qbs::Internal {

class ProjectTreeBuilder::Private
{
public:
    Private(const SetupProjectParameters &parameters, ItemPool &itemPool, Evaluator &evaluator,
            Logger &logger) : state(parameters, itemPool, evaluator, logger) {}

    Item *loadTopLevelProjectItem();
    void checkOverriddenValues();
    void collectNameFromOverride(const QString &overrideString);
    void handleTopLevelProject(Item *projectItem);
    void printProfilingInfo();

    LoaderState state;
    ProductsCollector productsCollector{state};
    ProductsHandler productsHandler{state};
    FileTime lastResolveTime;
    QVariantMap storedProfiles;

    qint64 elapsedTimePropertyChecking = 0;
};

ProjectTreeBuilder::ProjectTreeBuilder(const SetupProjectParameters &parameters, ItemPool &itemPool,
                                       Evaluator &evaluator, Logger &logger)
    : d(makePimpl<Private>(parameters, itemPool, evaluator, logger)) {}
ProjectTreeBuilder::~ProjectTreeBuilder() = default;

void ProjectTreeBuilder::setProgressObserver(ProgressObserver *progressObserver)
{
    d->state.topLevelProject().progressObserver = progressObserver;
}

void ProjectTreeBuilder::setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes)
{
    d->state.probesResolver().setOldProjectProbes(oldProbes);
}

void ProjectTreeBuilder::setOldProductProbes(
    const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes)
{
    d->state.probesResolver().setOldProductProbes(oldProbes);
}

void ProjectTreeBuilder::setLastResolveTime(const FileTime &time) { d->lastResolveTime = time; }

void ProjectTreeBuilder::setStoredProfiles(const QVariantMap &profiles)
{
    d->storedProfiles = profiles;
}

void ProjectTreeBuilder::setStoredModuleProviderInfo(
    const StoredModuleProviderInfo &moduleProviderInfo)
{
    d->state.dependenciesResolver().setStoredModuleProviderInfo(moduleProviderInfo);
}

ProjectTreeBuilder::Result ProjectTreeBuilder::load()
{
    TimedActivityLogger mainTimer(d->state.logger(), Tr::tr("ProjectTreeBuilder"),
                                  d->state.parameters().logElapsedTime());
    qCDebug(lcModuleLoader) << "load" << d->state.parameters().projectFilePath();

    d->checkOverriddenValues();

    Result result;
    TopLevelProjectContext &project = d->state.topLevelProject();
    project.profileConfigs = d->storedProfiles;
    result.root = d->loadTopLevelProjectItem();
    d->handleTopLevelProject(result.root);

    result.qbsFiles = d->state.itemReader().filesRead()
            - d->state.dependenciesResolver() .tempQbsFiles();
    result.productInfos = project.productInfos;
    result.profileConfigs = project.profileConfigs;
    const QVariantMap &profiles = d->state.localProfiles().profiles();
    for (auto it = profiles.begin(); it != profiles.end(); ++it)
        result.profileConfigs.remove(it.key());
    result.projectProbes = project.probes;
    result.storedModuleProviderInfo = d->state.dependenciesResolver().storedModuleProviderInfo();

    d->printProfilingInfo();

    return result;
}

Item *ProjectTreeBuilder::Private::loadTopLevelProjectItem()
{
    const QStringList topLevelSearchPaths
            = state.parameters().finalBuildConfigurationTree()
              .value(StringConstants::projectPrefix()).toMap()
              .value(StringConstants::qbsSearchPathsProperty()).toStringList();
    SearchPathsManager searchPathsManager(state.itemReader(), topLevelSearchPaths);
    Item * const root = state.itemReader().setupItemFromFile(
                state.parameters().projectFilePath(), {});
    if (!root)
        return {};

    switch (root->type()) {
    case ItemType::Product:
        return state.itemReader().wrapInProjectIfNecessary(root);
    case ItemType::Project:
        return root;
    default:
        throw ErrorInfo(Tr::tr("The top-level item must be of type 'Project' or 'Product', but it"
                               " is of type '%1'.").arg(root->typeName()), root->location());
    }
}

void ProjectTreeBuilder::Private::checkOverriddenValues()
{
    static const auto matchesPrefix = [](const QString &key) {
        static const QStringList prefixes({StringConstants::projectPrefix(),
                                           QStringLiteral("projects"),
                                           QStringLiteral("products"), QStringLiteral("modules"),
                                           StringConstants::moduleProviders(),
                                           StringConstants::qbsModule()});
        for (const auto &prefix : prefixes) {
            if (key.startsWith(prefix + QLatin1Char('.')))
                return true;
        }
        return false;
    };
    const QVariantMap &overriddenValues = state.parameters().overriddenValues();
    for (auto it = overriddenValues.begin(); it != overriddenValues.end(); ++it) {
        if (matchesPrefix(it.key())) {
            collectNameFromOverride(it.key());
            continue;
        }

        ErrorInfo e(Tr::tr("Property override key '%1' not understood.").arg(it.key()));
        e.append(Tr::tr("Please use one of the following:"));
        e.append(QLatin1Char('\t') + Tr::tr("projects.<project-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("modules.<module-name>.<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("products.<product-name>.<module-name>."
                                            "<property-name>:value"));
        e.append(QLatin1Char('\t') + Tr::tr("moduleProviders.<provider-name>."
                                            "<property-name>:value"));
        handlePropertyError(e, state.parameters(), state.logger());
    }
}

void ProjectTreeBuilder::Private::collectNameFromOverride(const QString &overrideString)
{
    static const auto extract = [](const QString &prefix, const QString &overrideString) {
        if (!overrideString.startsWith(prefix))
            return QString();
        const int startPos = prefix.length();
        const int endPos = overrideString.lastIndexOf(StringConstants::dot());
        if (endPos == -1)
            return QString();
        return overrideString.mid(startPos, endPos - startPos);
    };
    const QString &projectName = extract(StringConstants::projectsOverridePrefix(), overrideString);
    if (!projectName.isEmpty()) {
        state.topLevelProject().projectNamesUsedInOverrides.insert(projectName);
        return;
    }
    const QString &productName = extract(StringConstants::productsOverridePrefix(), overrideString);
    if (!productName.isEmpty()) {
        state.topLevelProject().productNamesUsedInOverrides.insert(productName.left(
            productName.indexOf(StringConstants::dot())));
        return;
    }
}

void ProjectTreeBuilder::Private::handleTopLevelProject(Item *projectItem)
{
    state.topLevelProject().buildDirectory = TopLevelProject::deriveBuildDirectory(
                state.parameters().buildRoot(),
                TopLevelProject::deriveId(state.parameters().finalBuildConfigurationTree()));
    projectItem->setProperty(StringConstants::sourceDirectoryProperty(),
                             VariantValue::create(QFileInfo(projectItem->file()->filePath())
                                                      .absolutePath()));
    projectItem->setProperty(StringConstants::buildDirectoryProperty(),
                             VariantValue::create(state.topLevelProject().buildDirectory));
    projectItem->setProperty(StringConstants::profileProperty(),
                             VariantValue::create(state.parameters().topLevelProfile()));
    productsCollector.run(projectItem);
    productsHandler.run();

    state.itemReader().clearExtraSearchPathsStack(); // TODO: Unneeded?
    AccumulatingTimer timer(state.parameters().logElapsedTime()
                            ? &elapsedTimePropertyChecking : nullptr);
    checkPropertyDeclarations(projectItem, state.topLevelProject().disabledItems,
                              state.parameters(), state.logger());
}

void ProjectTreeBuilder::Private::printProfilingInfo()
{
    if (!state.parameters().logElapsedTime())
        return;
    state.logger().qbsLog(LoggerInfo, true)
            << "  "
            << Tr::tr("Project file loading and parsing took %1.")
               .arg(elapsedTimeString(state.itemReader().elapsedTime()));
    productsCollector.printProfilingInfo(2);
    state.dependenciesResolver().printProfilingInfo(4);
    state.moduleInstantiator().printProfilingInfo(6);
    state.propertyMerger().printProfilingInfo(6);
    state.probesResolver().printProfilingInfo(4);
    state.logger().qbsLog(LoggerInfo, true)
            << "  "
            << Tr::tr("Property checking took %1.")
               .arg(elapsedTimeString(elapsedTimePropertyChecking));
}

} // namespace qbs::Internal
