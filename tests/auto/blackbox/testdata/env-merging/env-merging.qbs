import qbs.Host

Project {
    CppApplication {
        name: "tool"
        files: "main.c"
    }

    Product {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
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

