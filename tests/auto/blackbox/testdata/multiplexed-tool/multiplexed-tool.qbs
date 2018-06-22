Project {
    CppApplication {
        name: "tool"
        consoleApplication: true
        Profile {
            name: "debugProfile"
            qbs.buildVariant: "debug"
        }
        Profile {
            name: "releaseProfile"
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
