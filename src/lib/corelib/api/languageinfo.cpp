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

#include "languageinfo.h"

#include <language/builtindeclarations.h>

namespace qbs {

LanguageInfo::LanguageInfo()
{
}

QByteArray LanguageInfo::qmlTypeInfo()
{
    Internal::BuiltinDeclarations builtins;

    // Header:
    QByteArray result;
    result.append("import QtQuick.tooling 1.0\n\n");
    result.append("// This file describes the plugin-supplied types contained in the library.\n");
    result.append("// It is used for QML tooling purposes only.\n\n");
    result.append("Module {\n");

    // Individual Components:
    foreach (const QString &typeName, builtins.allTypeNames()) {
        QByteArray utf8TypeName = typeName.toUtf8();
        result.append("    Component {\n");
        result.append(QByteArray("        name: \"") + utf8TypeName + QByteArray("\"\n"));
        result.append("        exports: [ \"qbs/");
        result.append(utf8TypeName);
        result.append(" ");
        result.append(builtins.languageVersion().toUtf8());
        result.append("\" ]\n");
        result.append("        prototype: \"QQuickItem\"\n");

        Internal::ItemDeclaration itemDecl = builtins.declarationsForType(typeName);
        foreach (const Internal::PropertyDeclaration &property, itemDecl.properties()) {
            result.append("        Property { name=\"");
            result.append(property.name().toUtf8());
            result.append("\"; ");
            switch (property.type()) {
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

} // namespace qbs
