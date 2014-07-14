import qbs

Project {
    QtApplication {
        type: "application" // suppress bundle generation
        files: "main.cpp"
        name: "infinite-loop"
    }

    Product {
        type: "mytype"
        name: "caller"
        Depends { name: "infinite-loop" }
        Rule {
            usings: "application"
            Artifact {
                filePath: "dummy"
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
