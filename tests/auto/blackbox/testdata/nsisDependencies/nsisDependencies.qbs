import qbs.FileInfo
import qbs.TextFile

Project {
    property bool dummy: { console.info("is Windows: " + qbs.targetOS.includes("windows")); }

    NSISSetup {
        name: "inst"
        destinationDirectory: project.buildDirectory

        Depends { name: "app" }
        Depends { name: "lib" }

        Properties {
            condition: nsis.present
            nsis.defines: ["buildDirectory=" + FileInfo.toWindowsSeparators(project.buildDirectory)]
        }

        files: ["hello.nsi"]
    }

    Application {
        Depends { name: "cpp" }
        name: "app"
        files: ["main.c"]

        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
        destinationDirectory: project.buildDirectory
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "lib"
        files: ["main.c"]

        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
        Rule {
            // This rule tries to provoke the installer into building too early (and the test
            // verifies that it does not) by causing the build of the installables to take
            // a lot longer.
            multiplex: true
            outputFileTags: ["c"]
            outputArtifacts: {
                var artifacts = [];
                for (var i = 0; i < 96; ++i)
                    artifacts.push({ filePath: "c" + i + ".c", fileTags: ["c"] });
                return artifacts;
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    for (var i = 0; i < outputs["c"].length; ++i) {
                        var tf;
                        try {
                            tf = new TextFile(outputs["c"][i].filePath, TextFile.WriteOnly);
                            tf.writeLine("int main" + i + "() { return 0; }");
                        } finally {
                            if (tf)
                                tf.close();
                        }
                    }
                };
                return [cmd];
            }
        }
        destinationDirectory: project.buildDirectory
    }
}
