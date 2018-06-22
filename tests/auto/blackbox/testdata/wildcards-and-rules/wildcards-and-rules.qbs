import qbs.TextFile

Product {
    name: "wildcards-and-rules"
    type: "mytype"
    files: ["*.inp", "*.dep"]
    FileTagger {
        patterns: "*.inp"
        fileTags: ["inp"]
    }
    FileTagger {
        patterns: "*.dep"
        fileTags: ["dep"]
    }
    Rule {
        multiplex: true
        inputs: ["inp"]
        explicitlyDependsOn: ["dep"]
        Artifact {
            filePath: "test.mytype"
            fileTags: product.type
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating output artifact";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var file = new TextFile(output.filePath, TextFile.WriteOnly);
                for (var i = 0; i < inputs.inp.length; ++i)
                    file.writeLine(inputs.inp[i].fileName);
                file.close();
            }
            return cmd;
        }
    }
}
