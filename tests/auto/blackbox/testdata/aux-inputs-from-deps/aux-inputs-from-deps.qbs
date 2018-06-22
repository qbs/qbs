import qbs.File
import qbs.TextFile

import "util.js" as Utils

Project {
    CppApplication {
        name: "app"
        files: ["main.cpp"]
        Depends { name: "dep" }
    }
    Product {
        name: "p"
        type: ["p.out"]
        Depends { name: "dep" }
        Rule {
            multiplex: true
            explicitlyDependsOnFromDependencies: ["hpp"]
            Artifact {
                filePath: "dummy.out"
                fileTags: ["p.out"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating dummy.out";
                cmd.sourceCode = function() {
                    File.copy(project.buildDirectory + "/dummy.h", output.filePath);
                };
                return [cmd];
            }
        }
    }
    Product {
        name: "dep"
        type: ["hpp"]
        property bool sleep: true
        Rule {
            inputs: ["blubb.in"]
            Artifact {
                filePath: project.buildDirectory + "/dummy.h"
                fileTags: ["hpp"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating header";
                cmd.sourceCode = function() {
                    if (product.sleep)
                        Utils.sleep(1000);
                    File.copy(input.filePath, output.filePath);
                }
                return [cmd];
            }
        }
        Rule {
            multiplex: true
            Artifact {
                filePath: "dummy.blubb"
                fileTags: ["blubb.in"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating blubb.in";
                cmd.sourceCode = function() {
                    if (product.sleep)
                        Utils.sleep(1000);
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                }
                return [cmd];
            }
        }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [project.buildDirectory]
        }
    }
}


