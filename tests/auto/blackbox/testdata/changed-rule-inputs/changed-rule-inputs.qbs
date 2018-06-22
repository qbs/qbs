Project {
    Product {
        name: "p1"
        type: "p1"
        Rule {
            alwaysRun: true
            multiplex: true
            Artifact {
                fileTags: "p1"
                filePath: "p1-dummy"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() {};
                return cmd;
            }
        }
    }
    Product {
        name: "p2"
        type: "p2"
        Depends { name: "p1" }
        Rule {
            requiresInputs: false
            multiplex: true
            inputsFromDependencies: "p1"
            Artifact {
                fileTags: "p2"
                filePath: "p2-dummy"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() {};
                return cmd;
            }
        }
    }
}
