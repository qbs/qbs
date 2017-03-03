import qbs

Module {
    property stringList listProp

    Rule {
        inputs: ["intype"]
        Artifact {
            filePath: "dummy.out"
            fileTags: "outtype"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() {
                console.info("listProp = " + JSON.stringify(product.lower.listProp));
            };
            cmd.silent = true;
            return [cmd];
        }
    }
}
