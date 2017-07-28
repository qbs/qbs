import qbs
import qbs.File

Project {
    CppApplication {
        name: "my_app"
        files: "main.cpp"
        Group {
            fileTagsFilter: ["application"]
            fileTags: ["app-to-compress"]
        }
    }
    Product {
        name: "my_compressed_app"
        type: ["compressed_application"]
        Depends { name: "my_app" }
        Rule {
            inputsFromDependencies: ["app-to-compress"]
            Artifact {
                filePath: "compressed-" + input.fileName
                fileTags: ["compressed_application"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "compressing " + input.fileName;
                cmd.highlight = "linker";
                cmd.sourceCode = function () {
                    File.copy(input.filePath, output.filePath);
                };
                return cmd;
            }
        }
    }
}
