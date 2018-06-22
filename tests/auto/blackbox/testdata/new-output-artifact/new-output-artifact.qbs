import qbs.TextFile

Product {
    name: "theProduct"
    type: ["output"]
    property int artifactCount: 99
    Group {
        files: ["input.txt"]
        fileTags: ["input"]
    }
    qbs.installPrefix: ""
    Group {
        fileTagsFilter: product.type
        qbs.install: true
    }
    Rule {
        inputs: ["input"]
        outputFileTags: ["output"]
        outputArtifacts: {
            var list = [];
            for (var i = 0; i < product.artifactCount; ++i)
                list.push({ filePath: "output_" + i + ".out", fileTags: ["output"]});
            return list;
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                for (var i = 0; i < outputs["output"].length; ++i) {
                    var f = new TextFile(outputs["output"][i].filePath, TextFile.WriteOnly);
                    f.close();
                }
            }
            return [cmd];
        }
    }
}
