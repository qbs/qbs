import qbs.File

Product {
    type: ["out"]
    Group {
        files: ["test.txt"]
        fileTags: ["in"]
    }
    Rule {
        inputs: ["in"]
        outputFileTags: "out"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return [cmd];
        }
    }
}
