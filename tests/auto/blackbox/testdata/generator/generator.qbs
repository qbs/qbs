import qbs.TextFile

CppApplication {
    name: "app"
    files: "main.cpp"
    Group {
        files: "input.txt"
        fileTags: "generator.in"
    }
    Rule {
        inputs: ["generator.in"]
        Artifact { filePath: "file1.cpp"; fileTags: ["cpp", "file1"]; alwaysUpdated: false }
        Artifact { filePath: "file2.cpp"; fileTags: ["cpp", "file2"]; alwaysUpdated: false }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "potentially generating files";
            cmd.sourceCode = function() {
                var f = new TextFile(input.filePath, TextFile.ReadOnly);
                var content = f.readAll();
                f.close();
                if (content.contains("file1")) {
                    f = new TextFile(outputs.file1[0].filePath, TextFile.WriteOnly);
                    f.writeLine("void f1() {}");
                    f.close();
                }
                if (content.contains("file2")) {
                    f = new TextFile(outputs.file2[0].filePath, TextFile.WriteOnly);
                    f.writeLine("void f2() {}");
                    f.close();
                }
            };
            return cmd;
        }
    }
}
