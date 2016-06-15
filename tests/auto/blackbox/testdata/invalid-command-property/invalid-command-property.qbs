import qbs
import qbs.TextFile

Product {
    type: ["output"]
    Group {
        files: ["input.txt"]
        fileTags: ["input"]
    }
    Rule {
        inputs: ["input"]
        Artifact {
            filePath: "dummy"
            fileTags: ["output"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating output";
            cmd.textFile = new TextFile(input.filePath, TextFile.ReadOnly);
            cmd.sourceCode = function() {
                var content = textFile.readAll();
                textFile.close();
            }
            return [cmd];
        }
    }
}
