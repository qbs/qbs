import qbs.Host

Project {
    CppApplication {
        name: "tool"
        consoleApplication: true
        property bool _testPlatform: {
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        Profile {
            name: "debugProfile"
            baseProfile: project.profile
            qbs.buildVariant: "debug"
        }
        Profile {
            name: "releaseProfile"
            baseProfile: project.profile
            qbs.buildVariant: "release"
        }
        multiplexByQbsProperties: "profiles"
        qbs.profiles: ["debugProfile", "releaseProfile"]
        files: "tool.cpp"
        Properties {
            condition: qbs.buildVariant === "debug"
            cpp.defines: "WRONG_VARIANT"
        }
        Export {
            Rule {
                multiplex: true
                inputsFromDependencies: "application"
                Artifact {
                    filePath: "tool.out"
                    fileTags: "tool.output"
                }
                prepare: {
                    var cmd = new Command(input.filePath, []);
                    cmd.description = "creating " + output.fileName;
                    return cmd;
                }
            }
        }
    }
    Product {
        name: "p"
        type: "tool.output"
        multiplexByQbsProperties: "buildVariants"
        qbs.buildVariants: ["debug", "release"]
        Depends { name: "tool"; profiles: "releaseProfile" }
    }
    Product {
        name: "p2"
        type: "tool.output"
        multiplexByQbsProperties: "buildVariants"
        qbs.buildVariants: ["debug", "release"]
        Depends { name: "helper" }
    }
    Product {
        name: "helper"
        Export {
            Depends { name: "tool"; profiles: "releaseProfile" }
        }
    }
}
