/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "builtindeclarations.h"
#include "item.h"

namespace qbs {
namespace Internal {

const char QBS_LANGUAGE_VERSION[] = "1.0";

BuiltinDeclarations::BuiltinDeclarations()
    : m_languageVersion(QLatin1String(QBS_LANGUAGE_VERSION))
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

QString BuiltinDeclarations::languageVersion() const
{
    return m_languageVersion;
}

bool BuiltinDeclarations::containsType(const QString &typeName) const
{
    return m_builtins.contains(typeName);
}

QStringList BuiltinDeclarations::allTypeNames() const
{
    return m_builtins.keys();
}

ItemDeclaration BuiltinDeclarations::declarationsForType(const QString &typeName) const
{
    return m_builtins.value(typeName);
}

void BuiltinDeclarations::setupItemForBuiltinType(Item *item) const
{
    foreach (const PropertyDeclaration &pd, declarationsForType(item->typeName()).properties()) {
        item->m_propertyDeclarations.insert(pd.name(), pd);
        ValuePtr &value = item->m_properties[pd.name()];
        if (!value) {
            JSSourceValuePtr sourceValue = JSSourceValue::create();
            sourceValue->setFile(item->file());
            static const QString undefinedKeyword = QLatin1String("undefined");
            sourceValue->setSourceCode(pd.initialValueSource().isEmpty()
                                       ? QStringRef(&undefinedKeyword)
                                       : QStringRef(&pd.initialValueSource()));
            value = sourceValue;
        }
    }
}

void BuiltinDeclarations::insert(const ItemDeclaration &decl)
{
    m_builtins.insert(decl.typeName(), decl);
}

static PropertyDeclaration conditionProperty()
{
    PropertyDeclaration decl(QLatin1String("condition"), PropertyDeclaration::Boolean);
    decl.setInitialValueSource(QLatin1String("true"));
    return decl;
}

static PropertyDeclaration nameProperty()
{
    return PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String);
}

static PropertyDeclaration prepareScriptProperty()
{
    PropertyDeclaration decl(QLatin1String("prepare"), PropertyDeclaration::Verbatim);
    decl.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project") << QLatin1String("product")
                << QLatin1String("inputs") << QLatin1String("outputs")
                << QLatin1String("input") << QLatin1String("output"));
    return decl;
}

void BuiltinDeclarations::addArtifactItem()
{
    ItemDeclaration item(QLatin1String("Artifact"));
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("fileName"), PropertyDeclaration::Verbatim);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant);
    PropertyDeclaration decl(QLatin1String("alwaysUpdated"), PropertyDeclaration::Boolean);
    decl.setInitialValueSource(QLatin1String("true"));
    item << decl;
    insert(item);
}

void BuiltinDeclarations::addDependsItem()
{
    ItemDeclaration item(QLatin1String("Depends"));
    item << conditionProperty();
    item << nameProperty();
    item << PropertyDeclaration(QLatin1String("submodules"), PropertyDeclaration::Variant);
    PropertyDeclaration requiredDecl(QLatin1String("required"), PropertyDeclaration::Boolean);
    requiredDecl.setInitialValueSource(QLatin1String("true"));
    item << requiredDecl;
    insert(item);
}

void BuiltinDeclarations::addExportItem()
{
    ItemDeclaration item(QLatin1String("Export"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Depends"));
    insert(item);
}

void BuiltinDeclarations::addFileTaggerItem()
{
    ItemDeclaration item(QLatin1String("FileTagger"));

    // TODO: Remove in 1.3
    item << PropertyDeclaration(QLatin1String("pattern"), PropertyDeclaration::StringList);

    item << PropertyDeclaration(QLatin1String("patterns"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant);
    insert(item);
}

void BuiltinDeclarations::addGroupItem()
{
    ItemDeclaration item(QLatin1String("Group"));
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("files"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("fileTagsFilter"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("excludeFiles"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("prefix"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    PropertyDeclaration declaration;
    declaration.setName(QLatin1String("overrideTags"));
    declaration.setType(PropertyDeclaration::Boolean);
    declaration.setFlags(PropertyDeclaration::PropertyNotAvailableInConfig);
    declaration.setInitialValueSource(QLatin1String("true"));
    item << declaration;
    insert(item);
}

void BuiltinDeclarations::addModuleItem()
{
    ItemDeclaration item(QLatin1String("Module"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Depends")
            << QLatin1String("FileTagger")
            << QLatin1String("Rule")
            << QLatin1String("Probe")
            << QLatin1String("PropertyOptions")
            << QLatin1String("Transformer")
            << QLatin1String("Scanner"));
    item << nameProperty();
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("setupBuildEnvironment"),
                                      PropertyDeclaration::Verbatim);
    item << PropertyDeclaration(QLatin1String("setupRunEnvironment"),
                                      PropertyDeclaration::Verbatim);
    item << PropertyDeclaration(QLatin1String("validate"),
                                      PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("additionalProductFileTags"),
                                      PropertyDeclaration::Variant);
    PropertyDeclaration presentDecl(QLatin1String("present"), PropertyDeclaration::Boolean);
    presentDecl.setInitialValueSource(QLatin1String("true"));
    item << presentDecl;
    insert(item);
}

void BuiltinDeclarations::addProbeItem()
{
    ItemDeclaration item(QLatin1String("Probe"));
    item << conditionProperty();
    PropertyDeclaration foundProperty(QLatin1String("found"), PropertyDeclaration::Boolean);
    foundProperty.setInitialValueSource(QLatin1String("false"));
    item << foundProperty;
    item << PropertyDeclaration(QLatin1String("configure"), PropertyDeclaration::Verbatim);
    insert(item);
}

void BuiltinDeclarations::addProductItem()
{
    ItemDeclaration item(QLatin1String("Product"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Depends")
            << QLatin1String("Transformer")
            << QLatin1String("Group")
            << QLatin1String("FileTagger")
            << QLatin1String("Export")
            << QLatin1String("Probe")
            << QLatin1String("Rule"));
    item << conditionProperty();
    PropertyDeclaration decl(QLatin1String("type"), PropertyDeclaration::StringList);
    decl.setInitialValueSource(QLatin1String("[]"));
    item << decl;
    item << nameProperty();
    decl = PropertyDeclaration(QLatin1String("targetName"), PropertyDeclaration::String);
    decl.setInitialValueSource(QLatin1String("name"));
    item << decl;
    decl = PropertyDeclaration(QLatin1String("destinationDirectory"), PropertyDeclaration::String);
    decl.setInitialValueSource(QLatin1String("'.'"));
    item << decl;
    item << PropertyDeclaration(QLatin1String("consoleApplication"),
                                PropertyDeclaration::Boolean);
    item << PropertyDeclaration(QLatin1String("files"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("excludeFiles"), PropertyDeclaration::Variant,
                                PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("qbsSearchPaths"),
                                PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("version"), PropertyDeclaration::String);
    insert(item);
}

void BuiltinDeclarations::addProjectItem()
{
    ItemDeclaration item(QLatin1String("Project"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Project")
            << QLatin1String("SubProject")
            << QLatin1String("Product")
            << QLatin1String("FileTagger")
            << QLatin1String("Rule"));
    item << nameProperty();
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("references"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    item << PropertyDeclaration(QLatin1String("qbsSearchPaths"),
            PropertyDeclaration::StringList, PropertyDeclaration::PropertyNotAvailableInConfig);
    insert(item);
}

void BuiltinDeclarations::addPropertiesItem()
{
    insert(ItemDeclaration(QLatin1String("Properties")));
}

void BuiltinDeclarations::addPropertyOptionsItem()
{
    ItemDeclaration item(QLatin1String("PropertyOptions"));
    item << nameProperty();
    item << PropertyDeclaration(QLatin1String("allowedValues"), PropertyDeclaration::Variant);
    item << PropertyDeclaration(QLatin1String("description"), PropertyDeclaration::String);
    insert(item);
}

void BuiltinDeclarations::addRuleItem()
{
    ItemDeclaration item(QLatin1String("Rule"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Artifact"));
    item << conditionProperty();
    PropertyDeclaration decl(QLatin1String("multiplex"), PropertyDeclaration::Boolean);
    decl.setInitialValueSource(QLatin1String("false"));
    item << decl;
    item << PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String);
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("outputFileTags"), PropertyDeclaration::StringList);
    decl = PropertyDeclaration(QLatin1String("outputArtifacts"), PropertyDeclaration::Verbatim);
    decl.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project") << QLatin1String("product")
                << QLatin1String("inputs") << QLatin1String("input"));
    item << decl;
    item << PropertyDeclaration(QLatin1String("usings"), PropertyDeclaration::StringList);
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
    ItemDeclaration item(QLatin1String("SubProject"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Project") // needed, because we're adding this internally
            << QLatin1String("Properties"));
    item << PropertyDeclaration(QLatin1String("filePath"), PropertyDeclaration::Path);
    PropertyDeclaration inheritProperty;
    inheritProperty.setName(QLatin1String("inheritProperties"));
    inheritProperty.setType(PropertyDeclaration::Boolean);
    inheritProperty.setInitialValueSource(QLatin1String("true"));
    item << inheritProperty;
    insert(item);
}

void BuiltinDeclarations::addTransformerItem()
{
    ItemDeclaration item(QLatin1String("Transformer"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Artifact"));
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::Variant);
    item << prepareScriptProperty();
    item << PropertyDeclaration(QLatin1String("explicitlyDependsOn"),
                                      PropertyDeclaration::StringList);
    insert(item);
}

void BuiltinDeclarations::addScannerItem()
{
    ItemDeclaration item(QLatin1String("Scanner"));
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::StringList);
    PropertyDeclaration recursive(QLatin1String("recursive"), PropertyDeclaration::Boolean);
    recursive.setInitialValueSource(QLatin1String("false"));
    item << recursive;
    PropertyDeclaration searchPaths(QLatin1String("searchPaths"), PropertyDeclaration::Verbatim);
    searchPaths.setFunctionArgumentNames(
                QStringList()
                << QLatin1String("project")
                << QLatin1String("product")
                << QLatin1String("input"));
    item << searchPaths;
    PropertyDeclaration scan(QLatin1String("scan"), PropertyDeclaration::Verbatim);
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
