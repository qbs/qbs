/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
}

QString BuiltinDeclarations::languageVersion() const
{
    return m_languageVersion;
}

QByteArray BuiltinDeclarations::qmlTypeInfo() const
{
    // Header:
    QByteArray result;
    result.append("import QtQuick.tooling 1.0\n\n");
    result.append("// This file describes the plugin-supplied types contained in the library.\n");
    result.append("// It is used for QML tooling purposes only.\n\n");
    result.append("Module {\n");

    // Individual Components:
    foreach (const QString &component, m_builtins.keys()) {
        QByteArray componentName = component.toUtf8();
        result.append("    Component {\n");
        result.append(QByteArray("        name: \"") + componentName + QByteArray("\"\n"));
        result.append("        exports: [ \"qbs/");
        result.append(componentName);
        result.append(" ");
        result.append(QBS_LANGUAGE_VERSION);
        result.append("\" ]\n");
        result.append("        prototype: \"QQuickItem\"\n");

        ItemDeclaration itemDecl = m_builtins.value(component);
        foreach (const PropertyDeclaration &property, itemDecl.properties()) {
            result.append("        Property { name=\"");
            result.append(property.name.toUtf8());
            result.append("\"; ");
            switch (property.type) {
            case qbs::Internal::PropertyDeclaration::UnknownType:
                result.append("type=\"unknown\"");
                break;
            case qbs::Internal::PropertyDeclaration::Boolean:
                result.append("type=\"bool\"");
                break;
            case qbs::Internal::PropertyDeclaration::Integer:
                result.append("type=\"int\"");
                break;
            case qbs::Internal::PropertyDeclaration::Path:
                result.append("type=\"string\"");
                break;
            case qbs::Internal::PropertyDeclaration::PathList:
                result.append("type=\"string\"; isList=true");
                break;
            case qbs::Internal::PropertyDeclaration::String:
                result.append("type=\"string\"");
                break;
            case qbs::Internal::PropertyDeclaration::StringList:
                result.append("type=\"string\"; isList=true");
                break;
            case qbs::Internal::PropertyDeclaration::Variant:
                result.append("type=\"QVariant\"");
                break;
            case qbs::Internal::PropertyDeclaration::Verbatim:
                result.append("type=\"string\"");
                break;
            }
            result.append(" }\n"); // Property
        }

        result.append("    }\n"); // Component
    }

    // Footer:
    result.append("}\n"); // Module
    return result;
}

bool BuiltinDeclarations::containsType(const QString &typeName) const
{
    return m_builtins.contains(typeName);
}

ItemDeclaration BuiltinDeclarations::declarationsForType(const QString &typeName) const
{
    return m_builtins.value(typeName);
}

void BuiltinDeclarations::setupItemForBuiltinType(Item *item) const
{
    foreach (const PropertyDeclaration &pd, declarationsForType(item->typeName()).properties()) {
        item->m_propertyDeclarations.insert(pd.name, pd);
        ValuePtr &value = item->m_properties[pd.name];
        if (!value) {
            JSSourceValuePtr sourceValue = JSSourceValue::create();
            sourceValue->setFile(item->file());
            sourceValue->setSourceCode(pd.initialValueSource.isEmpty() ?
                                           "undefined" : pd.initialValueSource);
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
    decl.initialValueSource = QLatin1String("true");
    return decl;
}

static PropertyDeclaration nameProperty()
{
    return PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String);
}

static PropertyDeclaration prepareScriptProperty()
{
    PropertyDeclaration decl(QLatin1String("prepare"), PropertyDeclaration::Verbatim);
    decl.functionArgumentNames
            << QLatin1String("project") << QLatin1String("product")
            << QLatin1String("inputs") << QLatin1String("outputs")
            << QLatin1String("input") << QLatin1String("output");
    return decl;
}

void BuiltinDeclarations::addArtifactItem()
{
    ItemDeclaration item(QLatin1String("Artifact"));
    item << conditionProperty();
    item << PropertyDeclaration(QLatin1String("fileName"), PropertyDeclaration::Verbatim);
    item << PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant);
    PropertyDeclaration decl(QLatin1String("alwaysUpdated"), PropertyDeclaration::Boolean);
    decl.initialValueSource = QLatin1String("true");
    item << decl;
    insert(item);
}

void BuiltinDeclarations::addDependsItem()
{
    ItemDeclaration item(QLatin1String("Depends"));
    item << conditionProperty();
    item << nameProperty();
    item << PropertyDeclaration(QLatin1String("submodules"), PropertyDeclaration::Variant);
    item << PropertyDeclaration(QLatin1String("required"), PropertyDeclaration::Boolean);
    item << PropertyDeclaration(QLatin1String("failureMessage"), PropertyDeclaration::String);
    insert(item);
}

void BuiltinDeclarations::addExportItem()
{
    ItemDeclaration item(QLatin1String("Export"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Depends")
            << QLatin1String("Module")  // needed, because we're adding module instances internally
                              );
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
    declaration.name = QLatin1String("overrideTags");
    declaration.type = PropertyDeclaration::Boolean;
    declaration.flags = PropertyDeclaration::PropertyNotAvailableInConfig;
    declaration.initialValueSource = QLatin1String("true");
    item << declaration;
    insert(item);
}

void BuiltinDeclarations::addModuleItem()
{
    ItemDeclaration item(QLatin1String("Module"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Module")  // needed, because we're adding module instances internally
                              );
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
    insert(item);
}

void BuiltinDeclarations::addProbeItem()
{
    ItemDeclaration item(QLatin1String("Probe"));
    item << conditionProperty();
    PropertyDeclaration foundProperty(QLatin1String("found"), PropertyDeclaration::Boolean);
    foundProperty.initialValueSource = QLatin1String("false");
    item << foundProperty;
    item << PropertyDeclaration(QLatin1String("configure"), PropertyDeclaration::Verbatim);
    insert(item);
}

void BuiltinDeclarations::addProductItem()
{
    ItemDeclaration item(QLatin1String("Product"));
    item.setAllowedChildTypes(ItemDeclaration::TypeNames()
            << QLatin1String("Module")
            << QLatin1String("Depends")
            << QLatin1String("Transformer")
            << QLatin1String("Group")
            << QLatin1String("FileTagger")
            << QLatin1String("Export")
            << QLatin1String("Probe")
            << QLatin1String("Rule"));
    item << conditionProperty();
    PropertyDeclaration decl(QLatin1String("type"), PropertyDeclaration::StringList);
    decl.initialValueSource = QLatin1String("[]");
    item << decl;
    item << nameProperty();
    decl = PropertyDeclaration("targetName", PropertyDeclaration::String);
    decl.initialValueSource = QLatin1String("name");
    item << decl;
    decl = PropertyDeclaration(QLatin1String("destinationDirectory"), PropertyDeclaration::String);
    decl.initialValueSource = QLatin1String("'.'");
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
            << QLatin1String("Module")
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
    decl.initialValueSource = QLatin1String("false");
    item << decl;
    item << PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("usings"), PropertyDeclaration::StringList);
    item << PropertyDeclaration(QLatin1String("auxiliaryInputs"),
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
    inheritProperty.name = QLatin1String("inheritProperties");
    inheritProperty.type = PropertyDeclaration::Boolean;
    inheritProperty.initialValueSource = QLatin1String("true");
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

} // namespace Internal
} // namespace qbs
