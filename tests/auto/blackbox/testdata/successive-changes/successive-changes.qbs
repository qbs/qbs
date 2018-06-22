import qbs.TextFile

Project {
    property string version: "1"
    Product {
        name: "theProduct"
        type: ["output"]
        Group {
            files: ["input.in"]
            fileTags: ["input"]
        }
        Rule {
            inputs: ["input"]
            Artifact {
                filePath: "output.out"
                fileTags: ["output"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "Creating output";
                cmd.sourceCode = function() {
                    var f = new TextFile(output.filePath, TextFile.WriteOnly);
                    f.write(project.version);
                }
                return [cmd];
            }
        }
    }
}
