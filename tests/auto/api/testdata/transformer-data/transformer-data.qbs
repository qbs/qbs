import qbs.File
import qbs.TextFile

Product {
    type: ["theType"]
    Rule {
        multiplex: true
        Artifact {
            filePath: "artifact1"
            fileTags: ["type1"]
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
    Rule {
        inputs: ["type1"]
        Artifact {
            filePath: "artifact2"
            fileTags: ["theType"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating " + output.fileName;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return [cmd];
        }
    }
}
