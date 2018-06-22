Module {
    property string propDependency: "value in lowerlevel module"
    property string prop: propDependency
    property string someOtherProp

    Rule {
        inputs: ["dummy-input"]
        outputFileTags: "mytype"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() { };
            var prop = product.lowerlevel.prop;
            cmd.description = "lowerlevel.prop is '" + prop + "'.";
            return [cmd];
        }
    }
}
