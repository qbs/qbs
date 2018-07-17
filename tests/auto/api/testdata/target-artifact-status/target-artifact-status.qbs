import qbs.TextFile

Product {
    name: "p"
    type: "p_type"
    property bool enableTagging
    Rule {
        multiplex: true
        Artifact { filePath: "a1"; fileTags: "p_type" }
        Artifact { filePath: "a2"; fileTags: "x" }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating outputs";
            cmd.sourceCode = function() {
                var f = new TextFile(outputs.p_type[0].filePath, TextFile.WriteOnly);
                f.close();
                f = new TextFile(outputs.x[0].filePath, TextFile.WriteOnly);
                f.close();
            };
            return cmd;
        }
    }
    Group {
        condition: enableTagging
        fileTagsFilter: "x"
        fileTags: "p_type"
    }
}
