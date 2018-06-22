import qbs.File

CppApplication {
    consoleApplication: true
    Group {
        files: ["broken.cpp.in", "main.cpp.in"]
        fileTags: ["cpp.in"]
    }
    Rule {
        multiplex: true
        inputs: ["cpp.in"]
        outputFileTags: ["cpp"]
        outputArtifacts: [{ filePath: "main.cpp" }, { filePath: "broken.nomatch" }]
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating main.cpp";
            cmd.sourceCode = function() {
                File.copy(product.sourceDirectory + "/main.cpp.in",
                          product.buildDirectory + "/main.cpp");
                File.copy(product.sourceDirectory + "/broken.cpp.in",
                          product.buildDirectory + "/broken.nomatch");
            };
            return [cmd];
        }
    }
    FileTagger {
        patterns: ["*.nomatch"]
        fileTags: ["utter nonsense"]
    }
}
