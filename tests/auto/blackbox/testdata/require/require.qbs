import 'blubb.js' as blubb

Product {
    type: ["text"]
    Rule {
        multiplex: true
        Artifact {
            fileTags: ["text"]
            filePath: "one.txt"
        }
        Artifact {
            fileTags: ["text"]
            filePath: "two.txt"
        }
        prepare: {
            var filePaths = outputs.text.map(function (artifact) {return artifact.filePath; });
            return blubb.createCommands(filePaths);
        }
    }
}

