Project {
    property bool kaputt: true
    Product {
        type: ["kaputt"]
        Group {
            name: "kaputt"
            condition: project.kaputt
            files: "input_kaputt.txt"
            fileTags: "input.kaputt"
        }
        Rule {
            inputs: "input.kaputt"
            Artifact {
                filePath: "output.kaputt"
                fileTags: "kaputt"
            }
            prepare: {
                var cmd = new Command("jibbetnich", [output.filePath]);
                cmd.silent = true;
                return cmd;
            }
        }
    }
}

