import qbs.TextFile

Product {
    name: "install-test"
    type: ["text"]
    qbs.installPrefix: ""
    Group {
        qbs.install: true
        qbs.installDir: "textfiles"
        fileTagsFilter: "text"
    }

    Rule {
        multiplex: true
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
