import qbs.TextFile

CppApplication {
    consoleApplication: true
    files: ["ignored1.cpp", "ignored2.cpp", "compiled.cpp"]

    cpp.includePaths: [buildDirectory]
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }

    install: true
    installDir: ""

    Rule {
        multiplex: true
        Artifact {
            filePath: "generated.h"
            fileTags: ["hpp"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
                var header = new TextFile(output.filePath, TextFile.WriteOnly);
                header.close();
            };
            return [cmd];
        }
    }
}
