import qbs

Project {
    CppApplication {
        name: "tool"
        files: "main.c"
    }

    Product {
        name: "p"
        type: "custom"
        Depends { name: "tool" }
        Rule {
            inputsFromDependencies: "application"
            Artifact {
                filePath: "dummy"
                fileTags: "custom"
            }
            prepare: {
                var cmd = new Command(input.filePath, []);
                cmd.description = "running tool";
                cmd.environment = ["PATH=/opt/tool/bin"];
                return cmd;
            }
        }
    }
}

