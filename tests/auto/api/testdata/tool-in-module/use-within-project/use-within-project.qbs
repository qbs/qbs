import qbs.Host

Project {
    CppApplication {
        name: "thetool"
        consoleApplication: true
        files: "main.cpp"

        property bool skip: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
            return result;
        }

        install: true
        installDir: ""
        qbs.installPrefix: ""
        Group {
            fileTagsFilter: ["application"]
            fileTags: ["thetool.thetool"]
        }

        Export {
            Depends { name: "cpp" }
            Rule {
                multiplex: true
                explicitlyDependsOnFromDependencies: ["thetool.thetool"]
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
