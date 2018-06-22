import qbs.File

Project {
    Product {
        name: "dep"

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [importingProduct.buildDirectory + "/random_dir"]

            Rule {
                inputs: ["hpp_in"]
                Artifact {
                    filePath: product.buildDirectory + "/random_dir/" + input.completeBaseName
                    fileTags: ["hpp"]
                }
                prepare: {
                    var cmd = new JavaScriptCommand();
                    cmd.description = "Copying file";
                    cmd.sourceCode = function() {
                        File.copy(input.filePath, output.filePath);
                    }
                    return [cmd];
                }
            }
        }
    }

    CppApplication {
        name: "theProduct"
        Depends { name: "dep" }
        Group {
            files: ["header.h.in"]
            fileTags: ["hpp_in"]
        }
        files: ["main.cpp"]
    }

    CppApplication {
        name: "theProduct2"
        Depends { name: "dep" }
    }
}
