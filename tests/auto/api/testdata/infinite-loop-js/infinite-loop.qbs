import qbs

Product {
    type: "mytype"
    Transformer {
        Artifact {
            fileName: "output.txt"
            fileTags: "mytype"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Running infinite loop";
            cmd.sourceCode = function() {
                while (true)
                    ;
            }
            return cmd;
        }
    }
}
