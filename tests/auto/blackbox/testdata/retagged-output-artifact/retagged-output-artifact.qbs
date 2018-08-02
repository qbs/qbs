import qbs.File
import qbs.TextFile

Product {
    name: "p"
    type: "p_type"
    property bool useTag1
    Rule {
        multiplex: true
        outputFileTags: ["tag1", "tag2"]
        outputArtifacts: [{filePath: "a1.txt", fileTags: product.useTag1 ? "tag1" : "tag2"}]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.filePath;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            };
            return cmd;
        }
    }
    Rule {
        inputs: "tag1"
        Artifact { filePath: "a2.txt"; fileTags: "p_type" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.filePath;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return cmd;
        }
    }
    Rule {
        inputs: "tag2"
        Artifact { filePath: "a3.txt"; fileTags: "p_type" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.filePath;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return cmd;
        }
    }
}
