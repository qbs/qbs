import qbs.TextFile

Product {
    type: "testproduct"
    files: "../lib/lib.h"

    Rule {
        multiplex: true
        Artifact {
            fileTags: ["testproduct"]
            filePath: "fubar"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating text file";
            cmd.sourceCode = function() {
                var tf = new TextFile(output.filePath, TextFile.WriteOnly);
                tf.writeLine("blubb");
                tf.close();
            }
            return cmd;
        }
    }
}
