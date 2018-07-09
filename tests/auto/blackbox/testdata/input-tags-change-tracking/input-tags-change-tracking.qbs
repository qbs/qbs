import qbs.TextFile

Product {
    name: "p"
    type: "p_tag"
    Group {
        files: "input.txt"
        fileTags: "txt"
    }
    Rule {
        inputs: "txt"
        outputFileTags: "p_tag"
        outputArtifacts: [{
                filePath: input.fileTags.contains("y") ? "y.out" : "x.out",
                fileTags: "p_tag"
        }]
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
