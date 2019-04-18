import qbs.TextFile

DynamicLibrary {
    type: base.concat("custom")
    Depends { name: "cpp" }
    files: ["testlib.c"]
    Group {
        name: "linker scripts"
        files: [
            "linkerscript1",
            "linkerscript2",
        ]
        fileTags: ["linkerscript"]
    }

    cpp.libraryPaths: [
        product.sourceDirectory, // location of linkerscripts that are included
    ]

    Rule {
        multiplex: true
        outputFileTags: "custom"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.warn("---" + product.cpp.nmPath + "---");
            }
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        requiresInputs: false
        Artifact  {
            filePath: product.buildDirectory + "/linkerscript_with_includes"
            fileTags: ["linkerscript"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourcePath = product.sourceDirectory;
            cmd.buildPath = product.buildDirectory;
            cmd.sourceCode = function() {
                var file = new TextFile(buildPath + "/linkerscript_with_includes",
                                        TextFile.WriteOnly);
                file.write("SEARCH_DIR(\"" + sourcePath + "/scripts\")\n" +
                           "INCLUDE linkerscript_to_include\n" +
                           "INCLUDE linkerscript_in_directory\n");
                file.close();
            }
            cmd.highlight = "codegen";
            cmd.description = "generating linkerscript with SEARCH_DIR and INCLUDE";
            return [cmd];
        }
    }

    qbs.installPrefix: ""
    install: true
    installDir: ""
}
