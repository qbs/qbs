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
    TrafoChangeTracker(const TransformerPtr &transformer,
                       const ResolvedProductPtr &product,
                       const QMap<QString, ResolvedProductPtr> &productsByName,
                       const QMap<QString, const ResolvedProject *> projectsByName)
        : m_transformer(transformer),
          m_product(product),
          m_productsByName(productsByName),
          m_projectsByName(projectsByName)
    {
    }

    bool propertyChangeFound();
    bool environmentChangeFound();

private:
    SourceArtifactConstPtr findSourceArtifact(const QString &artifactFilePath,
                                              QMap<QString, SourceArtifactConstPtr> &artifactMap);
    QVariantMap propertyMapByKind(const Property &property) const;
    void invalidateTransformer();
    bool checkForPropertyChange(const Property &restoredProperty,
                                const QVariantMap &newProperties) const;
    bool checkForImportFileChange(const std::vector<QString> &importedFiles,
                                  const FileTime &referenceTime,
                                  const char *context) const;

    const TransformerPtr &m_transformer;
    const ResolvedProductPtr &m_product;
    const QMap<QString, ResolvedProductPtr> &m_productsByName;
    const QMap<QString, const ResolvedProject *> m_projectsByName;
};

bool checkForPropertyChanges(const TransformerPtr &transformer,
                             const ResolvedProductPtr &product,
                             const QMap<QString, ResolvedProductPtr> &productsByName,
                             const QMap<QString, const ResolvedProject *> projectsByName)
{
    return TrafoChangeTracker(transformer, product, productsByName, projectsByName)
            .propertyChangeFound();
}

bool checkForEnvironmentChanges(const TransformerPtr &transformer,
                                const ResolvedProductPtr &product,
                                const QMap<QString, ResolvedProductPtr> &productsByName,
                                const QMap<QString, const ResolvedProject *> projectsByName)
{
    return TrafoChangeTracker(transformer, product, productsByName, projectsByName)
            .environmentChangeFound();
}

SourceArtifactConstPtr TrafoChangeTracker::findSourceArtifact(const QString &artifactFilePath,
        QMap<QString, SourceArtifactConstPtr> &artifactMap)
{
    SourceArtifactConstPtr &artifact = artifactMap[artifactFilePath];
    if (!artifact) {
        for (const SourceArtifactConstPtr &a : m_product->allFiles()) {
            if (a->absoluteFilePath == artifactFilePath) {
                artifact = a;
                break;
            }
        }
    }
    return artifact;
}

template<typename T> static QVariantMap getParameterValue(
        const QHash<T, QVariantMap> &parameters,
        const QString &depName)
{
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        if (it.key()->name == depName)
            return it.value();
    }
    return QVariantMap();
}

QVariantMap TrafoChangeTracker::propertyMapByKind(const Property &property) const
{
    const auto getProduct = [&property, this]() {
        return property.productName == m_product->uniqueName()
                ? m_product : m_productsByName.value(property.productName);
    };
    switch (property.kind) {
    case Property::PropertyInModule: {
        const ResolvedProductConstPtr &p = getProduct();
        return p ? p->moduleProperties->value() : QVariantMap();
    }
    case Property::PropertyInProduct: {
        const ResolvedProductConstPtr &p = getProduct();
        return p ? p->productProperties : QVariantMap();
    }
    case Property::PropertyInProject: {
        const ResolvedProject *project = property.productName == m_product->project->name
                ? m_product->project.get() : m_projectsByName.value(property.productName);
        return project ? project->projectProperties() : QVariantMap();
    }
    case Property::PropertyInParameters: {
        const int sepIndex = property.moduleName.indexOf(QLatin1Char(':'));
        const QString depName = property.moduleName.left(sepIndex);
        const ResolvedProductConstPtr &p = getProduct();
        if (!p)
            return QVariantMap();
        QVariantMap v = getParameterValue(p->dependencyParameters, depName);
        if (!v.empty())
            return v;
        return getParameterValue(p->moduleParameters, depName);
    }
    default:
        QBS_CHECK(false);
    }
    return QVariantMap();
}

void TrafoChangeTracker::invalidateTransformer()
{
    const JavaScriptCommandPtr &pseudoCommand = JavaScriptCommand::create();
    pseudoCommand->setSourceCode(QStringLiteral("random stuff that will cause "
                                                "commandsEqual() to fail"));
    m_transformer->commands << pseudoCommand;
}

bool TrafoChangeTracker::propertyChangeFound()
{
    // This check must come first, as it can prevent build data rescuing.
    for (const Property &property : qAsConst(m_transformer->propertiesRequestedInCommands)) {
        if (checkForPropertyChange(property, propertyMapByKind(property))) {
            invalidateTransformer();
            return true;
        }
    }

    QMap<QString, SourceArtifactConstPtr> artifactMap;
    for (auto it = m_transformer->propertiesRequestedFromArtifactInCommands.cbegin();
         it != m_transformer->propertiesRequestedFromArtifactInCommands.cend(); ++it) {
        const SourceArtifactConstPtr artifact
                = findSourceArtifact(it.key(), artifactMap);
        if (!artifact)
            continue;
        for (const Property &property : qAsConst(it.value())) {
            if (checkForPropertyChange(property, artifact->properties->value())) {
                invalidateTransformer();
                return true;
            }
        }
    }

    const FileTime referenceTime = m_transformer->product()->topLevelProject()->lastResolveTime; // TODO: This will need to be a transformer-specific timestamp
    if (checkForImportFileChange(m_transformer->importedFilesUsedInCommands, referenceTime,
                                 "command")) {
        invalidateTransformer();
        return true;
    }

    if (!m_transformer->depsRequestedInCommands.isUpToDate(m_product->topLevelProject())) {
        invalidateTransformer();
        return true;
    }

    for (const Property &property : qAsConst(m_transformer->propertiesRequestedInPrepareScript)) {
        if (checkForPropertyChange(property, propertyMapByKind(property)))
            return true;
    }

    if (checkForImportFileChange(m_transformer->importedFilesUsedInPrepareScript, referenceTime,
                                 "prepare script")) {
        return true;
    }

    for (auto it = m_transformer->propertiesRequestedFromArtifactInPrepareScript.constBegin();
         it != m_transformer->propertiesRequestedFromArtifactInPrepareScript.constEnd(); ++it) {
        const SourceArtifactConstPtr &artifact = findSourceArtifact(it.key(), artifactMap); // TODO: This can use the actual artifact data later
        if (!artifact)
            continue;
        for (const Property &property : qAsConst(it.value())) {
            if (checkForPropertyChange(property, artifact->properties->value()))
                return true;
        }
    }

    if (!m_transformer->depsRequestedInPrepareScript.isUpToDate(m_product->topLevelProject()))
        return true;

    return false;
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
    }
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
                                  << "has been updated, need to re-run";
            return true;
        }
    }
    return false;
}

bool TrafoChangeTracker::environmentChangeFound()
{
    // TODO: Also check results of getEnv() from commands here; we currently do not track them
    for (const AbstractCommandPtr &c : qAsConst(m_transformer->commands)) {
        if (c->type() != AbstractCommand::ProcessCommandType)
            continue;
        for (const QString &var : std::static_pointer_cast<ProcessCommand>(c)->relevantEnvVars()) {
            if (m_transformer->product()->topLevelProject()->environment.value(var) // TODO: These will need to get stored per transformer
                    != m_product->topLevelProject()->environment.value(var)) {
                invalidateTransformer();
                return true;
            }
        }
    }
    return false;
}

} // namespace Internal
} // namespace qbs
