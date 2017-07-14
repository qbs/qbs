import qbs
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process
import qbs.TextFile

Product {
    Depends { name: "qbs-config" }
    Depends { name: "qbs-config-ui" }
    Depends { name: "qbs-qmltypes" }
    Depends { name: "qbs-setup-android" }
    Depends { name: "qbs-setup-qt" }
    Depends { name: "qbs-setup-toolchains" }
    Depends { name: "qbs_app" }
    Depends { name: "qbs_processlauncher" }
    Depends { name: "qbsversion" }
    Depends { name: "qbscore" }
    Depends { name: "qbsqtprofilesetup" }
    Depends { name: "qbs_cpp_scanner" }
    Depends { name: "qbs_qt_scanner" }
    Depends { name: "qbs documentation" }
    Depends { name: "qbs resources" }

    Depends { name: "archiver" }
    Depends { name: "Qt.core" }

    readonly property bool hasQt56: {
        if (Qt.core.versionMajor === 5)
            return Qt.core.versionMinor >= 6;
        return Qt.core.versionMajor > 5;
    }

    property stringList windeployqtArgs: [
        "--no-svg",
        "--no-system-d3d-compiler",
        "--no-angle",
        "--no-compiler-runtime",
    ].concat(hasQt56 ? ["--no-opengl-sw"] : [])

    // List of path prefixes to be excluded from the generated archive
    property stringList excludedPathPrefixes: [
        "bin/icudt",
        "bin/icuin",
        "bin/icuuc",
        "bin/iconengines/",
        "bin/imageformats/",
    ]
    property bool includeTopLevelDir: false

    condition: qbs.targetOS.contains("windows")
    builtByDefault: false
    name: "qbs archive"
    type: ["archiver.archive"]
    targetName: "qbs-windows-" + qbs.architecture + "-" + qbsversion.version
    destinationDirectory: project.buildDirectory

    archiver.type: "zip"
    Properties {
        condition: includeTopLevelDir
        archiver.workingDirectory: qbs.installRoot + "/.."
    }
    archiver.workingDirectory: qbs.installRoot

    Rule {
        multiplex: true
        inputsFromDependencies: ["installable"]

        Artifact {
            filePath: "windeployqt.json"
            fileTags: ["dependencies.json"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "windeployqt";
            cmd.outputFilePath = output.filePath;
            cmd.installRoot = product.moduleProperty("qbs", "installRoot");
            cmd.windeployqtArgs = product.windeployqtArgs;
            cmd.binaryFilePaths = inputs.installable.filter(function (artifact) {
                return artifact.fileTags.contains("application")
                        || artifact.fileTags.contains("dynamiclibrary");
            }).map(function(a) { return ModUtils.artifactInstalledFilePath(a); });
            cmd.extendedDescription = FileInfo.joinPaths(
                        product.moduleProperty("Qt.core", "binPath"), "windeployqt") + ".exe " +
                    ["--json"].concat(cmd.windeployqtArgs).concat(cmd.binaryFilePaths).join(" ");
            cmd.sourceCode = function () {
                var out;
                var process;
                try {
                    process = new Process();
                    process.exec(FileInfo.joinPaths(product.moduleProperty("Qt.core", "binPath"),
                                                    "windeployqt"), ["--json"]
                                 .concat(windeployqtArgs).concat(binaryFilePaths), true);
                    out = process.readStdOut();
                } finally {
                    if (process)
                        process.close();
                }

                var tf;
                try {
                    tf = new TextFile(outputFilePath, TextFile.WriteOnly);
                    tf.write(out);
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["dependencies.json"]
        inputsFromDependencies: ["installable"]

        Artifact {
            filePath: "list.txt"
            fileTags: ["archiver.input-list"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.excludedPathPrefixes = product.excludedPathPrefixes;
            cmd.inputFilePaths = inputs.installable.map(function(a) {
                return ModUtils.artifactInstalledFilePath(a);
            });
            cmd.outputFilePath = output.filePath;
            cmd.baseDirectory = product.moduleProperty("archiver", "workingDirectory");
            cmd.sourceCode = function() {
                var tf;
                for (var i = 0; i < inputs["dependencies.json"].length; ++i) {
                    try {
                        tf = new TextFile(inputs["dependencies.json"][i].filePath,
                                          TextFile.ReadOnly);
                        inputFilePaths = inputFilePaths.concat(
                                    JSON.parse(tf.readAll())["files"].map(function (obj) {
                                        return FileInfo.joinPaths(
                                                    FileInfo.fromWindowsSeparators(obj.target),
                                                    FileInfo.fileName(
                                                        FileInfo.fromWindowsSeparators(
                                                            obj.source)));
                        }));
                    } finally {
                        if (tf)
                            tf.close();
                    }
                }

                inputFilePaths.sort();

                try {
                    tf = new TextFile(outputFilePath, TextFile.ReadWrite);
                    for (var i = 0; i < inputFilePaths.length; ++i) {
                        var ignore = false;
                        var relativePath = FileInfo.relativePath(baseDirectory, inputFilePaths[i]);
                        for (var j = 0; j < excludedPathPrefixes.length; ++j) {
                            if (relativePath.startsWith(excludedPathPrefixes[j])) {
                                ignore = true;
                                break;
                            }
                        }

                        if (!ignore)
                            tf.writeLine(relativePath);
                    }
                } finally {
                    if (tf)
                        tf.close();
                }
            };

            return [cmd];
        }
    }
}
