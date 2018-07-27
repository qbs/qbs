import qbs.File

QtApplication {
    name: "p"
    files: ["main.cpp", "object.h"]
    Group {
        files: "object.cpp.in"
        fileTags: "cpp.in"
    }
    Rule {
        inputs: "cpp.in"
        Artifact {
            filePath: input.completeBaseName
            fileTags: "cpp"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generatating " + output.fileName;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
            return cmd;
        }
    }
    cpp.includePaths: path
}

