import qbs
import qbs.File

Product {
    type: ["out"]
    Group {
        files: ["test.txt"]
        fileTags: ["in"]
    }
    Rule {
        inputs: ["in"]
        Artifact {
            filePath: "dummy.txt"
            fileTags: ["out"]
        }
        prepare: {
            File.copy(input.filePath, output.filePath);
            var cmd = new JavaScriptCommand();
            cmd.description = "no-op";
            cmd.sourceCode = function() { };
            return [cmd];
        }
    }
}
