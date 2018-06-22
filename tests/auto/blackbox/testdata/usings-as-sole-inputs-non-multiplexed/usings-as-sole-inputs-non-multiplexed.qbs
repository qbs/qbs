import qbs.FileInfo
import qbs.TextFile

Project {
    Product {
        name: "p1"
        type: "custom"
        Group {
            files: "custom1.in"
            fileTags: "custom.in"
        }
    }
    Product {
        name: "p2"
        type: "custom"
        Group {
            files: "custom2.in"
            fileTags: "custom.in"
        }
    }

    Rule {
        inputs: "custom.in"
        Artifact {
            filePath: FileInfo.baseName(input.filePath) + ".out"
            fileTags: "custom"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.close();
            }
            return cmd;
        }
    }

    Product {
        name: "p3"
        type: "custom-plus"
        Depends { name: "p1" }
        Depends { name: "p2" }
        Rule {
            inputsFromDependencies: "custom"
            Artifact {
                filePath: FileInfo.fileName(input.filePath) + ".plus"
                fileTags: "custom-plus"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.close();
                }
            return cmd;
            }
        }
    }
}
