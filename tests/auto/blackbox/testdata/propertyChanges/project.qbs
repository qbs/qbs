import qbs 1.0
import qbs.File
import qbs.TextFile

Project {
    property var projectDefines: ["blubb2"]
    property string fileContentSuffix: "suffix 1"
    CppApplication {
        name: qbs.enableDebugCode ? "product 1.debug" : "product 1.release"
        cpp.defines: ["blubb1"]
        files: "source1.cpp"
    }
    CppApplication {
        Depends { name: 'library' }
        name: "product 2"
        cpp.defines: project.projectDefines
        files: "source2.cpp"
    }
    CppApplication {
        name: "product 3"
        cpp.defines: qbs.getEnv("QBS_BLACKBOX_DEFINE")
        files: "source3.cpp"
    }
    DynamicLibrary {
        name: "library"
        Depends { name: "Qt.core" }
        files: "lib.cpp"
        bundle.isBundle: false
    }

    Product {
        name: "generated text file"
        property string fileContentPrefix: "prefix 1"

        Transformer {
            Artifact { filePath: "nothing" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() { print(product.fileContentPrefix); }
                return cmd;
            }
        }

        Transformer {
            Artifact { filePath: "generated.txt" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.filePath;
                cmd.highlight = "codegen";
                cmd.sourceCode = function() {
                    file = new TextFile(output.filePath, TextFile.WriteOnly);
                    file.truncate();
                    file.write(product.fileContentPrefix + "contents 1"
                               + project.fileContentSuffix);
                    file.close();
                }
                return cmd;
            }
        }
    }

    Product {
        Depends { name: "ruletest" }
        type: ["test-output2"]
        Rule {
            inputsFromDependencies: ['test-output']
            Artifact {
                fileTags: "test-output2"
                filePath: input.fileName + ".out2"
            }

            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.highlight = "codegen";
                cmd.description = "Making output from other output";
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); }
                return cmd;
            }
        }
    }

    references: "ruletest.qbs"

    qbsSearchPaths: "."
}
