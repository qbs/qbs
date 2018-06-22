Project {
    Product {
        name: "p1"
        type: "blubb1"
        Rule {
            multiplex: true
            outputFileTags: "blubb1"
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    console.info(product.buildDirectory);
                }
                return cmd;
            }
        }
    }
    Product {
        name: "p2"
        type: "blubb2"
        Depends { name: "p1" }
        Rule {
            inputsFromDependencies: "blubb1"
            outputFileTags: "blubb2"
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    console.info(product.buildDirectory);
                    console.info(project.buildDirectory);
                    console.info(project.sourceDirectory);
                }
                return cmd;
            }
        }
    }
}
