import qbs.TextFile

Project {
    Product {
        name: "theDep"
        type: ["genheader"]

        Rule {
            multiplex: true
            Artifact {
                filePath: project.buildDirectory + "/theHeader.h"
                fileTags: product.type
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                }
                return [cmd];
            }
        }
    }
    CppApplication {
        name: "theApp"
        cpp.includePaths: [project.buildDirectory]
        files: ["main.cpp"]
    }
}


