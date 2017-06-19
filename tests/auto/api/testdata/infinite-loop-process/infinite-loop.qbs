import qbs

Project {
    CppApplication {
        type: "application"
        consoleApplication: true // suppress bundle generation
        files: "main.cpp"
        name: "infinite-loop"
    }

    Product {
        type: "mytype"
        name: "caller"
        Depends { name: "infinite-loop" }
        Rule {
            inputsFromDependencies: "application"
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
