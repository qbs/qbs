import qbs.Host
import qbs.TextFile

Product {
    property bool isSignedByDefault: {
        // on Apple silicon, apps are signed by default
        return Host.architecture() === "arm64"
               && (qbs.targetOS.includes("macos")
                || qbs.targetOS.includes("ios-simulator") && qbs.targetArchitecture === "arm64")
    }
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
                var present = product.moduleProperty("xcode", "present");
                tools["present"] = !!present;
                if (present) {
                    var keys = [
                        "developerPath",
                        "version"
                    ];
                    for (var i = 0; i < keys.length; ++i) {
                        var key = keys[i];
                        tools[key] = product.xcode[key];
                    }
                }
                tools["isSignedByDefault"] = product.isSignedByDefault;

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
