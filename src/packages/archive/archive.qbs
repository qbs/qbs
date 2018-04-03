import qbs
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process
import qbs.TextFile

QbsProduct {
    Depends { name: "qbs_processlauncher" }
    Depends { name: "qbscore" }
    Depends { name: "bundledqt" }
    Depends { name: "qbsqtprofilesetup" }
    Depends { name: "qbs documentation" }
    Depends { name: "qbs resources" }
    Depends { name: "qbs man page"; condition: qbs.targetOS.contains("unix") }
    Depends { productTypes: ["qbsapplication", "qbsplugin"] }

    Depends { name: "archiver" }

    property stringList windeployqtArgs: [
        "--no-svg",
        "--no-system-d3d-compiler",
        "--no-angle",
        "--no-compiler-runtime",
        "--no-opengl-sw",
    ]

    // List of path prefixes to be excluded from the generated archive
    property stringList excludedPathPrefixes: [
        "bin/icudt",
        "bin/icuin",
        "bin/icuuc",
        "bin/iconengines/",
        "bin/imageformats/",
    ]
    property bool includeTopLevelDir: false

    condition: qbs.targetOS.containsAny(["windows", "macos"])
    builtByDefault: false
    name: "qbs archive"
    type: ["archiver.archive"]
    targetName: "qbs-" + qbs.targetOS[0] + "-" + qbs.architecture + "-" + qbsversion.version
    destinationDirectory: project.buildDirectory

    archiver.type: qbs.targetOS.contains("windows") ? "zip" : "tar"
    Properties {
        condition: includeTopLevelDir
        archiver.workingDirectory: qbs.installRoot + "/.."
    }
    archiver.workingDirectory: qbs.installRoot

    Group {
        name: "Licenses"
        prefix: "../../../"
        files: [
            "LGPL_EXCEPTION.txt",
            "LICENSE.LGPLv3",
            "LICENSE.LGPLv21",
            "LICENSE.GPL3-EXCEPT",
        ]
        qbs.install: true
        qbs.installDir: "share/doc/qbs"
    }

    Rule {
        condition: qbs.targetOS.contains("windows")
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
        inputs: ["dependencies.json", "installable"]
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
                for (var i = 0; i < (inputs["dependencies.json"] || []).length; ++i) {
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

                        // QTBUG-65916
                        var fileName = FileInfo.fileName(inputFilePaths[i]);
                        if (fileName.endsWith(".qm") && !fileName.startsWith("qt_"))
                            ignore = true;

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
