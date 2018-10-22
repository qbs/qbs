import qbs.File
import qbs.TextFile

Product {
    type: ["mytype.final"]

    Group {
        files: ["input.txt"]
        fileTags: ["mytype.in"]
    }

    Rule {
        inputs: ["mytype.in"]
        Artifact {
            filePath: "output.txt"
            fileTags: ["mytype.out"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating " + output.fileName;
            cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
            return [cmd];
        }
    }

    Rule {
        inputs: ["mytype.out"]
        outputFileTags: ["mytype.final", "dummy"]
        outputArtifacts: {
            var file;
            var inFile = new TextFile(input.filePath, TextFile.ReadOnly);
            try {
                file = inFile.readLine();
                if (!file)
                    throw "no file name found";
            } finally {
                inFile.close();
           }
            return [
                    { filePath: file, fileTags: ["mytype.final"] },
                    { filePath: "dummy", fileTags: ["dummy"], alwaysUpdated: false }
            ];
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            var output = outputs["mytype.final"][0];
            cmd.description = "generating " + output.fileName;
            cmd.outputFilePath = output.filePath;
            cmd.sourceCode = function() { File.copy(input.filePath, outputFilePath); };
            return [cmd];
        }
    }
}

