import qbs
import qbs.TextFile

Product {
    type: "mytype"
    files: "dependency.txt"
    FileTagger {
        pattern: "*.txt"
        fileTags: ["txt"]
    }
    Transformer {
        explicitlyDependsOn: "txt"
        Artifact {
            fileName: "test.mytype"
            fileTags: product.type
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating output artifact";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var file = new TextFile(output.fileName, TextFile.WriteOnly);
                file.truncate();
                file.close();
            }
            return cmd;
        }
    }
}
