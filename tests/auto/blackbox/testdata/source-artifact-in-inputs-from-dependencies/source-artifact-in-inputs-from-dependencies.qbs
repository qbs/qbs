import qbs.FileInfo
import qbs.TextFile

Project {
    Product {
        name: "dep1"
        Group {
            files: ["header.h"]
            qbs.install: true
            qbs.installDir: "include1"
        }
    }

    Product {
        name: "dep2"
        Group {
            files: ["header.h"]
            qbs.install: true
            qbs.installDir: "include2"
        }
    }

    Product {
        name: "p"
        type: ["custom"]
        Depends { name: "dep1" }
        Depends { name: "dep2" }

        Rule {
            multiplex: true
            inputsFromDependencies: ["installable"]
            Artifact {
                filePath: "output.txt"
                fileTags: ["custom"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    var tf;
                    try {
                        tf = new TextFile(output.filePath, TextFile.WriteOnly);
                        var artifactList = inputs["installable"];
                        for (var i = 0; i < (artifactList ? artifactList.length : 0); ++i) {
                            var artifact = artifactList[i];
                            tf.writeLine(FileInfo.joinPaths(artifact.qbs.installDir,
                                                            artifact.fileName));
                        }
                    } finally {
                        if (tf)
                            tf.close();
                    }
                }
                return [cmd];
            }
        }
    }
}
