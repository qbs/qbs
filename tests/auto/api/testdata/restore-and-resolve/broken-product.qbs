import qbs.TextFile

syntax error
Product {
    type: "t"
    Rule {
        multiplex: true
        Artifact { filePath: "dummy.txt"; fileTags: "t" }
        prepare: {
            var cmd = JavaScriptCommand();
            cmd.description = "creating " + output.fileName;
            cmd.sourceCode = function() {
                new TextFile(output.filePath, TextFile.WriteOnly);
            };
            return cmd;
        }
    }
}
