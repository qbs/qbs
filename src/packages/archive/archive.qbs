import qbs
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process
import qbs.TextFile

QbsProduct {
    Depends { name: "qbs_processlauncher" }
    Depends { name: "qbscore" }
    Depends { name: "bundledqt"; required: false }
    Depends { name: "qbs documentation"; condition: project.withDocumentation }
    Depends { name: "qbs resources" }
    Depends {
        name: "qbs man page"
        condition: qbs.targetOS.contains("unix") && project.withDocumentation
    }
    Depends { productTypes: ["qbsapplication", "qbsplugin"] }

    Depends { name: "archiver" }

    property bool includeTopLevelDir: false

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
        multiplex: true
        inputs: ["installable"]
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
                inputFilePaths.sort();
                var tf;
                try {
                    tf = new TextFile(outputFilePath, TextFile.WriteOnly);
                    for (var i = 0; i < inputFilePaths.length; ++i) {
                        var relativePath = FileInfo.relativePath(baseDirectory, inputFilePaths[i]);
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
