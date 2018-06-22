import qbs.TextFile

CppApplication {
    type: ["application", "stuff"]
    consoleApplication: true
    files: ["file1.cpp", "file2.cpp", "main.cpp"]

    Rule {
        inputs: ["cpp"]
        outputFileTags: ["stuff"]
        outputArtifacts: {
            return [{
                filePath: input.completeBaseName + ".stuff",
                fileTags: ["stuff"]
            }];
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.fileName
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.write("crazy stuff");
                f.close();
            }
            return cmd;
        }
    }
}

