import qbs
import qbs.TextFile

Product {
    name: "theProduct"
    type: ["output"]
    version: "1"
    Rule {
        inputs: []
        multiplex: true
        Artifact {
            filePath: "output.out"
            fileTags: ["output"]
        }
        prepare: {
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
