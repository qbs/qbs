Module {
    property string p1
    property string p2

    Rule {
        inputs: ["in1"]
        Artifact {
            filePath: "dummy1.txt"
            fileTags: ["out1"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating " + output.fileName + " with " + product.m.p1;
            cmd.sourceCode = function() {};
            return [cmd];
        }
    }

    Rule {
        inputs: ["in2"]
        Artifact {
            filePath: "dummy2.txt"
            fileTags: ["out2"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating " + output.fileName + " with " + product.m.p2;
            cmd.sourceCode = function() {};
            return [cmd];
        }
    }
}
