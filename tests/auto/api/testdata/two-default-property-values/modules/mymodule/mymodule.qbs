import qbs.TextFile

Module {
    property string direct
    property string indirect: direct ? "set" : "unset"

    Rule {
        inputs: ["txt"]
        Artifact {
            filePath: product.moduleProperty("mymodule", "indirect")
            fileTags: ["mymodule"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            };
            return [cmd];
        }
    }
}
