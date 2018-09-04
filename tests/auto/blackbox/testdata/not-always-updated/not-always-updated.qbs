import qbs.TextFile

Product {
    type: "p"
    Rule {
        multiplex: true
        Artifact { filePath: "dummy.txt"; fileTags: "t1"; alwaysUpdated: false }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating dummy";
            cmd.sourceCode = function() {};
            return cmd;
        }
    }
    Rule {
        inputs: "t1"
        Artifact { filePath: "o.txt"; fileTags: "p" }
        prepare: {
            var cmd = new JavaScriptCommand;
            cmd.description = "creating final";
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            };
            return cmd;
        }
    }
}



