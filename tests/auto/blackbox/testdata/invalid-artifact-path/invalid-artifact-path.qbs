Project {
    property string artifactDir
    Product {
        type: "t"
        Rule {
            multiplex: true
            Artifact {
                filePath: project.artifactDir + "/file.out"
                fileTags: "t"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.sourceCode = function() {};
                return cmd;
            }
        }
    }
}
