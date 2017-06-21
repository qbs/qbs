import qbs
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
        name: "dep"
        type: ["hpp"]
        Rule {
            inputs: ["blubb.in"]
            Artifact {
                filePath: "dummy.h"
                fileTags: ["hpp"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating header";
                cmd.sourceCode = function() {
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
                cmd.description = "blubb.in";
                cmd.sourceCode = function() {
                    Utils.sleep(1000);
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                }
                return [cmd];
            }
        }
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [product.buildDirectory]
        }
    }
}


