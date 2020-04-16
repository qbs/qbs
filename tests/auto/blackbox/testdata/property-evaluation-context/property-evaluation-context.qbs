Project {
    qbsSearchPaths: [ path ]
    Product {
        name: "mylib"
        Export {
            Depends { name: "top" }
            top.productInExport: product.name
        }
    }

    Product {
        type: "rule-output"
        name: "myapp"
        Depends { name: "mylib" }

        Rule {
            alwaysRun: true
            multiplex: true
            requiresInputs: false
            outputFileTags: "rule-output"
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    console.info("base.productInBase evaluated in: " + product.base.productInBase);
                    console.info("base.productInTop evaluated in: " + product.base.productInTop);
                    console.info("top.productInExport evaluated in: " + product.top.productInExport);
                    console.info("top.productInTop evaluated in: " + product.top.productInTop);
                }
                return [cmd];
            }
        }
    }
}
