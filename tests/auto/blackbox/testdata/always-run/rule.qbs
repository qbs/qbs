import qbs.TextFile

Product {
    type: ["blubb"]
    Group {
        files: ["dummy.txt"]
        fileTags: ["dummy"]
    }

    Rule {
        alwaysRun: false
        inputs: ["dummy"]
        Artifact {
            filePath: "blubb.txt"
            fileTags: product.type
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "yo";
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.write("blubb");
                f.close();
            }
            return [cmd];
        }
    }
}
