import qbs 1.0
import qbs.TextFile

Product {
    name: "install-test"
    Group {
        qbs.install: true
        qbs.installDir: "textfiles"
        fileTagsFilter: "text"
    }

    Transformer {
        Artifact {
            filePath: "HelloWorld.txt"
            fileTags: ["text"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating file:'" + output.fileName + "'";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.writeLine("Hello World!")
                file.close();
            }
            return cmd;
        }
    }
}
