import qbs 1.0
import qbs.TextFile

Project {
    property var projectDefines: ["blubb2"]
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
    }

    Product {
        name: "generated text file"
        property string fileContentPrefix: "prefix 1"

        Transformer {
            Artifact { fileName: "generated.txt" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.highlight = "codegen";
                cmd.sourceCode = function() {
                    file = new TextFile(output.fileName, TextFile.WriteOnly);
                    file.truncate();
                    file.write(product.fileContentPrefix + "contents 1");
                    file.close();
                }
                return cmd;
            }
        }
    }

    references: "ruletest.qbs"

    qbsSearchPaths: "."
}
