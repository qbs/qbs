import qbs.TextFile

Product {
    name: "p"
    property string errorType
    type: ["output"]
    Group {
        files: ["input.txt"]
        fileTags: ["input"]
    }
    Rule {
        inputs: ["input"]
        Artifact {
            filePath: "dummy"
            fileTags: ["output"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating output";
            if (product.errorType === "qobject")
                cmd.dummy = new TextFile(input.filePath, TextFile.ReadOnly);
            else if (product.errorType === "input")
                cmd.dummy = input;
            else if (product.errorType === "artifact")
                cmd.dummy = product.artifacts.qbs[0];
            else
                throw "invalid error type " + product.errorType;
            cmd.sourceCode = function() { }
            return [cmd];
        }
    }
}
