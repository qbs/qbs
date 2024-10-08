import qbs.File

CppApplication {
    name: "MyApp"
    consoleApplication: true
    property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
    cpp.includePaths: [product.buildDirectory]
    Group {
        files: ["pch.h"]
        fileTags: ["cpp_pch_src"]
    }
    Group {
        files: ["autogen.h.in"]
        fileTags: ["hpp.in"]
    }
    files: ["main.cpp"]
    Rule {
        inputs: ["hpp.in"]
        Artifact {
            filePath: "autogen.h"
            fileTags: ["hpp"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
            return [cmd];
        }
    }
}
