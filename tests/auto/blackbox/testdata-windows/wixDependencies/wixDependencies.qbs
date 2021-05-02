import qbs.TextFile

Project {
    WindowsInstallerPackage {
        Depends { name: "app" }
        Depends { name: "lib" }
        name: "QbsSetup"
        targetName: "qbs"
        files: ["QbsSetup.wxs"]
        wix.extensions: ["WixBalExtension", "WixUIExtension"]
        destinationDirectory: project.buildDirectory
    }

    Application {
        Depends { name: "cpp" }
        name: "app"
        files: ["main.c"]
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
                    for (var j = 0; j < 1000; ++j) { // Artificial delay.
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
                    }
                };
                return [cmd];
            }
        }
        destinationDirectory: project.buildDirectory
    }
}
