import qbs.TextFile

Product {
    type: ["txt.out"]
    qbs.installPrefix: ""
    Group {
        files: "dir/**"
        qbs.install: true
        qbs.installDir: "dir"
    }
    FileTagger {
        patterns: ["*.txt"]
        fileTags: ["txt.in"]
    }
    Rule {
        multiplex: true
        requiresInputs: false
        explicitlyDependsOn: ["txt.in"]
        Artifact {
            filePath: "output.txt"
            fileTags: product.type
            qbs.install: true
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Creating " + output.fileName;
            cmd.sourceCode = function() {
                var inputList = explicitlyDependsOn["txt.in"];
                var fileNameList = [];
                for (var i = 0; i < inputList.length; ++i)
                    fileNameList.push(inputList[i].fileName);
                fileNameList.sort();
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                try {
                    f.write(fileNameList.join(''));
                } finally {
                    f.close();
                }
            };
            return [cmd];
        }
    }
}
