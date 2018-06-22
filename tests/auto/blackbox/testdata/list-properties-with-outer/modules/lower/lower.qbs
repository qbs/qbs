Module {
    property stringList listProp

    Rule {
        inputs: ["intype"]
        outputFileTags: "outtype"
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
