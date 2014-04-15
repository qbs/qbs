import qbs

Project {
    CppApplication {
        type: "application" // suppress bundle generation
        Depends { name: "Qt.core" }
        files: "main.cpp"
        name: "infinite-loop"
    }

    Product {
        type: "mytype"
        name: "caller"
        Depends { name: "infinite-loop" }
        Group {
            files: "dummy-input.txt" // Needed because of QBS-277
            fileTags: "schnurz"
        }
        Rule {
            inputs: "schnurz"
            usings: "application"
            Artifact {
                fileName: "dummy"
                fileTags: "mytype"
            }
            prepare: {
                var cmd = new Command(inputs["application"][0].filePath);
                cmd.description = "Calling application that runs forever";
                return cmd;
            }
        }
    }
}
