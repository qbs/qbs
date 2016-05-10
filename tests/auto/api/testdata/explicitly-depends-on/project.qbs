import qbs
import qbs.TextFile

Product {
    type: "mytype"
    files: "dependency.txt"
    FileTagger {
        patterns: "*.txt"
        fileTags: ["txt"]
    }
    Rule {
        multiplex: true
        explicitlyDependsOn: "txt"
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
                file.truncate();
                file.close();
            }
            return cmd;
        }
    }
}
