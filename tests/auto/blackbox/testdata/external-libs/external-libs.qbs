import qbs
import qbs.TextFile

Project {
    property string libDir: sourceDirectory + "/libs"
    StaticLibrary {
        name: "lib1"
        destinationDirectory: project.libDir
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["lib1.cpp"]
    }
    StaticLibrary {
        name: "lib2"
        destinationDirectory: project.libDir
        Depends { name: "cpp" }
        Depends { name: "lib1" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["lib2.cpp"]
    }
    // TODO: Remove once we have parameterized dependencies
    Product {
        name: "barrier"
        type: ["blubb"]
        Depends { name: "lib1" }
        Depends { name: "lib2" }
        Rule {
            multiplex: true
            inputsFromDependencies: ["staticlibrary"]
            Artifact {
                filePath: "dummy"
                fileTags: ["blubb"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() { }
                return [cmd];
            }
        }
    }
    CppApplication {
        Depends { name: "barrier" }
        files: ["main.cpp"]
        cpp.libraryPaths: [project.libDir]
        cpp.staticLibraries: ["lib1", "lib2", "lib1"]
        Rule {
            inputsFromDependencies: ["blubb"]
            Artifact {
                filePath: "dummy.cpp"
                fileTags: ["cpp"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.writeLine("void dummy() { }");
                    f.close();
                };
                return [cmd];
            }
        }
    }
}
