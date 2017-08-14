import qbs
import qbs.TextFile

Project {
    CppApplication {
        consoleApplication: true
        name: "no-match"
        files: "main.cpp"
    }

    Project {
        CppApplication {
            consoleApplication: true
            name: "app1"
            files: "main.cpp"
        }
        CppApplication {
            consoleApplication: true
            name: "app2"
            files: "main.cpp"
        }
        CppApplication {
            consoleApplication: true
            name: "app3"
            files: "main.cpp"
        }

        DynamicLibrary {
            Depends { name: "cpp" }
            name: "lib-product"
            files: "main.cpp"
            Properties {
                condition: qbs.targetOS.contains("darwin")
                bundle.isBundle: false
            }
        }

        CppApplication {
            type: base.concat(["app-list"])
            consoleApplication: true
            name: "app list"
            Depends {
                productTypes: ["application"]
                limitToSubProject: true
            }
            files: ["main.cpp"]

            Rule {
                multiplex: true
                inputsFromDependencies: "application"
                Artifact {
                    filePath: "app-list.txt"
                    fileTags: "app-list"
                }
                prepare: {
                    var cmd = new JavaScriptCommand();
                    cmd.description = "Collecting apps";
                    cmd.sourceCode = function() {
                        var file = new TextFile(output.filePath, TextFile.WriteOnly);
                        for (var i = 0; i < inputs["application"].length; ++i)
                            file.writeLine(inputs["application"][i].filePath);
                        file.close();
                    };
                    return cmd;
                }
            }
        }
    }
}
