import qbs.TextFile

Product {
    Depends { name: "xcode"; required: false }
    type: ["json"]
    Rule {
        multiplex: true
        Artifact {
            filePath: ["xcode.json"]
            fileTags: ["json"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};
                if (product.moduleProperty("xcode", "present")) {
                    var keys = [
                        "developerPath",
                        "version"
                    ];
                    for (var i = 0; i < keys.length; ++i) {
                        var key = keys[i];
                        tools[key] = product.xcode[key];
                    }
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
