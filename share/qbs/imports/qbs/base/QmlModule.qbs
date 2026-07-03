// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import qbs.FileInfo
import qbs.TextFile

// A QML module: a plugin library with automatic qmldir and type registration.
// Mirrors the convenience provided by qt_add_qml_module() in CMake.
//
// Minimal usage:
//   QmlModule {
//       uri: "com.example.MyModule"
//       files: ["MyItem.qml", "mycpptype.h", "mycpptype.cpp"]
//   }
//
// QML files are tagged automatically (via Qt.qml's FileTagger), listed in the
// generated qmldir, and embedded in the plugin's resources at :/qt/qml/<URI>/
// so that loadFromModule() works without setting QML_IMPORT_PATH at runtime.
// C++ files decorated with QML_ELEMENT etc. are picked up by qmltyperegistrar
// and produce a plugins.qmltypes next to the plugin library.
//
// The module is installed under <Qt QML path>/<URI path> by default; override
// installDir to change the location.

CppLibrary {
    // iOS (and other Apple embedded targets) require static plugins.
    // Android needs the extra nativelibrary tag. Everything else is a plain shared library.
    // Override type to ["staticlibrary"] to produce a static plugin on other platforms.
    type: qbs.targetOS.containsAny(["ios", "tvos", "watchos"])  || Qt.core.staticBuild
            ? ["staticlibrary"]
            : ["dynamiclibrary"].concat(isForAndroid ? ["android.nativelibrary"] : [])

    // The QML module URI (e.g. "com.example.MyModule"). Required.
    property string uri
    readonly property string uriAsPath: uri.replace(/\./g, "/")
    readonly property string uriAsIdentifier: uri.replace(/\./g, "_")
    readonly property string resourcePrefix: FileInfo.joinPaths("/qt/qml", uriAsPath)

    version: "1.0"

    // Installation directory for qmldir, plugins.qmltypes, and QML files.
    // Defaults to <Qt QML path>/<URI path>.
    property string modulesInstallDir: uri ? FileInfo.joinPaths(Qt.qml.qmlPath, uriAsPath) : undefined

    // The library binary goes alongside the module files when loaded as a plugin at runtime.
    Properties {
        condition: Qt.qml.loadedAtRuntime
        installDir: modulesInstallDir
    }

    Depends { name: "bundle" }
    bundle.isBundle: false

    Depends { name: "Qt.qml" }
    Qt.qml.importName: uri
    Qt.qml.qmldirInstallDir: modulesInstallDir
    Qt.qml.typesInstallDir: modulesInstallDir

    Group {
        name: "QML and JS sources"
        fileTagsFilter: ["qt.qml.qml", "qt.qml.js"]
        fileTags: "qt.core.resource_data"
        Qt.core.resourcePrefix: resourcePrefix
        qbs.install: modulesInstallDir
        qbs.installDir: modulesInstallDir
    }

    Group {
        name: "QML module descriptor"
        fileTagsFilter: ["qt.qml.qmldir"]
        fileTags: "qt.core.resource_data"
        Qt.core.resourcePrefix: resourcePrefix
    }

    Export {
        property string uri: exportingProduct.uri
        property string uriAsIdentifier: exportingProduct.uriAsIdentifier
        property bool needsImportStub: exportingProduct.type
            && (exportingProduct.type.contains("staticlibrary")
                || (exportingProduct.type.contains("dynamiclibrary")
                    && !exportingProduct.Qt.qml.loadedAtRuntime))

        // When the plugin gets linked directly into the consuming product, the linker may drop
        // the library because no symbols are referenced directly.
        // This rule injects a small C++ file into the consuming product that forces the linker
        // to keep the library by referencing one of its symbols.
        Rule {
            condition: needsImportStub
            multiplex: true
            requiresInputs: false
            Artifact {
                filePath: "import_" + product[product.moduleName].uriAsIdentifier + ".cpp"
                fileTags: ["cpp", "unmocable", "qt.untranslatable"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating QML plugin import stub for "
                        + product[product.moduleName].uri;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    var uriAsIdentifier = product[product.moduleName].uriAsIdentifier;
                    var funcName = "qml_register_types_" + uriAsIdentifier;
                    var varName = "_qbs_qml_" + uriAsIdentifier;
                    f.writeLine("extern void " + funcName + "();");
                    f.writeLine("volatile auto " + varName + " = &" + funcName + ";");
                    f.close();
                };
                return [cmd];
            }
        }
    }
}
