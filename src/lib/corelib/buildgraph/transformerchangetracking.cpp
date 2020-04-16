/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "transformerchangetracking.h"

#include "projectbuilddata.h"
#include "requesteddependencies.h"
#include "rulecommands.h"
#include "transformer.h"
#include <language/language.h>
#include <language/property.h>
#include <language/propertymapinternal.h>
#include <language/qualifiedid.h>
#include <logging/categories.h>
#include <tools/fileinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

class TrafoChangeTracker
{
public:
    TrafoChangeTracker(const Transformer *transformer,
                       const ResolvedProduct *product,
                       const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
                       const std::unordered_map<QString, const ResolvedProject *> &projectsByName)
        : m_transformer(transformer),
          m_product(product),
          m_productsByName(productsByName),
          m_projectsByName(projectsByName)
    {
    }

    bool prepareScriptNeedsRerun() const;
    bool commandsNeedRerun() const;

private:
    QVariantMap propertyMapByKind(const Property &property) const;
    bool checkForPropertyChange(const Property &restoredProperty,
                                const QVariantMap &newProperties) const;
    bool checkForImportFileChange(const std::vector<QString> &importedFiles,
                                  const FileTime &referenceTime,
                                  const char *context) const;
    bool isExportedModuleUpToDate(const QString &productName, const ExportedModule &module) const;
    bool areExportedModulesUpToDate(
            const std::unordered_map<QString, ExportedModule> &exportedModules) const;
    const Artifact *getArtifact(const QString &filePath, const QString &productName) const;
    const ResolvedProduct *getProduct(const QString &name) const;

    const Transformer * const m_transformer;
    const ResolvedProduct * const m_product;
    const std::unordered_map<QString, const ResolvedProduct *> &m_productsByName;
    const std::unordered_map<QString, const ResolvedProject *> &m_projectsByName;
    mutable const ResolvedProduct * m_lastProduct = nullptr;
    mutable const Artifact *m_lastArtifact = nullptr;
};

template<typename T> static QVariantMap getParameterValue(
        const QHash<T, QVariantMap> &parameters,
        const QString &depName)
{
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        if (it.key()->name == depName)
            return it.value();
    }
    return {};
}

QVariantMap TrafoChangeTracker::propertyMapByKind(const Property &property) const
{
    switch (property.kind) {
    case Property::PropertyInModule: {
        const ResolvedProduct * const p = getProduct(property.productName);
        return p ? p->moduleProperties->value() : QVariantMap();
    }
    case Property::PropertyInProduct: {
        const ResolvedProduct * const p = getProduct(property.productName);
        return p ? p->productProperties : QVariantMap();
    }
    case Property::PropertyInProject: {
        if (property.productName == m_product->project->name)
            return m_product->project->projectProperties();
        const auto it = m_projectsByName.find(property.productName);
        if (it != m_projectsByName.cend())
            return it->second->projectProperties();
        return {};
    }
    case Property::PropertyInParameters: {
        const int sepIndex = property.moduleName.indexOf(QLatin1Char(':'));
        const QString depName = property.moduleName.left(sepIndex);
        const ResolvedProduct * const p = getProduct(property.productName);
        if (!p)
            return {};
        QVariantMap v = getParameterValue(p->dependencyParameters, depName);
        if (!v.empty())
            return v;
        return getParameterValue(p->moduleParameters, depName);
    }
    case Property::PropertyInArtifact:
    default:
        QBS_CHECK(false);
    }
    return {};
}

bool TrafoChangeTracker::checkForPropertyChange(const Property &restoredProperty,
                                                const QVariantMap &newProperties) const
{
    QVariant v;
    switch (restoredProperty.kind) {
    case Property::PropertyInProduct:
    case Property::PropertyInProject:
        v = newProperties.value(restoredProperty.propertyName);
        break;
    case Property::PropertyInModule:
        v = moduleProperty(newProperties, restoredProperty.moduleName,
                           restoredProperty.propertyName);
        break;
    case Property::PropertyInParameters: {
        const int sepIndex = restoredProperty.moduleName.indexOf(QLatin1Char(':'));
        QualifiedId moduleName
                = QualifiedId::fromString(restoredProperty.moduleName.mid(sepIndex + 1));
        QVariantMap map = newProperties;
        while (!moduleName.empty())
            map = map.value(moduleName.takeFirst()).toMap();
        v = map.value(restoredProperty.propertyName);
        break;
    }
    case Property::PropertyInArtifact:
        QBS_CHECK(false);
    }
    if (restoredProperty.value != v) {
        qCDebug(lcBuildGraph).noquote().nospace()
                << "Value for property '" << restoredProperty.moduleName << "."
                << restoredProperty.propertyName << "' has changed.\n"
                << "Old value was '" << restoredProperty.value << "'.\n"
                << "New value is '" << v << "'.";
        return true;
    }
    return false;
}

bool TrafoChangeTracker::checkForImportFileChange(const std::vector<QString> &importedFiles,
                                                  const FileTime &referenceTime,
                                                  const char *context) const
{
    for (const QString &importedFile : importedFiles) {
        const FileInfo fi(importedFile);
        if (!fi.exists()) {
            qCDebug(lcBuildGraph) << context << "imported file" << importedFile
                                  << "is gone, need to re-run";
            return true;
        }
        if (fi.lastModified() > referenceTime) {
            qCDebug(lcBuildGraph) << context << "imported file" << importedFile
                                  << "has been updated, need to re-run"
                                  << fi.lastModified() << referenceTime;
            return true;
        }
    }
    return false;
}

bool TrafoChangeTracker::isExportedModuleUpToDate(const QString &productName,
                                                  const ExportedModule &module) const
{
    const ResolvedProduct * const product = getProduct(productName);
    if (!product) {
        qCDebug(lcBuildGraph) << "product" << productName
                              << "does not exist anymore, need to re-run";
        return false;
    }
    if (product->exportedModule != module) {
        qCDebug(lcBuildGraph) << "exported module has changed for product" << productName
                              << ", need to re-run";
        return false;
    }
    return true;
}

bool TrafoChangeTracker::areExportedModulesUpToDate(
        const std::unordered_map<QString, ExportedModule> &exportedModules) const
{
    for (const auto &kv : exportedModules) {
        if (!isExportedModuleUpToDate(kv.first, kv.second))
            return false;
    }
    return true;
}

const Artifact *TrafoChangeTracker::getArtifact(const QString &filePath,
                                                const QString &productName) const
{
    if (m_lastArtifact && m_lastArtifact->filePath() == filePath
            && m_lastArtifact->product.get()->uniqueName() == productName) {
        return m_lastArtifact;
    }
    const ResolvedProduct * const product = getProduct(productName);
    if (!product)
        return nullptr;
    const auto &candidates = product->topLevelProject()->buildData->lookupFiles(filePath);
    const Artifact *artifact = nullptr;
    for (const FileResourceBase * const candidate : candidates) {
        if (candidate->fileType() == FileResourceBase::FileTypeArtifact) {
            auto const a = static_cast<const Artifact *>(candidate);
            if (a->product.get() == product) {
                m_lastArtifact = artifact = a;
                break;
            }
        }
    }
    return artifact;
}

const ResolvedProduct *TrafoChangeTracker::getProduct(const QString &name) const
{
    if (m_lastProduct && name == m_lastProduct->uniqueName())
        return m_lastProduct;
    if (name == m_product->uniqueName()) {
        m_lastProduct = m_product;
        return m_product;
    }
    const auto it = m_productsByName.find(name);
    if (it != m_productsByName.cend()) {
        m_lastProduct = it->second;
        return it->second;
    }
    return nullptr;
}

bool TrafoChangeTracker::prepareScriptNeedsRerun() const
{
    for (const Property &property : qAsConst(m_transformer->propertiesRequestedInPrepareScript)) {
        if (checkForPropertyChange(property, propertyMapByKind(property)))
            return true;
    }

    if (checkForImportFileChange(m_transformer->importedFilesUsedInPrepareScript,
                                 m_transformer->lastPrepareScriptExecutionTime, "prepare script")) {
        return true;
    }

    for (auto it = m_transformer->propertiesRequestedFromArtifactInPrepareScript.constBegin();
         it != m_transformer->propertiesRequestedFromArtifactInPrepareScript.constEnd(); ++it) {
        for (const Property &property : qAsConst(it.value())) {
            const Artifact * const artifact = getArtifact(it.key(), property.productName);
            if (!artifact)
                return true;
            if (property.kind == Property::PropertyInArtifact) {
                if (sorted(artifact->fileTags().toStringList()) != property.value.toStringList())
                    return true;
                continue;
            }
            if (checkForPropertyChange(property, artifact->properties->value()))
                return true;
        }
    }

    if (!m_transformer->depsRequestedInPrepareScript.isUpToDate(m_product->topLevelProject()))
        return true;
    if (!m_transformer->artifactsMapRequestedInPrepareScript.isUpToDate(
                m_product->topLevelProject())) {
        return true;
    }
    if (!areExportedModulesUpToDate(m_transformer->exportedModulesAccessedInPrepareScript))
        return true;

    return false;
}

bool TrafoChangeTracker::commandsNeedRerun() const
{
    for (const Property &property : qAsConst(m_transformer->propertiesRequestedInCommands)) {
        if (checkForPropertyChange(property, propertyMapByKind(property)))
            return true;
    }

    QMap<QString, SourceArtifactConstPtr> artifactMap;
    for (auto it = m_transformer->propertiesRequestedFromArtifactInCommands.cbegin();
         it != m_transformer->propertiesRequestedFromArtifactInCommands.cend(); ++it) {
        for (const Property &property : qAsConst(it.value())) {
            const Artifact * const artifact = getArtifact(it.key(), property.productName);
            if (!artifact)
                return true;
            if (property.kind == Property::PropertyInArtifact) {
                if (sorted(artifact->fileTags().toStringList()) != property.value.toStringList())
                    return true;
                continue;
            }
            if (checkForPropertyChange(property, artifact->properties->value()))
                return true;
        }
    }

    if (checkForImportFileChange(m_transformer->importedFilesUsedInCommands,
                                 m_transformer->lastCommandExecutionTime, "command")) {
        return true;
    }

    if (!m_transformer->depsRequestedInCommands.isUpToDate(m_product->topLevelProject()))
        return true;
    if (!m_transformer->artifactsMapRequestedInCommands.isUpToDate(m_product->topLevelProject()))
        return true;
    if (!areExportedModulesUpToDate(m_transformer->exportedModulesAccessedInCommands))
        return true;

    // TODO: Also track env access in JS commands and prepare scripts
    for (const AbstractCommandPtr &c : qAsConst(m_transformer->commands.commands())) {
        if (c->type() != AbstractCommand::ProcessCommandType)
            continue;
        const ProcessCommandPtr &processCmd = std::static_pointer_cast<ProcessCommand>(c);
        const auto envVars = processCmd->relevantEnvVars();
        for (const QString &var : envVars) {
            const QString &oldValue = processCmd->relevantEnvValue(var);
            const QString &newValue = m_product->buildEnvironment.value(var);
            if (oldValue != newValue) {
                qCDebug(lcBuildGraph) << "need to re-run command: value of environment variable"
                                      << var << "changed from" << oldValue << "to" << newValue
                                      << "for command" << processCmd->program() << "in rule"
                                      << m_transformer->rule->toString();
                return true;
            }
        }
    }

    return false;
}

bool prepareScriptNeedsRerun(
        Transformer *transformer, const ResolvedProduct *product,
        const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
        const std::unordered_map<QString, const ResolvedProject *> &projectsByName)
{
    if (!transformer->prepareScriptNeedsChangeTracking)
        return false;
    transformer->prepareScriptNeedsChangeTracking = false;
    return TrafoChangeTracker(transformer, product, productsByName, projectsByName)
            .prepareScriptNeedsRerun();
}

bool commandsNeedRerun(Transformer *transformer, const ResolvedProduct *product,
                       const std::unordered_map<QString, const ResolvedProduct *> &productsByName,
                       const std::unordered_map<QString, const ResolvedProject *> &projectsByName)
{
    if (!transformer->commandsNeedChangeTracking)
        return false;
    transformer->commandsNeedChangeTracking = false;
    return TrafoChangeTracker(transformer, product, productsByName, projectsByName)
            .commandsNeedRerun();
}

} // namespace Internal
} // namespace qbs
