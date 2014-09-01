import qbs
import qbs.TextFile

Project {
    CppApplication {
        type: "application"
        name: "no-match"
        files: "main.cpp"
    }

    Project {
        CppApplication {
            type: "application"
            name: "app1"
            files: "main.cpp"
        }
        CppApplication {
            type: "application"
            name: "app2"
            files: "main.cpp"
        }
        CppApplication {
            type: "application"
            name: "app3"
            files: "main.cpp"
        }

        DynamicLibrary {
            Depends { name: "cpp" }
            name: "lib-product"
            files: "main.cpp"
        }

        CppApplication {
            type: ["application", "app-list"]
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
