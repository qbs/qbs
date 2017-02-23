import qbs
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import qbs.Utilities

Product {
    name: "qbs resources"
    type: ["qbs qml type descriptions", "qbs qml type bundle"]
    Depends { name: "qbsbuildconfig" }

    Group {
        name: "Incredibuild"
        prefix: "../bin/"
        files: ["ibmsvc.xml", "ibqbs.bat"]
        fileTags: []
        qbs.install: qbs.targetOS.contains("windows")
        qbs.installDir: qbsbuildconfig.appInstallDir
    }

    Group {
        name: "Python executables"
        files: ["../src/3rdparty/python/bin/dmgbuild"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.libexecInstallDir
        qbs.installSourceBase: "../src/3rdparty/python/bin"
    }

    Group {
        name: "Python packages"
        prefix: "../src/3rdparty/python/**/"
        files: ["*.py"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share/qbs/python"
        qbs.installSourceBase: "../src/3rdparty/python/lib/python2.7/site-packages"
    }

    Group {
        name: "Imports"
        files: ["qbs/imports/qbs/**/*"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share"
        qbs.installSourceBase: "."
    }

    Group {
        name: "Modules"
        files: ["qbs/modules/**/*"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share"
        qbs.installSourceBase: "."
    }

    Group {
        name: "Module providers"
        files: ["qbs/module-providers/**/*"]
        fileTags: ["qbs resources"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share"
        qbs.installSourceBase: "."
    }

    Group {
        name: "Examples as resources"
        files: ["../examples/**/*"]
        fileTags: []
        qbs.install: true
        qbs.installDir: qbsbuildconfig.resourcesInstallDir + "/share/qbs"
        qbs.installSourceBase: ".."
    }

    Rule {
        condition: Utilities.versionCompare(product.qbs.version, "1.9.1") >= 0
        multiplex: true
        Artifact {
            filePath: "qbs.qmltypes"
            fileTags: ["qbs qml type descriptions"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Generating " + output.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var tf;
                try {
                    tf = new TextFile(output.filePath, TextFile.WriteOnly);
                    tf.writeLine(Utilities.qmlTypeInfo());
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return cmd;
        }
    }

    Rule {
        condition: Utilities.versionCompare(product.qbs.version, "1.9.1") >= 0
        multiplex: true
        Artifact {
            filePath: "qbs-bundle.json"
            fileTags: ["qbs qml type bundle"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Generating " + output.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var tf;
                try {
                    var imports = File.directoryEntries(FileInfo.joinPaths(product.sourceDirectory,
                                                                           "qbs", "imports", "qbs"),
                                                        File.Dirs | File.NoDotAndDotDot).filter(
                                function(i) { return i !== "base"; }).concat(
                                Utilities.builtinExtensionNames()).map(
                                function(i) { return "qbs." + i; });
                    imports.sort();
                    var obj = {
                        "name": "qbs",
                        "searchPaths": ["$(QBS_IMPORT_PATH)"],
                        "installPaths": ["$(QBS_IMPORT_PATH)"],
                        "implicitImports": ["__javascriptQt5__"],
                        "supportedImports": ["qbs"].concat(imports)
                    };
                    tf = new TextFile(output.filePath, TextFile.WriteOnly);
                    tf.writeLine(JSON.stringify(obj, undefined, 4));
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return cmd;
        }
    }

    Group {
        name: "QML Type Info"
        fileTagsFilter: ["qbs qml type descriptions", "qbs qml type bundle"]
        qbs.install: true
        qbs.installDir: qbsbuildconfig.qmlTypeDescriptionsInstallDir
    }
}
