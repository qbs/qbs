Module {
    Probe {
        id: propProbe
        property stringList prop: []
        configure: {
            prop = [];
            prop.push("probe");
            found = true;
        }
    }

    property stringList prop: propProbe.found ? propProbe.prop : ["other"]

    Rule {
        multiplex: true
        outputFileTags: "the-output"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating dummy";
            cmd.sourceCode = function() {
                console.info("prop: " + JSON.stringify(product.mymodule.prop));
            }
            return [cmd];
        }
    }
}
