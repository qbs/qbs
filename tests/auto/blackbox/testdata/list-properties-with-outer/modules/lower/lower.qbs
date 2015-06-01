import qbs

Module {
    property stringList listProp

    Rule {
        inputs: ["intype"]
        Artifact {
            filePath: "dummy.out"
            fileTags: ["outtype"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                print(product.moduleProperty("lower", "listProp").length + " elements");
            }
            return [cmd];
        }
    }
}
