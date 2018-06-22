import qbs.TextFile

Product {
    name: "p"
    type: ["p.out"]

    property bool enableTagger

    FileTagger {
        condition: enableTagger
        patterns: ["*.inp"]
        fileTags: ["p.in"]
    }

    Rule {
        multiplex: true
        requiresInputs: false
        inputs: ["p.in"]
        Artifact {
            filePath: "output.txt"
            fileTags: ["p.out"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Generating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                f.write('(');
                var inputsList = inputs["p.in"];
                if (inputsList) {
                    for (var i = 0; i < inputsList.length; ++i)
                        f.write(inputsList[i].fileName + ',');
                }
                f.write(')');
            };
            return [cmd];
        }
    }

    files: ["a.inp", "b.inp", "c.inp"]
}
