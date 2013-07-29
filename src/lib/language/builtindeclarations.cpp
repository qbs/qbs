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
    addPropertyOptionsItem();
    addRuleItem();
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

        QList<PropertyDeclaration> propertyList = m_builtins.value(component);
        foreach (const PropertyDeclaration &property, propertyList) {
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

QList<PropertyDeclaration> BuiltinDeclarations::declarationsForType(const QString &typeName) const
{
    return m_builtins.value(typeName);
}

void BuiltinDeclarations::setupItemForBuiltinType(Item *item) const
{
    foreach (const PropertyDeclaration &pd, declarationsForType(item->typeName())) {
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

void BuiltinDeclarations::addArtifactItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    properties += PropertyDeclaration(QLatin1String("fileName"), PropertyDeclaration::Verbatim);
    properties += PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant);
    PropertyDeclaration decl(QLatin1String("alwaysUpdated"), PropertyDeclaration::Boolean);
    decl.initialValueSource = QLatin1String("true");
    properties += decl;
    m_builtins[QLatin1String("Artifact")] = properties;
}

void BuiltinDeclarations::addDependsItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    properties += nameProperty();
    properties += PropertyDeclaration(QLatin1String("submodules"), PropertyDeclaration::Variant);
    properties += PropertyDeclaration(QLatin1String("required"), PropertyDeclaration::Boolean);
    properties += PropertyDeclaration(QLatin1String("failureMessage"), PropertyDeclaration::String);
    m_builtins[QLatin1String("Depends")] = properties;
}

void BuiltinDeclarations::addExportItem()
{
    QList<PropertyDeclaration> properties;
    m_builtins[QLatin1String("ProductModule")] = properties; // ### remove in 1.2
    m_builtins[QLatin1String("Export")] = properties;
}

void BuiltinDeclarations::addFileTaggerItem()
{
    QList<PropertyDeclaration> properties;
    properties += PropertyDeclaration(QLatin1String("pattern"), PropertyDeclaration::String);
    properties += PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant);
    m_builtins[QLatin1String("FileTagger")] = properties;
}

void BuiltinDeclarations::addGroupItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    properties += PropertyDeclaration(QLatin1String("name"), PropertyDeclaration::String,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("files"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("fileTagsFilter"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("excludeFiles"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("fileTags"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("prefix"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    PropertyDeclaration declaration;
    declaration.name = QLatin1String("overrideTags");
    declaration.type = PropertyDeclaration::Boolean;
    declaration.flags = PropertyDeclaration::PropertyNotAvailableInConfig;
    declaration.initialValueSource = QLatin1String("true");
    properties += declaration;
    m_builtins[QLatin1String("Group")] = properties;
}

void BuiltinDeclarations::addModuleItem()
{
    QList<PropertyDeclaration> properties;
    properties += nameProperty();
    properties += conditionProperty();
    properties += PropertyDeclaration(QLatin1String("setupBuildEnvironment"),
                                      PropertyDeclaration::Verbatim);
    properties += PropertyDeclaration(QLatin1String("setupRunEnvironment"),
                                      PropertyDeclaration::Verbatim);
    properties += PropertyDeclaration(QLatin1String("additionalProductFileTags"),
                                      PropertyDeclaration::Variant);
    m_builtins[QLatin1String("Module")] = properties;
}

void BuiltinDeclarations::addProbeItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    PropertyDeclaration foundProperty(QLatin1String("found"), PropertyDeclaration::Boolean);
    foundProperty.initialValueSource = QLatin1String("false");
    properties += foundProperty;
    properties += PropertyDeclaration(QLatin1String("configure"), PropertyDeclaration::Verbatim);
    m_builtins[QLatin1String("Probe")] = properties;
}

void BuiltinDeclarations::addProductItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    properties += PropertyDeclaration(QLatin1String("type"), PropertyDeclaration::StringList);
    properties += nameProperty();
    PropertyDeclaration decl = PropertyDeclaration("targetName", PropertyDeclaration::String);
    decl.initialValueSource = QLatin1String("name");
    properties += decl;
    decl = PropertyDeclaration(QLatin1String("destinationDirectory"), PropertyDeclaration::String);
    decl.initialValueSource = QLatin1String("'.'");
    properties += decl;
    properties += PropertyDeclaration(QLatin1String("consoleApplication"),
                                      PropertyDeclaration::Boolean);
    properties += PropertyDeclaration(QLatin1String("files"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("excludeFiles"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("moduleSearchPaths"),
                                      PropertyDeclaration::Variant);
    properties += PropertyDeclaration(QLatin1String("version"), PropertyDeclaration::String);
    m_builtins[QLatin1String("Product")] = properties;
}

void BuiltinDeclarations::addProjectItem()
{
    QList<PropertyDeclaration> properties;
    properties += nameProperty();
    properties += conditionProperty();
    properties += PropertyDeclaration(QLatin1String("references"), PropertyDeclaration::Variant,
                                      PropertyDeclaration::PropertyNotAvailableInConfig);
    properties += PropertyDeclaration(QLatin1String("moduleSearchPaths"),
            PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    m_builtins[QLatin1String("Project")] = properties;
}

void BuiltinDeclarations::addPropertyOptionsItem()
{
    QList<PropertyDeclaration> properties;
    properties += nameProperty();
    properties += PropertyDeclaration(QLatin1String("allowedValues"), PropertyDeclaration::Variant);
    properties += PropertyDeclaration(QLatin1String("description"), PropertyDeclaration::String);
    m_builtins[QLatin1String("PropertyOptions")] = properties;
}

void BuiltinDeclarations::addRuleItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    PropertyDeclaration decl(QLatin1String("multiplex"), PropertyDeclaration::Boolean);
    decl.initialValueSource = QLatin1String("false");
    properties += decl;
    properties += PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::StringList);
    properties += PropertyDeclaration(QLatin1String("usings"), PropertyDeclaration::StringList);
    properties += PropertyDeclaration(QLatin1String("auxiliaryInputs"),
                                      PropertyDeclaration::StringList);
    properties += PropertyDeclaration(QLatin1String("explicitlyDependsOn"),
                                      PropertyDeclaration::StringList);
    properties += PropertyDeclaration(QLatin1String("prepare"), PropertyDeclaration::Verbatim);
    m_builtins[QLatin1String("Rule")] = properties;
}

void BuiltinDeclarations::addSubprojectItem()
{
    QList<PropertyDeclaration> properties;
    properties += PropertyDeclaration(QLatin1String("filePath"), PropertyDeclaration::Path);
    PropertyDeclaration inheritProperty;
    inheritProperty.name = QLatin1String("inheritProperties");
    inheritProperty.type = PropertyDeclaration::Boolean;
    inheritProperty.initialValueSource = QLatin1String("true");
    properties += inheritProperty;
    m_builtins[QLatin1String("SubProject")] = properties;
}

void BuiltinDeclarations::addTransformerItem()
{
    QList<PropertyDeclaration> properties;
    properties += conditionProperty();
    properties += PropertyDeclaration(QLatin1String("inputs"), PropertyDeclaration::Variant);
    properties += PropertyDeclaration(QLatin1String("prepare"), PropertyDeclaration::Verbatim);
    m_builtins[QLatin1String("Transformer")] = properties;
}

} // namespace Internal
} // namespace qbs
