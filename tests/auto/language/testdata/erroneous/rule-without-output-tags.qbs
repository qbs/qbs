Product {
    Rule {
        inputs: "input-tag"
        prepare: {
            var cmd = new JavaScriptCommand;
            cmd.silent = true;
            cmd.sourceCode = function() {};
            return cmd;
        }
    }
}
