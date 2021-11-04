import qbs.TextFile

Product {
    type: "txt_output"

    Group {
        name: "lorem_ipsum"
        files: "lorem_ipsum.txt"
        fileTags: "txt_input"
    }

    //![1]
    Rule {
        multiplex: false
        inputs: ["txt_input"]
        Artifact {
            filePath: input.fileName + ".out"
            fileTags: ["txt_output"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating" + output.fileName + " from " + input.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var file = new TextFile(input.filePath);
                var content = file.readAll();
                file.close()
                content = content.toUpperCase();
                file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.write(content);
                file.close();
            }
            return [cmd];
        }
    }
}
