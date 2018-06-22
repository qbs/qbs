Module {
    property bool p

    Rule {
        multiplex: true
        Artifact {
            filePath: "dummy"
            fileTags: ["out"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating dummy";
            cmd.sourceCode = function() {
                console.info("m.p: " + product.m.p);
            };
            return [cmd];
        }
    }
}
