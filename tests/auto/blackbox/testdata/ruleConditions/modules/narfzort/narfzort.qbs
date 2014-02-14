import qbs 1.0
import qbs.FileInfo
import qbs.TextFile

Module {
    property bool buildZort: true
    FileTagger {
        patterns: "*.narf"
        fileTags: ["narf"]
    }
    Rule {
        condition: product.moduleProperty("narfzort", "buildZort");
        inputs: ["narf"]
        outputFileTags: ["zort"]
        outputArtifacts: [{
            filePath: product.name + "." + input.fileName + ".zort",
            fileTags: ["zort"]
        }]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + FileInfo.fileName(output.fileName);
            cmd.sourceCode = function() {
                    var f = new TextFile(output.fileName, TextFile.WriteOnly);
                    f.write("NARF! ZORT!");
                    f.close();
                }
            return cmd;
        }
    }
}
