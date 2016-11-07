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
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return [cmd];
        }
    }
}
