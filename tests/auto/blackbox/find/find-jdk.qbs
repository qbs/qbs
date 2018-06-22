import qbs.TextFile

Product {
    Depends { name: "java"; required: false }
    type: ["json"]
    Rule {
        multiplex: true
        Artifact {
            filePath: ["jdk.json"]
            fileTags: ["json"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};
                if (product.moduleProperty("java", "present")) {
                    tools["javac"] = product.moduleProperty("java", "compilerFilePath");
                    tools["java"] = product.moduleProperty("java", "interpreterFilePath");
                    tools["jar"] = product.moduleProperty("java", "jarFilePath");
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
