import qbs

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
        alwaysRun: true
        Artifact {
            filePath: "dummy"
            fileTags: ["the-output"]
        }
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
