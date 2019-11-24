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
#include <tools/stringconstants.h>

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
        { QStringLiteral("Artifact"), ItemType::Artifact },
        { QStringLiteral("Depends"), ItemType::Depends },
        { QStringLiteral("Export"), ItemType::Export },
        { QStringLiteral("FileTagger"), ItemType::FileTagger },
        { QStringLiteral("Group"), ItemType::Group },
        { QStringLiteral("JobLimit"), ItemType::JobLimit },
        { QStringLiteral("Module"), ItemType::Module },
        { QStringLiteral("ModuleProvider"), ItemType::ModuleProvider },
        { QStringLiteral("Parameter"), ItemType::Parameter },
        { QStringLiteral("Parameters"), ItemType::Parameters },
        { QStringLiteral("Probe"), ItemType::Probe },
        { QStringLiteral("Product"), ItemType::Product },
        { QStringLiteral("Profile"), ItemType::Profile },
        { QStringLiteral("Project"), ItemType::Project },
        { QStringLiteral("Properties"), ItemType::Properties }, // Callers have to handle the SubProject case.
        { QStringLiteral("PropertyOptions"), ItemType::PropertyOptions },
        { QStringLiteral("Rule"), ItemType::Rule },
        { QStringLiteral("Scanner"), ItemType::Scanner },
        { QStringLiteral("SubProject"), ItemType::SubProject },
        { QStringLiteral("Transformer"), ItemType::Transformer }
    }))
{
    addArtifactItem();
    addDependsItem();
    addExportItem();
    addFileTaggerItem();
    addGroupItem();
    addJobLimitItem();
    addModuleItem();
    addModuleProviderItem();
    addProbeItem();
    addProductItem();
    addProfileItem();
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
                                          const CodeLocation &location) const
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
        return QStringLiteral("Properties");
    for (auto it = m_typeMap.constBegin(); it != m_typeMap.constEnd(); ++it) {
        if (it.value() == itemType)
            return it.key();
    }
    QBS_CHECK(false);
    return {};
}

QStringList BuiltinDeclarations::argumentNamesForScriptFunction(ItemType itemType,
                                                                const QString &scriptName) const
{
    const ItemDeclaration itemDecl = declarationsForType(itemType);
    const auto properties = itemDecl.properties();
    for (const PropertyDeclaration &propDecl : properties) {
        if (propDecl.name() == scriptName)
            return propDecl.functionArgumentNames();
    }
    QBS_CHECK(false);
    return {};
}

void BuiltinDeclarations::insert(const ItemDeclaration &decl)
{
    m_builtins.insert(decl.type(), decl);
}

static PropertyDeclaration conditionProperty()
{
    return PropertyDeclaration(StringConstants::conditionProperty(), PropertyDeclaration::Boolean,
                               StringConstants::trueValue());
}

static PropertyDeclaration alwaysRunProperty()
{
    return PropertyDeclaration(StringConstants::alwaysRunProperty(), PropertyDeclaration::Boolean,
                               StringConstants::falseValue());
}

static PropertyDeclaration nameProperty()
{
    return PropertyDeclaration(StringConstants::nameProperty(), PropertyDeclaration::String);
}

static PropertyDeclaration buildDirProperty()
{
    return PropertyDeclaration(StringConstants::buildDirectoryProperty(),
                               PropertyDeclaration::Path);
}

static PropertyDeclaration prepareScriptProperty()
{
    PropertyDeclaration decl(StringConstants::prepareProperty(), PropertyDeclaration::Variant,
                             QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    decl.setFunctionArgumentNames(
                QStringList()
                << StringConstants::projectVar() << StringConstants::productValue()
                << StringConstants::inputsVar() << StringConstants::outputsVar()
                << StringConstants::inputVar() << StringConstants::outputVar()
                << StringConstants::explicitlyDependsOnVar());
    return decl;
}

static PropertyDeclaration priorityProperty()
{
    return {StringConstants::priorityProperty(), PropertyDeclaration::Integer};
}

void BuiltinDeclarations::addArtifactItem()
{
    ItemDeclaration item(ItemType::Artifact);
    PropertyDeclaration conditionDecl = conditionProperty();
    conditionDecl.setDeprecationInfo(DeprecationInfo(Version(1, 4), Tr::tr("If you need "
            "dynamic artifacts, use the Rule.outputArtifacts script instead of Artifact items.")));
    item << conditionDecl;
    PropertyDeclaration fileNameDecl(StringConstants::fileNameProperty(),
                                     PropertyDeclaration::String);
    fileNameDecl.setDeprecationInfo(DeprecationInfo(Version(1, 4),
                                                    Tr::tr("Please use 'filePath' instead.")));
    item << fileNameDecl;
    item << PropertyDeclaration(StringConstants::filePathProperty(), PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::fileTagsProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::alwaysUpdatedProperty(),
                                PropertyDeclaration::Boolean, StringConstants::trueValue());
    insert(item);
}

void BuiltinDeclarations::addDependsItem()
{
    ItemDeclaration item(ItemType::Depends);
    item << conditionProperty();
    item << nameProperty();
    item << PropertyDeclaration(StringConstants::submodulesProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::requiredProperty(), PropertyDeclaration::Boolean,
                                StringConstants::trueValue());
    item << PropertyDeclaration(StringConstants::versionAtLeastProperty(),
                                PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::versionBelowProperty(),
                                PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::profilesProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::productTypesProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::limitToSubProjectProperty(),
                                PropertyDeclaration::Boolean, StringConstants::falseValue());
    item << PropertyDeclaration(StringConstants::multiplexConfigurationIdsProperty(),
                                PropertyDeclaration::StringList, QString(),
                                PropertyDeclaration::ReadOnlyFlag);
    item << PropertyDeclaration(StringConstants::enableFallbackProperty(),
                                PropertyDeclaration::Boolean, StringConstants::trueValue());
    insert(item);
}

void BuiltinDeclarations::addExportItem()
{
    ItemDeclaration item = moduleLikeItem(ItemType::Export);
    item << PropertyDeclaration(StringConstants::prefixMappingProperty(),
                                PropertyDeclaration::Variant);
    auto allowedChildTypes = item.allowedChildTypes();
    allowedChildTypes.insert(ItemType::Parameters);
    allowedChildTypes.insert(ItemType::Properties);
    item.setAllowedChildTypes(allowedChildTypes);
    insert(item);
}

void BuiltinDeclarations::addFileTaggerItem()
{
    ItemDeclaration item(ItemType::FileTagger);
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::patternsProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::fileTagsProperty(),
                                PropertyDeclaration::StringList);
    item << priorityProperty();
    insert(item);
}

void BuiltinDeclarations::addGroupItem()
{
    ItemDeclaration item(ItemType::Group);
    item.setAllowedChildTypes({ ItemType::Group });
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::nameProperty(), PropertyDeclaration::String,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::filesProperty(), PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::fileTagsFilterProperty(),
                                PropertyDeclaration::StringList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::excludeFilesProperty(),
                                PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::fileTagsProperty(),
                                PropertyDeclaration::StringList, QString(),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::prefixProperty(), PropertyDeclaration::String,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::overrideTagsProperty(),
                                PropertyDeclaration::Boolean, StringConstants::trueValue(),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::filesAreTargetsProperty(),
                                PropertyDeclaration::Boolean, StringConstants::falseValue(),
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    insert(item);
}

void BuiltinDeclarations::addJobLimitItem()
{
    ItemDeclaration item(ItemType::JobLimit);
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::jobPoolProperty(), PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::jobCountProperty(), PropertyDeclaration::Integer);
    insert(item);
}

void BuiltinDeclarations::addModuleItem()
{
    ItemDeclaration item = moduleLikeItem(ItemType::Module);
    item << priorityProperty();
    insert(item);
}

void BuiltinDeclarations::addModuleProviderItem()
{
    ItemDeclaration item(ItemType::ModuleProvider);
    item << nameProperty()
         << PropertyDeclaration(QStringLiteral("outputBaseDir"), PropertyDeclaration::String)
         << PropertyDeclaration(QStringLiteral("relativeSearchPaths"),
                                PropertyDeclaration::StringList);
    insert(item);
}

ItemDeclaration BuiltinDeclarations::moduleLikeItem(ItemType type)
{
    ItemDeclaration item(type);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Group
            << ItemType::Depends
            << ItemType::FileTagger
            << ItemType::JobLimit
            << ItemType::Rule
            << ItemType::Parameter
            << ItemType::Probe
            << ItemType::PropertyOptions
            << ItemType::Scanner);
    item << nameProperty();
    item << conditionProperty();
    PropertyDeclaration setupBuildEnvDecl(StringConstants::setupBuildEnvironmentProperty(),
                                          PropertyDeclaration::Variant, QString(),
                                          PropertyDeclaration::PropertyNotAvailableInConfig);
    setupBuildEnvDecl.setFunctionArgumentNames(QStringList{StringConstants::projectVar(),
                                                           StringConstants::productVar()});
    item << setupBuildEnvDecl;
    PropertyDeclaration setupRunEnvDecl(StringConstants::setupRunEnvironmentProperty(),
                                        PropertyDeclaration::Variant, QString(),
                             PropertyDeclaration::PropertyNotAvailableInConfig);
    setupRunEnvDecl.setFunctionArgumentNames(QStringList{StringConstants::projectVar(),
                                                         StringConstants::productVar()});
    item << setupRunEnvDecl;
    item << PropertyDeclaration(StringConstants::validateProperty(),
                                      PropertyDeclaration::Boolean, QString(),
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::additionalProductTypesProperty(),
                                      PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::versionProperty(), PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::presentProperty(), PropertyDeclaration::Boolean,
                                StringConstants::trueValue());
    return item;
}

void BuiltinDeclarations::addProbeItem()
{
    ItemDeclaration item(ItemType::Probe);
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::foundProperty(), PropertyDeclaration::Boolean,
                                StringConstants::falseValue());
    item << PropertyDeclaration(StringConstants::configureProperty(), PropertyDeclaration::Variant,
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
            << ItemType::JobLimit
            << ItemType::Export
            << ItemType::Probe
            << ItemType::Profile
            << ItemType::PropertyOptions
            << ItemType::Rule);
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::typeProperty(), PropertyDeclaration::StringList,
                                StringConstants::emptyArrayValue());
    item << nameProperty();
    item << PropertyDeclaration(StringConstants::builtByDefaultProperty(),
                                PropertyDeclaration::Boolean, StringConstants::trueValue());
    PropertyDeclaration profilesDecl(StringConstants::profilesProperty(),
                                     PropertyDeclaration::StringList);
    profilesDecl.setDeprecationInfo(DeprecationInfo(Version::fromString(QStringLiteral("1.9.0")),
                                                    Tr::tr("Use qbs.profiles instead.")));
    item << profilesDecl;
    item << PropertyDeclaration(StringConstants::targetNameProperty(), PropertyDeclaration::String,
                                QStringLiteral("new String(name)"
                                              ".replace(/[/\\\\?%*:|\"<>]/g, '_').valueOf()"));
    item << buildDirProperty();
    item << PropertyDeclaration(StringConstants::destinationDirProperty(),
                                PropertyDeclaration::String,
                                StringConstants::buildDirectoryProperty());
    item << PropertyDeclaration(StringConstants::consoleApplicationProperty(),
                                PropertyDeclaration::Boolean);
    item << PropertyDeclaration(StringConstants::filesProperty(), PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::excludeFilesProperty(),
                                PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::qbsSearchPathsProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::versionProperty(), PropertyDeclaration::String);

    item << PropertyDeclaration(StringConstants::multiplexByQbsPropertiesProperty(),
                                PropertyDeclaration::StringList, QStringLiteral("[\"profiles\"]"));
    item << PropertyDeclaration(StringConstants::multiplexedTypeProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::aggregateProperty(), PropertyDeclaration::Boolean);
    item << PropertyDeclaration(StringConstants::multiplexedProperty(),
                                PropertyDeclaration::Boolean,
                                QString(), PropertyDeclaration::ReadOnlyFlag);
    item << PropertyDeclaration(StringConstants::multiplexConfigurationIdProperty(),
                                PropertyDeclaration::String, QString(),
                                PropertyDeclaration::ReadOnlyFlag);
    insert(item);
}

void BuiltinDeclarations::addProfileItem()
{
    ItemDeclaration item(ItemType::Profile);
    item << conditionProperty();
    item << nameProperty();
    item << PropertyDeclaration(StringConstants::baseProfileProperty(),
                                PropertyDeclaration::String);
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
            << ItemType::Profile
            << ItemType::Probe
            << ItemType::FileTagger
            << ItemType::JobLimit
            << ItemType::Rule);
    item << nameProperty();
    item << conditionProperty();
    item << buildDirProperty();
    item << PropertyDeclaration(StringConstants::minimumQbsVersionProperty(),
                                PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::sourceDirectoryProperty(),
                                PropertyDeclaration::Path);
    item << PropertyDeclaration(StringConstants::profileProperty(), PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::referencesProperty(),
                                PropertyDeclaration::PathList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(StringConstants::qbsSearchPathsProperty(),
                                PropertyDeclaration::StringList,
                                QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    insert(item);
}

void BuiltinDeclarations::addPropertiesItem()
{
    ItemDeclaration item(ItemType::Properties);
    item << conditionProperty();
    insert(item);
}

void BuiltinDeclarations::addPropertyOptionsItem()
{
    ItemDeclaration item(ItemType::PropertyOptions);
    item << nameProperty();
    item << PropertyDeclaration(StringConstants::allowedValuesProperty(),
                                PropertyDeclaration::Variant);
    item << PropertyDeclaration(StringConstants::descriptionProperty(),
                                PropertyDeclaration::String);
    item << PropertyDeclaration(StringConstants::removalVersionProperty(),
                                PropertyDeclaration::String);
    insert(item);
}

void BuiltinDeclarations::addRuleItem()
{
    ItemDeclaration item(ItemType::Rule);
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << ItemType::Artifact);
    item << conditionProperty();
    item << alwaysRunProperty();
    item << PropertyDeclaration(StringConstants::multiplexProperty(), PropertyDeclaration::Boolean,
                                StringConstants::falseValue());
    item << PropertyDeclaration(StringConstants::requiresInputsProperty(),
                                PropertyDeclaration::Boolean);
    item << nameProperty();
    item << PropertyDeclaration(StringConstants::inputsProperty(), PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::outputFileTagsProperty(),
                                PropertyDeclaration::StringList);
    PropertyDeclaration outputArtifactsDecl(StringConstants::outputArtifactsProperty(),
                                            PropertyDeclaration::Variant, QString(),
                                            PropertyDeclaration::PropertyNotAvailableInConfig);
    outputArtifactsDecl.setFunctionArgumentNames(
                QStringList()
                << StringConstants::projectVar() << StringConstants::productVar()
                << StringConstants::inputsVar() << StringConstants::inputVar());
    item << outputArtifactsDecl;
    PropertyDeclaration usingsDecl(QStringLiteral("usings"), PropertyDeclaration::StringList);
    usingsDecl.setDeprecationInfo(DeprecationInfo(Version(1, 5),
                                                  Tr::tr("Use 'inputsFromDependencies' instead")));
    item << usingsDecl;
    item << PropertyDeclaration(StringConstants::inputsFromDependenciesProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::auxiliaryInputsProperty(),
                                      PropertyDeclaration::StringList);
    PropertyDeclaration excludedAuxInputs(StringConstants::excludedAuxiliaryInputsProperty(),
                                          PropertyDeclaration::StringList);
    excludedAuxInputs.setDeprecationInfo(DeprecationInfo(Version(1, 14),
                                                         Tr::tr("Use 'excludedInputs' instead")));
    item << excludedAuxInputs;
    item << PropertyDeclaration(StringConstants::excludedInputsProperty(),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::explicitlyDependsOnProperty(),
                                      PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::explicitlyDependsOnFromDependenciesProperty(),
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
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::filePathProperty(), PropertyDeclaration::Path);
    item << PropertyDeclaration(StringConstants::inheritPropertiesProperty(),
                                PropertyDeclaration::Boolean,
                                StringConstants::trueValue());
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
    item << PropertyDeclaration(StringConstants::inputsProperty(), PropertyDeclaration::PathList);
    item << prepareScriptProperty();
    item << PropertyDeclaration(StringConstants::explicitlyDependsOnProperty(),
                                      PropertyDeclaration::StringList);
    insert(item);
}

void BuiltinDeclarations::addScannerItem()
{
    ItemDeclaration item(ItemType::Scanner);
    item << conditionProperty();
    item << PropertyDeclaration(StringConstants::inputsProperty(), PropertyDeclaration::StringList);
    item << PropertyDeclaration(StringConstants::recursiveProperty(), PropertyDeclaration::Boolean,
                                StringConstants::falseValue());
    PropertyDeclaration searchPaths(StringConstants::searchPathsProperty(),
                                    PropertyDeclaration::StringList);
    searchPaths.setFunctionArgumentNames(
                QStringList()
                << StringConstants::projectVar()
                << StringConstants::productVar()
                << StringConstants::inputVar());
    item << searchPaths;
    PropertyDeclaration scan(StringConstants::scanProperty(), PropertyDeclaration::Variant,
                             QString(), PropertyDeclaration::PropertyNotAvailableInConfig);
    scan.setFunctionArgumentNames(
                QStringList()
                << StringConstants::projectVar()
                << StringConstants::productVar()
                << StringConstants::inputVar()
                << StringConstants::filePathVar());
    item << scan;
    insert(item);
}

} // namespace Internal
} // namespace qbs
