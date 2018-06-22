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
        Product {
            type: myconfig.typeDecider ? ["application"] : []
            Depends { name: "cpp" }
            Depends { name: "myconfig" }
            consoleApplication: true
            name: "app4"
            files: "main.cpp"
        }
        Product {
            name: "other-product"
            type: "other"
            Rule {
                multiplex: true
                Artifact {
                    filePath: "output.txt"
                    fileTags: "other"
                }
                prepare: {
                    var cmd = new JavaScriptCommand();
                    cmd.description = "creating " + output.filePath;
                    cmd.sourceCode = function() {
                        var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    }
                    return cmd;
                }
            }
            Export {
                Depends { productTypes: "other2" }
            }
        }
        Product {
            name: "yet-another-product"
            type: "other2"
            Rule {
                multiplex: true
                Artifact {
                    filePath: "output.txt"
                    fileTags: "other2"
                }
                prepare: {
                    var cmd = new JavaScriptCommand();
                    cmd.description = "creating " + output.filePath;
                    cmd.sourceCode = function() {
                        var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    }
                    return cmd;
                }
            }
        }
        Product {
            name: "helper"
            Export {
                Depends { productTypes: "other" }
            }
        }

        CppApplication {
            condition: false
            consoleApplication: true
            name: "disabled-app"
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
            Depends { name: "helper" }
            files: ["main.cpp"]

            Rule {
                multiplex: true
                inputsFromDependencies: ["application", "other", "other2"]
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
                        for (i = 0; i < inputs["other"].length; ++i)
                            file.writeLine(inputs["other"][i].filePath);
                        for (i = 0; i < inputs["other2"].length; ++i)
                            file.writeLine(inputs["other2"][i].filePath);
                        file.close();
                    };
                    return cmd;
                }
            }
        }
        Product {
            name: "dummy"
            Depends { productTypes: [] }
        }
    }
}
