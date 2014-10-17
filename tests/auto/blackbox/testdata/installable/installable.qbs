import qbs
import qbs.TextFile

Project {
    CppApplication {
        type: ["application"]
        name: "app"
        Group {
            files: ["main.cpp"]
            qbs.install: true
        }

        Group {
            fileTagsFilter: product.type
            qbs.install: true
        }
    }

    Product {
        name: "install-list"
        type: ["install-list"]
        Depends { name: "app" }
        Rule {
            multiplex: true
            inputsFromDependencies: ["installable"]
            Artifact {
                filePath: "installed-files.txt"
                fileTags: product.type
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "Creating " + output.fileName;
                cmd.sourceCode = function() {
                    var file = new TextFile(output.filePath, TextFile.WriteOnly);
                    for (var i = 0; i < inputs.installable.length; ++i)
                        file.writeLine(inputs.installable[i].filePath);
                    file.close();
                };
                return [cmd];
            }
        }
    }
}
