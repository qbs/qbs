import qbs.TextFile

Product {
    type: "custom"
    Group {
        files: "dummy.txt"
        fileTags: "input"
    }
    Rule {
        inputs: "input"
        Artifact {
            fileTags: "custom"
            filePath: "oldfile"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating file";
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            }
            return cmd;
        }
    }
}
