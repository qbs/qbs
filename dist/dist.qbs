import qbs
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process
import qbs.TextFile
import QbsFunctions

Product {
    Depends { name: "qbs-config" }
    Depends { name: "qbs-config-ui" }
    Depends { name: "qbs-qmltypes" }
    Depends { name: "qbs-setup-android" }
    Depends { name: "qbs-setup-qt" }
    Depends { name: "qbs-setup-toolchains" }
    Depends { name: "qbs_app" }
    Depends { name: "qbscore" }
    Depends { name: "qbsqtprofilesetup" }
    Depends { name: "qbs_cpp_scanner" }
    Depends { name: "qbs_qt_scanner" }
    Depends { name: "documentation" }
    Depends { name: "qbs resources" }

    Depends { name: "archiver" }
    Depends { name: "Qt.core" }

    condition: qbs.targetOS.contains("windows")
    builtByDefault: false
    type: ["archiver.archive"]
    targetName: "qbs-windows-" + qbs.architecture + "-" + QbsFunctions.qbsVersion()
    destinationDirectory: project.buildDirectory

    archiver.type: "zip"
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
            cmd.binaryFilePaths = inputs.installable.filter(function (artifact) {
                return artifact.fileTags.contains("application")
                        || artifact.fileTags.contains("dynamiclibrary");
            }).map(ModUtils.artifactInstalledFilePath);
            cmd.sourceCode = function () {
                var out;
                var process;
                try {
                    process = new Process();
                    process.exec(FileInfo.joinPaths(product.moduleProperty("Qt.core", "binPath"),
                                                    "windeployqt"), ["--json"]
                                 .concat(binaryFilePaths), true);
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
            cmd.inputFilePaths = inputs.installable.map(ModUtils.artifactInstalledFilePath);
            cmd.outputFilePath = output.filePath;
            cmd.installRoot = product.moduleProperty("qbs", "installRoot");
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
                    for (var i = 0; i < inputFilePaths.length; ++i)
                        tf.writeLine(FileInfo.relativePath(installRoot, inputFilePaths[i]));
                } finally {
                    if (tf)
                        tf.close();
                }
            };

            return [cmd];
        }
    }
}
