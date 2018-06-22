import qbs.TextFile

CppApplication {
    name: "app"
    files: "main.c"
    cpp.includePaths: buildDirectory
    Rule {
        multiplex: true
        alwaysRun: true
        Artifact { filePath: "myheader.h"; fileTags: "hpp" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            };
            return cmd;
        }
    }
}
