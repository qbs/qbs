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
                console.info("listProp: " + JSON.stringify(input.lower.listProp));
            }
            return [cmd];
        }
    }
}
