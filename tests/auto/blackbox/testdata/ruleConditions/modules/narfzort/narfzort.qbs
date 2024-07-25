import qbs.FileInfo
import qbs.TextFile

Module {
    property bool enableGroup
    property bool enableRule

    FileTagger {
        patterns: "*.narf"
        fileTags: ["narf"]
    }

    Group {
        condition: enableGroup

        Rule {
            condition: product.narfzort.enableRule
            inputs: ["narf"]
            outputFileTags: ["zort"]
            outputArtifacts: [{
                filePath: product.name + "." + input.fileName + ".zort",
                fileTags: ["zort"]
            }]
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + FileInfo.fileName(output.filePath);
                cmd.sourceCode = function() {
                        var f = new TextFile(output.filePath, TextFile.WriteOnly);
                        f.write("NARF! ZORT!");
                        f.close();
                    }
                return cmd;
            }
        }
    }
}
