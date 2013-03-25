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
    QList<PropertyDeclaration> decls;

    PropertyDeclaration conditionProperty("condition", PropertyDeclaration::Boolean);
    decls += conditionProperty;
    decls += PropertyDeclaration("name", PropertyDeclaration::String);
    decls += PropertyDeclaration("submodules", PropertyDeclaration::Variant);
    decls += PropertyDeclaration("required", PropertyDeclaration::Boolean);
    decls += PropertyDeclaration("failureMessage", PropertyDeclaration::String);
    m_builtins["Depends"] = decls;

    decls.clear();
    decls += conditionProperty;
    decls += PropertyDeclaration("name", PropertyDeclaration::String, PropertyDeclaration::PropertyNotAvailableInConfig);
    decls += PropertyDeclaration("files", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    decls += PropertyDeclaration("fileTagsFilter", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    decls += PropertyDeclaration("excludeFiles", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    decls += PropertyDeclaration("fileTags", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    decls += PropertyDeclaration("prefix", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);

    PropertyDeclaration declaration;
    declaration.name = "overrideTags";
    declaration.type = PropertyDeclaration::Boolean;
    declaration.flags = PropertyDeclaration::PropertyNotAvailableInConfig;
    declaration.initialValueSource = "true";
    decls += declaration;
    m_builtins["Group"] = decls;

    QList<PropertyDeclaration> project;
    project += PropertyDeclaration("references", PropertyDeclaration::Variant);
    project += PropertyDeclaration("moduleSearchPaths", PropertyDeclaration::Variant);
    m_builtins["Project"] = project;

    QList<PropertyDeclaration> product;
    decls += conditionProperty;
    product += PropertyDeclaration("type", PropertyDeclaration::String);
    product += PropertyDeclaration("name", PropertyDeclaration::String);
    PropertyDeclaration decl = PropertyDeclaration("targetName", PropertyDeclaration::String);
    decl.initialValueSource = "name";
    product += decl;
    product += PropertyDeclaration("destinationDirectory", PropertyDeclaration::String);
    product += PropertyDeclaration("consoleApplication", PropertyDeclaration::Boolean);
    product += PropertyDeclaration("files", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    product += PropertyDeclaration("excludeFiles", PropertyDeclaration::Variant, PropertyDeclaration::PropertyNotAvailableInConfig);
    product += PropertyDeclaration("moduleSearchPaths", PropertyDeclaration::Variant);
    m_builtins["Product"] = product;

    QList<PropertyDeclaration> fileTagger;
    fileTagger += PropertyDeclaration("pattern", PropertyDeclaration::String);
    fileTagger += PropertyDeclaration("fileTags", PropertyDeclaration::Variant);
    m_builtins["FileTagger"] = fileTagger;

    QList<PropertyDeclaration> artifact;
    artifact += conditionProperty;
    artifact += PropertyDeclaration("fileName", PropertyDeclaration::Verbatim);
    artifact += PropertyDeclaration("fileTags", PropertyDeclaration::Variant);
    artifact += PropertyDeclaration("alwaysUpdated", PropertyDeclaration::Boolean);
    m_builtins["Artifact"] = artifact;

    QList<PropertyDeclaration> rule;
    rule += PropertyDeclaration("multiplex", PropertyDeclaration::Boolean);
    rule += PropertyDeclaration("inputs", PropertyDeclaration::Variant);
    rule += PropertyDeclaration("usings", PropertyDeclaration::Variant);
    rule += PropertyDeclaration("explicitlyDependsOn", PropertyDeclaration::Variant);
    rule += PropertyDeclaration("prepare", PropertyDeclaration::Verbatim);
    m_builtins["Rule"] = rule;

    QList<PropertyDeclaration> transformer;
    transformer += PropertyDeclaration("inputs", PropertyDeclaration::Variant);
    transformer += PropertyDeclaration("prepare", PropertyDeclaration::Verbatim);
    transformer += conditionProperty;
    m_builtins["Transformer"] = transformer;

    QList<PropertyDeclaration> productModule;
    m_builtins["ProductModule"] = productModule;

    QList<PropertyDeclaration> module;
    module += PropertyDeclaration("name", PropertyDeclaration::String);
    module += PropertyDeclaration("setupBuildEnvironment", PropertyDeclaration::Verbatim);
    module += PropertyDeclaration("setupRunEnvironment", PropertyDeclaration::Verbatim);
    module += PropertyDeclaration("additionalProductFileTags", PropertyDeclaration::Variant);
    module += conditionProperty;
    m_builtins["Module"] = module;

    QList<PropertyDeclaration> propertyOptions;
    propertyOptions += PropertyDeclaration("name", PropertyDeclaration::String);
    propertyOptions += PropertyDeclaration("allowedValues", PropertyDeclaration::Variant);
    propertyOptions += PropertyDeclaration("description", PropertyDeclaration::String);
    m_builtins["PropertyOptions"] = propertyOptions;

    QList<PropertyDeclaration> probe;
    decls += conditionProperty;
    PropertyDeclaration foundProperty("found", PropertyDeclaration::Boolean);
    foundProperty.initialValueSource = "false";
    probe += foundProperty;
    probe += PropertyDeclaration("configure", PropertyDeclaration::Verbatim);
    m_builtins["Probe"] = probe;
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
            case qbs::Internal::PropertyDeclaration::Path:
                result.append("type=\"string\"");
                break;
            case qbs::Internal::PropertyDeclaration::PathList:
                result.append("type=\"string\"; isList=true");
                break;
            case qbs::Internal::PropertyDeclaration::String:
                result.append("type=\"string\"");
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

} // namespace Internal
} // namespace qbs
