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
                var listProp = input.moduleProperty("lower", "listProp");
                console.info("listProp: " + JSON.stringify(listProp));
            }
            return [cmd];
        }
    }
}
