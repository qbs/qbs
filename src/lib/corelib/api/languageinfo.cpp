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

#include "languageinfo.h"

#include <language/builtindeclarations.h>
#include <tools/qttools.h>
#include <tools/version.h>

#include <QtCore/qstringlist.h>

namespace qbs {

std::string LanguageInfo::qmlTypeInfo()
{
    const Internal::BuiltinDeclarations &builtins = Internal::BuiltinDeclarations::instance();

    // Header:
    std::string result;
    result.append("import QtQuick.tooling 1.0\n\n");
    result.append("// This file describes the plugin-supplied types contained in the library.\n");
    result.append("// It is used for QML tooling purposes only.\n\n");
    result.append("Module {\n");

    // Individual Components:
    auto typeNames = builtins.allTypeNames();
    typeNames.sort();
    for (const QString &typeName : qAsConst(typeNames)) {
        const auto typeNameString = typeName.toStdString();
        result.append("    Component {\n");
        result.append("        name: \"" + typeNameString + "\"\n");
        result.append("        exports: [ \"qbs/");
        result.append(typeNameString);
        result.append(" ");
        const auto v = builtins.languageVersion();
        result.append(QStringLiteral("%1.%2")
                      .arg(v.majorVersion()).arg(v.minorVersion()).toUtf8().data());
        result.append("\" ]\n");
        result.append("        prototype: \"QQuickItem\"\n");

        Internal::ItemDeclaration itemDecl
                = builtins.declarationsForType(builtins.typeForName(typeName));
        auto properties = itemDecl.properties();
        std::sort(std::begin(properties), std::end(properties), []
                  (const Internal::PropertyDeclaration &a, const Internal::PropertyDeclaration &b) {
            return a.name() < b.name();
        });
        for (const Internal::PropertyDeclaration &property : qAsConst(properties)) {
            result.append("        Property { name: \"");
            result.append(property.name().toUtf8().data());
            result.append("\"; ");
            switch (property.type()) {
            case qbs::Internal::PropertyDeclaration::UnknownType:
                result.append("type: \"unknown\"");
                break;
            case qbs::Internal::PropertyDeclaration::Boolean:
                result.append("type: \"bool\"");
                break;
            case qbs::Internal::PropertyDeclaration::Integer:
                result.append("type: \"int\"");
                break;
            case qbs::Internal::PropertyDeclaration::Path:
                result.append("type: \"string\"");
                break;
            case qbs::Internal::PropertyDeclaration::PathList:
                result.append("type: \"string\"; isList: true");
                break;
            case qbs::Internal::PropertyDeclaration::String:
                result.append("type: \"string\"");
                break;
            case qbs::Internal::PropertyDeclaration::StringList:
                result.append("type: \"string\"; isList: true");
                break;
            case qbs::Internal::PropertyDeclaration::Variant:
                result.append("type: \"QVariant\"");
                break;
            case qbs::Internal::PropertyDeclaration::VariantList:
                result.append("type: \"QVariantList\"");
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

Version LanguageInfo::qbsVersion()
{
    static const auto v = Version::fromString(QLatin1String(QBS_VERSION));
    return v;
}

} // namespace qbs
