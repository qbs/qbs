import qbs.TextFile

Product {
    Depends { name: "cli"; required: false }
    type: ["json"]
    Rule {
        multiplex: true
        Artifact {
            filePath: ["cli.json"]
            fileTags: ["json"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};
                if (product.moduleProperty("cli", "present"))
                    tools["toolchainInstallPath"] = product.moduleProperty("cli",
                                                                           "toolchainInstallPath");

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
