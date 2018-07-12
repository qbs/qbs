Product {
    name: "p"
    type: "p_type"
    Rule {
        multiplex: true
        outputFileTags: ["x"]
        outputArtifacts: [{filePath: "dummy1", fileTags: ["x","y","z"]}]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() { };
            return cmd;
        }
    }
    Rule {
        inputs: ["y"]
        Artifact { filePath: "dummy2"; fileTags: "p_type" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() { };
            return cmd;
        }
    }
    Rule {
        inputs: ["x"]
        Artifact { filePath: "dummy3"; fileTags: "p_type" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() { };
            return cmd;
        }
    }
}
