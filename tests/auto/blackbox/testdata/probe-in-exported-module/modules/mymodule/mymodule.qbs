Module {
    Depends { name: "depmodule" }
    Probe {
        id: theProbe
        configure: { found = true; }
    }
    property bool found: theProbe.found
    depmodule.prop: found ? "yes" : "no"
    depmodule.listProp: theProbe.found ? ["my"] : []

    Rule {
        inputs: ["in"]
        outputFileTags: "out"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating out artifact";
            cmd.sourceCode = function() {
                console.info("found: " + product.mymodule.found);
            };
            return [cmd];
        }
    }
}
