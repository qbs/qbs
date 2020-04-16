Module {
    property stringList listProp: [ "lower" ]

    Rule {
        inputs: ["intype"]
        Artifact {
            filePath: "dummy.out"
            fileTags: "outtype"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() {
                console.warn("listProp = " + JSON.stringify(product.lower.listProp));
            };
            cmd.silent = true;
            return [cmd];
        }
    }
}
