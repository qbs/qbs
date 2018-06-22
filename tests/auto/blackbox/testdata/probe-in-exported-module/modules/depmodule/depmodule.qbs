Module {
    property string prop
    property stringList listProp: []

    Rule {
        inputs: ["dep-in"]
        outputFileTags: "dep-out"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating dep-out artifact";
            cmd.sourceCode = function() {
                console.info("prop: " + product.depmodule.prop);
                console.info("listProp: " + product.depmodule.listProp);
            };
            return [cmd];
        }
    }
}
