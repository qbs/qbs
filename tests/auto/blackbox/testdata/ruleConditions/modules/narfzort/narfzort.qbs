import qbs 1.0
import qbs.TextFile

Module {
    property bool buildZort: true
    FileTagger {
        pattern: "*.narf"
        fileTags: ["narf"]
    }
    Rule {
        condition: product.moduleProperty("narfzort", "buildZort");
        inputs: ["narf"]
        Artifact {
            fileName: product.name + "." + input.fileName + ".zort"
            fileTags: ["zort"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() {
                    var f = new TextFile(output.fileName, TextFile.WriteOnly);
                    f.write("NARF! ZORT!");
                    f.close();
                }
            return cmd;
        }
    }
}
