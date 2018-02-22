import qbs

Project {
    CppApplication {
        name: "thetool"
        files: "main.cpp"

        Group {
            fileTagsFilter: ["application"]
            fileTags: ["thetool.thetool"]
            qbs.install: true
        }

        Export {
            Depends { name: "cpp" }
            Rule {
                multiplex: true
                explicitlyDependsOn: ["thetool.thetool"]
                Artifact {
                    filePath: "tool-output.txt"
                    fileTags: ["thetool.output"]
                }
                prepare: {
                    var cmd = new Command(explicitlyDependsOn["thetool.thetool"][0].filePath,
                                          output.filePath);
                    cmd.description = "running the tool";
                    return [cmd];
                }
            }
        }
    }

    Product {
        name: "user-in-project"
        type: ["thetool.output"]
        Depends { name: "thetool" }
    }
}
