import qbs.TextFile

Product {
    Depends { name: "nodejs"; required: false }
    type: ["json"]
    Rule {
        multiplex: true
        Artifact {
            filePath: ["nodejs.json"]
            fileTags: ["json"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};
                if (product.moduleProperty("nodejs", "present")) {
                    tools["node"] = product.moduleProperty("nodejs", "interpreterFilePath");
                }

                var tf;
                try {
                    tf = new TextFile(output.filePath, TextFile.WriteOnly);
                    tf.writeLine(JSON.stringify(tools, undefined, 4));
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return cmd;
        }
    }
}
