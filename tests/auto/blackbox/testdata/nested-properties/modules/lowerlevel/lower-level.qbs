import qbs

Module {
    property string prop: "value in lowerlevel module"
    Rule {
        inputs: ["dummy-input"]
        Artifact {
            filePath: "dummy.out"
            fileTags: "mytype"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() { };
            var prop = product.moduleProperty("lowerlevel", "prop");
            cmd.description = "lowerlevel.prop is '" + prop + "'.";
            return [cmd];
        }
    }
}
