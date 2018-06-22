import "prepare.js" as PrepareHelper

Product {
    type: ["output"]
    Group {
        files: ["test.txt"]
        fileTags: ["input"]
    }
    Rule {
        inputs: ["input"]
        Artifact {
            filePath: "dummy"
            fileTags: product.type
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            PrepareHelper.prepare(cmd);
            cmd.description = "Creating output";
            return [cmd];
        }
    }
}
