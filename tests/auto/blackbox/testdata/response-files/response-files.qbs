import qbs.FileInfo
import qbs.TextFile

Project {
    CppApplication {
        name: "cat-response-file"
        files: ["cat-response-file.cpp"]
        cpp.enableExceptions: true
    }
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "response-file-text"
        type: ["text"]
        Depends { name: "cpp" }
        Depends { name: "cat-response-file" }
        qbs.installPrefix: ""
        Group {
            fileTagsFilter: ["text"]
            qbs.install: true
        }
        Rule {
            inputsFromDependencies: ["application"]
            Artifact {
                filePath: "response-file-content.txt"
                fileTags: ["text"]
            }
            prepare: {
                var filePath = inputs["application"][0].filePath;
                var args = [output.filePath, "foo", "with space", "bar"];
                var cmd = new Command(filePath, args);
                cmd.responseFileThreshold = 1;
                cmd.responseFileArgumentIndex = 1;
                cmd.responseFileUsagePrefix = '@';
                cmd.silent = true;
                return cmd;
            }
        }
    }
    Product {
        name: "lotsofobjects"
        type: ["dynamiclibrary"]
        // clang-cl does not use response file internally, thus linker complains that command is
        // too long. This can be worked around by calling the linker directly
        cpp.linkerMode: qbs.toolchain.contains("clang-cl") ? "manual" : original
        Depends { name: "cpp" }
        Rule {
            multiplex: true
            outputFileTags: ["cpp"]
            outputArtifacts: {
                var artifacts = [];
                for (var i = 0; i < 1000; ++i)
                    artifacts.push({filePath: "source-" + i + ".cpp", fileTags: ["cpp"]});
                return artifacts;
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + outputs["cpp"].length + " dummy source files";
                cmd.outputFilePaths = outputs["cpp"].map(function (a) {
                    return a.filePath;
                });
                cmd.sourceCode = function () {
                    var index = 0;
                    outputFilePaths.map(function (fp) {
                        var tf = new TextFile(fp, TextFile.WriteOnly);
                        try {
                            tf.writeLine("extern int foo" + index + "() { return 0; }");
                            ++index;
                        } finally {
                            tf.close();
                        }
                    });
                };
                return [cmd];
            }
        }
    }
}
