CppApplication {
    type: base.concat("txt")
    consoleApplication: true
    files : ["main.cpp"]
    Rule {
        inputs: ["application"]
        outputArtifacts: [{
            filePath: input.completeBaseName + ".txt"
        }]
        outputFileTags: ["txt"]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            return cmd;
        }
    }
}
