/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "builtindeclarations.h"

#include "deprecationinfo.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>

#include <QtCore/qstringlist.h>

#include <initializer_list>
#include <utility>

namespace qbs {
namespace Internal {

class AClassWithPublicConstructor : public BuiltinDeclarations { };
Q_GLOBAL_STATIC(AClassWithPublicConstructor, theInstance)

const char QBS_LANGUAGE_VERSION[] = "1.0";

BuiltinDeclarations::BuiltinDeclarations()
    : m_languageVersion(Version::fromString(QLatin1String(QBS_LANGUAGE_VERSION)))
    , m_typeMap(std::initializer_list<std::pair<QString, ItemType>>({
        { QLatin1String("Artifact"), ItemType::Artifact },
        { QLatin1String("Depends"), ItemType::Depends },
        { QLatin1String("Export"), ItemType::Export },
        { QLatin1String("FileTagger"), ItemType::FileTagger },
        { QLatin1String("Group"), ItemType::Group },
        { QLatin1String("Module"), ItemType::Module },
        { QLatin1String("Parameter"), ItemType::Parameter },
        { QLatin1String("Parameters"), ItemType::Parameters },
        { QLatin1String("Probe"), ItemType::Probe },
        { QLatin1String("Product"), ItemType::Product },
        { QLatin1String("Project"), ItemType::Project },
        { QLatin1String("Properties"), ItemType::Properties }, // Callers have to handle the SubProject case.
        { QLatin1String("PropertyOptions"), ItemType::PropertyOptions },
        { QLatin1String("Rule"), ItemType::Rule },
        { QLatin1String("Scanner"), ItemType::Scanner },
        { QLatin1String("SubProject"), ItemType::SubProject },
        { QLatin1String("Transformer"), ItemType::Transformer }
    }))
{
    addArtifactItem();
    addDependsItem();
    addExportItem();
    addFileTaggerItem();
    addGroupItem();
    addModuleItem();
    addProbeItem();
    addProductItem();
    addProjectItem();
    addPropertiesItem();
    addPropertyOptionsItem();
    addRuleItem();
    addSubprojectItem();
    addTransformerItem();
    addScannerItem();
}

const BuiltinDeclarations &BuiltinDeclarations::instance()
{
    return *theInstance;
}

Version BuiltinDeclarations::languageVersion() const
{
    return m_languageVersion;
}

QStringList BuiltinDeclarations::allTypeNames() const
{
    return m_typeMap.keys();
}

ItemDeclaration BuiltinDeclarations::declarationsForType(ItemType type) const
{
    return m_builtins.value(type);
}

ItemType BuiltinDeclarations::typeForName(const QString &typeName,
                                          const CodeLocation location) const
{
    const auto it = m_typeMap.constFind(typeName);
    if (it == m_typeMap.constEnd())
        throw ErrorInfo(Tr::tr("Unexpected item type '%1'.").arg(typeName), location);
    return it.value();
}

QString BuiltinDeclarations::nameForType(ItemType itemType) const
{
    // Iterating is okay here, as this mapping is not used in hot code paths.
    if (itemType == ItemType::PropertiesInSubProject)
        return QLatin1String("Properties");
    for (auto it = m_typeMap.constBegin(); it != m_typeMap.constEnd(); ++it) {
        if (it.value() == itemType)
            return it.key();
    }
    QBS_CHECK(false);
    return QString();
}

void BuiltinDeclarations::insert(const ItemDeclaration &decl)
{
    m_builtins.insert(decl.type(), decl);
}

static PropertyDeclaration conditionProperty()
{
    return PropertyDeclaration(QLatin1String("condition"), PropertyDeclaration::Boolean,
                               QLatin1String("true"));
}

static PropertyDeclaration alwaysRunProperty()
{
    return PropertyDeclaration(QLatin1String("alwaysRun"), PropertyDeclaration::Boolean,
                               QLatin1String("false"));
}

static PropertyDeclaration nameProperty()
{
    return PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String);
}

static PropertyDeclaration buildDirProperty()
{
    return PropertyDeclaration(QLatin1String("buildDirectory"), PropertyDeclaration::Path);
}

static PropertyDeclaration prepareScriptProperty()
{
    PropertyDeclaration decl(QLatin1String("prepare"), PropertyDeclaration::Variant, QString(),
                             PropertyDeclaration::PropertyNotAvailableInConfig);
    decl.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project") << QLatin1String("product")
                << QLatin1String("inputs") << QLatin1String("outputs")
                << QLatin1String("input") << QLatin1String("output")
                << QLatin1String("explicitlyDependsOn"));
    return decl;
}

void BuiltinDeclarations::addArtifactItem()
{
    ItemDeclaration item(ItemType::Artifact);
    PropertyDeclaration conditionDecl = conditionProperty();
    conditionDecl.setDeprecationInfo(DeprecationInfo(Version(1, 4), Tr::tr("If you need "
            "dynamic artifacts, use the Rule.outputArtifacts script instead of Artifact items.")));
    item << conditionDecl;
    PropertyDeclaration fileNameDecl(QLatin1String("fileName"), PropertyDeclaration::String);
    fileNameDecl.setDeprecationInfo(DeprecationInfo(Version(1, 4),
                                                    Tr::tr("Please use 'filePath' instead.")));
    item << fileNameDecl;
    item << PropertyDeclaration(QLatin1String("filePath"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("alwaysUpdated"), PropertyDeclaration::Boolean,
                                QLatin1String("true"));
    insert(item);
}

void BuiltinDeclarations::addDependsItem()
{
    ItemDeclaration item(ItemType::Depends);
    item << conditionProperty();
    item << nameProperty();
    item << PropertyDeclaration(QLatin1String("submodules"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("required"), PropertyDeclaration::Boolean,
                                QLatin1String("true"));
    item << PropertyDeclaration(QLatin1String("versionAtLeast"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("versionBelow"), PropertyDeclaration::String);
    PropertyDeclaration profileDecl(QLatin1String("profiles"), PropertyDeclaration::StringList);
    profileDecl.setInitialValueSource(QLatin1String("[product.profile]"));
    item << profileDecl;
    item << PropertyDeclaration(QLatin1String("productTypes"), PropertyDeclaration::StringList);
    PropertyDeclaration limitDecl(QLatin1String("limitToSubProject"), PropertyDeclaration::Boolean);
    limitDecl.setInitialValueSource(QLatin1String("false"));
    item << limitDecl;
    item << PropertyDeclaration(QLatin1String("multiplexConfigurationId"),
                                PropertyDeclaration::String, QString(),
                                PropertyDeclaration::ReadOnlyFlag);
    insert(item);
}

void BuiltinDeclarations::addExportItem()
{
    ItemDeclaration item = moduleLikeItem(ItemType::Export);
    auto allowedChildTypes = item.allowedChildTypes();
    allowedChildTypes.insert(ItemType::Parameters);
    item.setAllowedChildTypes(allowedChildTypes);
    insert(item);
}

void BuiltinDeclarations::addFileTaggerItem()
{
    ItemDeclaration item(ItemType::FileTagger);
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("patterns"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::StringList);
    insert(item);
}

void BuiltinDeclarations::addGroupItem()
{
    ItemDeclaration item(ItemType::Group);
    item.setAllowedChildTypes({ ItemType::Group });
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String, QString(),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("files"), PropertyDeclaration::PathList, QString(),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("fileTagsFilter"), PropertyDeclaration::StringList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("excludeFiles"), PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::StringList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("prefix"), PropertyDeclaration::String,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("overrideTags"), PropertyDeclaration::Boolean,
                                QLatin1String("true"),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    insert(item);
}

void BuiltinDeclarations::addModuleItem()
{
    insert(moduleLikeItem(ItemType::Module));
}

ItemDeclaration BuiltinDeclarations::moduleLikeItem(ItemType type)
{
    ItemDeclaration item(type);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Group
            << ItemType::Depends
            << ItemType::FileTagger
            << ItemType::Rule
            << ItemType::Parameter
            << ItemType::Probe
            << ItemType::PropertyOptions
            << ItemType::Scanner);
    item << nameProperty();
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("setupBuildEnvironment"),
                                      PropertyDeclaration::Variant, QString(),
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("setupRunEnvironment"),
                                      PropertyDeclaration::Variant, QString(),
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("validate"),
                                      PropertyDeclaration::Boolean, QString(),
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("additionalProductTypes"),
                                      PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("version"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("present"), PropertyDeclaration::Boolean,
                                QLatin1String("true"));
    return item;
}

void BuiltinDeclarations::addProbeItem()
{
    ItemDeclaration item(ItemType::Probe);
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("found"), PropertyDeclaration::Boolean,
                                QLatin1String("false"));
    item << PropertyDeclaration(QLatin1String("configure"), PropertyDeclaration::Variant,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    insert(item);
}

void BuiltinDeclarations::addProductItem()
{
    ItemDeclaration item(ItemType::Product);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Depends
            << ItemType::Group
            << ItemType::FileTagger
            << ItemType::Export
            << ItemType::Probe
            << ItemType::PropertyOptions
            << ItemType::Rule);
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("type"), PropertyDeclaration::StringList,
                                QLatin1String("[]"));
    item << nameProperty();
    item << PropertyDeclaration(QLatin1String("builtByDefault"), PropertyDeclaration::Boolean,
                                QLatin1String("true"));
    PropertyDeclaration profilesDecl(QLatin1String("profiles"), PropertyDeclaration::StringList);
    profilesDecl.setDeprecationInfo(DeprecationInfo(Version::fromString(QLatin1String("1.9.0")),
                                                    Tr::tr("Use qbs.profiles instead.")));
    item << profilesDecl;
    item << PropertyDeclaration(QLatin1String("profile"), PropertyDeclaration::String,
                                QLatin1String("project.profile")); // Internal
    item << PropertyDeclaration(QLatin1String("targetName"), PropertyDeclaration::String,
                                QLatin1String("new String(name)"
                                              ".replace(/[/\\\\?%*:|\"<>]/g, '_').valueOf()"));
    item << buildDirProperty();
    item << PropertyDeclaration(QLatin1String("destinationDirectory"), PropertyDeclaration::String,
                                QStringLiteral("buildDirectory"));
    item << PropertyDeclaration(QLatin1String("consoleApplication"),
                                PropertyDeclaration::Boolean);
    item << PropertyDeclaration(QLatin1String("files"), PropertyDeclaration::PathList, QString(),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("excludeFiles"), PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("qbsSearchPaths"),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("version"), PropertyDeclaration::String);

    item << PropertyDeclaration(QLatin1String("multiplexByQbsProperties"),
                                PropertyDeclaration::StringList, QLatin1String("[\"profiles\"]"));
    item << PropertyDeclaration(QLatin1String("multiplexedType"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("aggregate"), PropertyDeclaration::Boolean);
    item << PropertyDeclaration(QLatin1String("multiplexed"), PropertyDeclaration::Boolean,
                                QString(), PropertyDeclaration::ReadOnlyFlag);
    item << PropertyDeclaration(QLatin1String("multiplexConfigurationId"),
                                PropertyDeclaration::String, QString(),
                                PropertyDeclaration::ReadOnlyFlag);
    insert(item);
}

void BuiltinDeclarations::addProjectItem()
{
    ItemDeclaration item(ItemType::Project);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Project
            << ItemType::PropertyOptions
            << ItemType::SubProject
            << ItemType::Product
            << ItemType::Probe
            << ItemType::FileTagger
            << ItemType::Rule);
    item << nameProperty();
    item << conditionProperty();
    item << buildDirProperty();
    item << PropertyDeclaration(QLatin1String("minimumQbsVersion"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("sourceDirectory"), PropertyDeclaration::Path);
    item << PropertyDeclaration(QLatin1String("profile"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("references"), PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("qbsSearchPaths"), PropertyDeclaration::StringList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    insert(item);
}

void BuiltinDeclarations::addPropertiesItem()
{
    insert(ItemDeclaration(ItemType::Properties));
}

void BuiltinDeclarations::addPropertyOptionsItem()
{
    ItemDeclaration item(ItemType::PropertyOptions);
    item << nameProperty();
    item << PropertyDeclaration(QLatin1String("allowedValues"), PropertyDeclaration::Variant);
    item << PropertyDeclaration(QLatin1String("description"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("removalVersion"), PropertyDeclaration::String);
    insert(item);
}

void BuiltinDeclarations::addRuleItem()
{
    ItemDeclaration item(ItemType::Rule);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Artifact);
    item << conditionProperty();
    item << alwaysRunProperty();
    item << PropertyDeclaration(QLatin1String("multiplex"), PropertyDeclaration::Boolean,
                                QLatin1String("false"));
    item << PropertyDeclaration(QLatin1String("requiresInputs"), PropertyDeclaration::Boolean);
    item << PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("outputFileTags"), PropertyDeclaration::StringList);
    PropertyDeclaration outputArtifactsDecl(QLatin1String("outputArtifacts"),
                                            PropertyDeclaration::Variant, QString(),
                                            PropertyDeclaration::PropertyNotAvailableInConfig);
    outputArtifactsDecl.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project") << QLatin1String("product")
                << QLatin1String("inputs") << QLatin1String("input"));
    item << outputArtifactsDecl;
    PropertyDeclaration usingsDecl(QLatin1String("usings"), PropertyDeclaration::StringList);
    usingsDecl.setDeprecationInfo(DeprecationInfo(Version(1, 5),
                                                  Tr::tr("Use 'inputsFromDependencies' instead")));
    item << usingsDecl;
    item << PropertyDeclaration(QLatin1String("inputsFromDependencies"),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("auxiliaryInputs"),
                                      PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("excludedAuxiliaryInputs"),
                                      PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("explicitlyDependsOn"),
                                      PropertyDeclaration::StringList);
    item << prepareScriptProperty();
    insert(item);
}

void BuiltinDeclarations::addSubprojectItem()
{
    ItemDeclaration item(ItemType::SubProject);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Project // needed, because we're adding this internally
            << ItemType::PropertiesInSubProject
            << ItemType::PropertyOptions);
    item << PropertyDeclaration(QLatin1String("filePath"), PropertyDeclaration::Path);
    item << PropertyDeclaration(QLatin1String("inheritProperties"), PropertyDeclaration::Boolean,
                                QLatin1String("true"));
    insert(item);
}

void BuiltinDeclarations::addTransformerItem()
{
    ItemDeclaration item(ItemType::Transformer);
    item.setDeprecationInfo(DeprecationInfo(Version(1, 7), Tr::tr("Use the 'Rule' item instead.")));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Artifact);
    item << conditionProperty();
    item << alwaysRunProperty();
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::PathList);
    item << prepareScriptProperty();
    item << PropertyDeclaration(QLatin1String("explicitlyDependsOn"),
                                      PropertyDeclaration::StringList);
    insert(item);
}

void BuiltinDeclarations::addScannerItem()
{
    ItemDeclaration item(ItemType::Scanner);
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("recursive"), PropertyDeclaration::Boolean,
                                QLatin1String("false"));
    PropertyDeclaration searchPaths(QLatin1String("searchPaths"), PropertyDeclaration::StringList);
    searchPaths.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project")
                << QLatin1String("product")
                << QLatin1String("input"));
    item << searchPaths;
    PropertyDeclaration scan(QLatin1String("scan"), PropertyDeclaration::Variant, QString(),
                             PropertyDeclaration::PropertyNotAvailableInConfig);
    scan.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project")
                << QLatin1String("product")
                << QLatin1String("input"));
    item << scan;
    insert(item);
}

} // namespace Internal
} // namespace qbs
