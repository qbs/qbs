import qbs.TextFile

MyApplication {
    name: "myapp"
    type: base.concat("extra-output")
    files: "main.cpp"
    Group {
        fileTagsFilter: "application"
        qbs.installDir: "binDir"
        fileTags: "extra-input"
    }
    Rule {
        inputs: "extra-input"
        Artifact {
            filePath: input.baseName + ".txt"
            fileTags: "extra-output"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            }
            return cmd;
        }
    }
}
