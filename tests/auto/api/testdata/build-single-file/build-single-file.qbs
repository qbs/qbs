import qbs
import qbs.TextFile

CppApplication {
    files: ["ignored1.cpp", "ignored2.cpp", "compiled.cpp"]

    cpp.includePaths: [buildDirectory]
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }

    Group {
        fileTagsFilter: ["application"]
        qbs.install: true
    }

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
