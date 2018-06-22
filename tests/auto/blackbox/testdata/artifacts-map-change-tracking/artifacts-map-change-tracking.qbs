import qbs.File

Project {
    CppApplication {
        name: "TheApp"
        targetName: "TheBinary"
        Rule {
            inputs: "cpp.in"
            Artifact { filePath: "test.cpp"; fileTags: 'cpp' }
            prepare: {
                console.info("running rule for " + output.fileName);
                var cmd = new JavaScriptCommand();
                cmd.description = "creating " + output.fileName;
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
                return cmd;
            }
        }
        files: ["main.cpp", /* 'test.txt' */]
        Group {
            files: "test.cpp.in"
            fileTags: "cpp.in"
        }
    }
    Product {
        name: "meta"
        type: "custom"
        Depends { name: "TheApp" }
        Group {
            files: "dummy.in"
            fileTags: "dummy.in"
        }
        Rule {
            inputs: ["dummy.in"]
            Artifact { filePath: "dummy"; fileTags: 'custom' }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "printing artifacts";
                cmd.sourceCode = function() {
                    var dep;
                    for (var i = 0; i < product.dependencies.length; ++i) {
                        var d = product.dependencies[i];
                        if (d.name === "TheApp") {
                            dep = d;
                            break;
                        }
                    }
                    for (var p in dep.artifacts) {
                        var list = dep.artifacts[p];
                        for (var i = 0; i < list.length; ++i)
                            console.info(list[i].fileName);
                    }
                    File.copy(input.filePath, output.filePath);
                };
                return cmd;
            }
        }
    }
}
