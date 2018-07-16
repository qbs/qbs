import qbs.TextFile

Product {
    name: "p"
    type: "p_tag"
    property string generateInput
    Group {
        condition: generateInput == "no"
        files: "input.txt"
        fileTags: ["txt", "empty"]
    }
    Rule {
        condition: generateInput == "static"
        multiplex: true
        Artifact { filePath: "input.txt"; fileTags: ["txt", "empty"] }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
                var out = new TextFile(output.filePath, TextFile.WriteOnly);
                out.close();
            };
            return cmd;
        }
    }
    Rule {
        condition: generateInput == "dynamic"
        multiplex: true
        outputFileTags: ["txt", "empty"]
        outputArtifacts: [{filePath: "input.txt", fileTags: ["txt", "empty"]}]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
                var out = new TextFile(output.filePath, TextFile.WriteOnly);
                out.close();
            };
            return cmd;
        }
    }

    Rule {
        inputs: "txt"
        outputFileTags: "p_tag"
        outputArtifacts: {
            if (input.fileTags.contains("empty"))
                return [];
            return [{
                filePath: input.fileTags.contains("y") ? "y.out" : "x.out",
                fileTags: "p_tag"
            }]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
                var out = new TextFile(output.filePath, TextFile.WriteOnly);
                out.close();
            };
            return cmd;
        }
    }
}
