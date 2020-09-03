import qbs.TextFile

Project {
    property int projectJobCount
    property int productJobCount
    property int moduleJobCount
    JobLimit {
        condition: projectJobCount !== -1
        jobPool: "singleton"
        jobCount: projectJobCount
    }
    JobLimit {
        condition: projectJobCount !== -1
        jobPool: "singleton"
        jobCount: 100
    }
    CppApplication {
        name: "tool"
        consoleApplication: true
        cpp.cxxLanguageVersion: "c++14"
        Properties {
            condition: qbs.targetOS.contains("macos")
            cpp.minimumMacosVersion: "10.9"
        }
        files: "main.cpp"
        Group {
            fileTagsFilter: "application"
            fileTags: "tool_tag"
        }
        Export {
            Rule {
                alwaysRun: true
                inputs: "tool_in"
                explicitlyDependsOnFromDependencies: "tool_tag"
                Artifact { filePath: input.completeBaseName + ".out"; fileTags: "tool_out" }
                prepare: {
                    var cmd = new Command(explicitlyDependsOn.tool_tag[0].filePath,
                                          [output.filePath]);
                    cmd.workingDirectory = product.buildDirectory;
                    cmd.description = "Running tool";
                    cmd.jobPool = "singleton";
                    return cmd;
                }
            }
            JobLimit {
                condition: project.moduleJobCount !== -1
                jobPool: "singleton"
                jobCount: project.moduleJobCount
            }
            JobLimit {
                condition: project.moduleJobCount !== -1
                jobPool: "singleton"
                jobCount: 200
            }
        }
    }
    Product {
        name: "p"
        type: "tool_out"
        Depends { name: "tool" }
        Rule {
            multiplex: true
            outputFileTags: "tool_in"
            outputArtifacts: {
                var artifacts = [];
                for (var i = 0; i < 5; ++i)
                    artifacts.push({filePath: "file" + i + ".in", fileTags: "tool_in"});
                return artifacts;
            }
            prepare: {
                var commands = [];
                for (var i = 0; i < outputs.tool_in.length; ++i) {
                    var cmd = new JavaScriptCommand();
                    var output = outputs.tool_in[i];
                    cmd.output = output.filePath;
                    cmd.description = "generating " + output.fileName;
                    cmd.sourceCode = function() {
                        var f = new TextFile(output, TextFile.WriteOnly);
                        f.close();
                    }
                    commands.push(cmd);
                };
                return commands;
            }
        }
        JobLimit {
            condition: project.productJobCount !== -1
            jobPool: "singleton"
            jobCount: project.productJobCount
        }
        JobLimit {
            condition: project.productJobCount !== -1
            jobPool: "singleton"
            jobCount: 300
        }
    }
}
