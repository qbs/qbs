Product {
    type: "product-under-test"
    Rule {
        multiplex: true
        Artifact {
            filePath: "output.txt"
            fileTags: "product-under-test"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Running infinite loop";
            cmd.sourceCode = function() {
                while (true)
                    ;
            }
            cmd.timeout = 3;
            return cmd;
        }
    }
}
