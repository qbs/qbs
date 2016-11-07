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
            var dummy = File.directoryEntries(product.sourceDirectory, File.Files);
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() { };
            return [cmd];
        }
    }
}
