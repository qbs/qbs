import qbs.TextFile

Product {
    name: "theProduct"
    type: ["output"]
    version: "1"
    property bool dummy: false
    Rule {
        inputs: []
        multiplex: true
        Artifact {
            filePath: "output.out"
            fileTags: ["output"]
        }
        prepare: {
            console.info("running the rule: " + product.dummy);
            var cmd = new JavaScriptCommand();
            cmd.description = "creating output";
            cmd.sourceCode = function() {
                console.info(product.version);
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            }
            return [cmd];
        }
    }
}
