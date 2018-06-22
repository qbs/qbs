Project {
    property bool prop: false
    Product {
        name: "test"
        type: ["out-type"]
        Group {
            name: "Rule input"
            files: ["dummy.txt"]
            fileTags: ["in-type"]
        }
        Group {
            name: "irrelevant"
            files: ["*.blubb"]
        }
        Rule {
            inputs: ["in-type"]
            Artifact {
                filePath: "dummy.out"
                fileTags: product.type
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    console.info("prop: " + project.prop);
                }
                return [cmd];
            }
        }
    }
}
