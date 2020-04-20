Project {
    CppApplication {
        name: "tool"
        files: "main.c"
    }

    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "p"
        type: "custom"
        Depends { name: "tool" }
        Rule {
            inputsFromDependencies: "application"
            outputFileTags: "custom"
            prepare: {
                var cmd = new Command(input.filePath, []);
                cmd.description = "running tool";
                cmd.environment = ["PATH=/opt/tool/bin"];
                return cmd;
            }
        }
    }
}

