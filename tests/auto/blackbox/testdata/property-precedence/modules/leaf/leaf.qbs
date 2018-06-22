Module {
    property string scalarProp: "leaf"
    property stringList listProp: ["leaf"]

    Rule {
        inputs: ["rule-input"]
        outputFileTags: "rule-output"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info("scalar prop: " + product.leaf.scalarProp);
                console.info("list prop: " + JSON.stringify(product.leaf.listProp));
            }
            return [cmd];
        }
    }
}
