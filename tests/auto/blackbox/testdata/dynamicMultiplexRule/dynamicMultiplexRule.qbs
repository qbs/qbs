import qbs.TextFile

Project {
    Product {
        type: ["stuff"]
        Group {
            files: ["one.txt", "two.txt", "three.txt"]
            fileTags: ["text"]
        }
        Rule {
            multiplex: true
            inputs: "text"
            outputFileTags: ["stuff"]
            outputArtifacts: {
                return [{
                    filePath: "stuff-from-" + inputs.text.length + "-inputs",
                    fileTags: ["stuff"]
                }];
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.write("narf!");
                    f.close();
                }
                return cmd;
            }
        }
    }
}
