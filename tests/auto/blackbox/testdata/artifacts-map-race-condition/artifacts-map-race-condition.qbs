Product {
    name: "p"
    type: ["custom1", "custom2", "custom3", "custom4", "custom5"]
    Rule {
        multiplex: true
        outputFileTags: "custom1"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "reader1";
            cmd.sourceCode = function()
            {
                for (var i = 0; i < 1000; ++i) {
                    for (var t in product.artifacts) {
                        var l = product.artifacts[t];
                        for (var j = 0; j < l.length; ++j)
                            var fileName = l[j].fileName;
                    }
                }
            };
            return cmd;
        }
    }
    Rule {
        multiplex: true
        outputFileTags: "helper"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "helper";
            cmd.sourceCode = function() { };
            return cmd;
        }
    }

    Rule {
        inputs: ["helper"]
        outputFileTags: ["custom2", "custom3", "custom4"]
        outputArtifacts: {
            console.info("writer");
            var artifacts = [];
            for (var i = 0; i < 1000; ++i) {
                artifacts.push({ filePath: "dummyt1" + i, fileTags: ["custom2"] });
                artifacts.push({ filePath: "dummyt2" + i, fileTags: ["custom3"] });
                artifacts.push({ filePath: "dummyt3" + i, fileTags: ["custom4"] });
            }
            return artifacts;
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "writer dummy command";
            cmd.sourceCode = function() { };
            return cmd;
        }
    }
    Rule {
        multiplex: true
        outputFileTags: "custom5"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "reader2";
            cmd.sourceCode = function()
            {
                for (var i = 0; i < 1000; ++i) {
                    for (var t in product.artifacts) {
                        var l = product.artifacts[t];
                        for (var j = 0; j < l.length; ++j)
                            var fileName = l[j].fileName;
                    }
                }
            };
            return cmd;
        }
    }
}
