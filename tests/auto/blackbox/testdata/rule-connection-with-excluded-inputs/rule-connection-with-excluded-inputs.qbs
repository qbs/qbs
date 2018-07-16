Product {
    name: "p"
    type: "p_type"
    Rule {
        multiplex: true
        Artifact { filePath: "x.txt"; fileTags: "x" }
        Artifact { filePath: "y.txt"; fileTags: "y" }
        Artifact { filePath: "p.txt"; fileTags: "p_type" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {};
            return cmd;
        }
    }
    Rule {
        multiplex: true
        Artifact { filePath: "x2.txt"; fileTags: "x" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {};
            return cmd;
        }
    }
    Rule {
        multiplex: true
        inputs: "x"
        excludedInputs: "y"
        Artifact { filePath: "dummy"; fileTags: "p_type" }
        prepare: {
            console.info("inputs.x: " + (inputs.x ? inputs.x.length : 0));
            console.info("inputs.y: " + (inputs.y ? inputs.y.length : 0));
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {};
            return cmd;
        }
    }
}
