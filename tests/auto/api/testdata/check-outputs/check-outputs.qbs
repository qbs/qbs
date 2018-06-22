import qbs.File

Project {
    Product {
        type: 'application'
        consoleApplication: true
        Group {
            files: 'foo.txt'
            fileTags: ['text']
        }
        Depends { name: 'cpp' }
    }

    Rule {
        inputs: ['text']
        Artifact {
            fileTags: ['cpp']
            filePath: input.baseName + '.cpp'
        }
        Artifact {
            fileTags: ['ghost']
            filePath: input.baseName + '.ghost'
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.inp = inputs["text"][0].filePath;
            cmd.out = outputs["cpp"][0].filePath;
            cmd.description = "generating " + outputs["cpp"][0].fileName;
            cmd.sourceCode = function() {
                File.copy(inp, out);
            };
            return cmd;
        }
    }
}
