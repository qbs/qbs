Product {
    type: "t"
    Depends { name: "m" }
    Group {
        files: "file.in"
        fileTags: "i"
    }
    Rule {
        inputs: "i"
        Artifact {
            filePath: "dummy"
            fileTags: "t"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {};
            return cmd;
        }
    }
}
