import qbs

Project {
    CppApplication {
        name: "app"
        files: ["main.cpp"]
    }
    Product {
        name: "app-caller"
        type: "mytype"
        Depends { name: "app" }
        property bool redirectStdout
        property bool redirectStderr
        property int argument
        Rule {
            inputsFromDependencies: ["application"]
            Artifact {
                filePath: "dummy"
                fileTags: product.type
                alwaysUpdated: false
            }
            prepare: {
                var cmd = new Command(inputs["application"][0].filePath, [product.argument]);
                if (product.redirectStdout)
                    cmd.stdoutFilePath = product.buildDirectory + "/stdout.txt";
                if (product.redirectStderr)
                    cmd.stderrFilePath = product.buildDirectory + "/stderr.txt";
                cmd.description = "Building app-caller";
                return [cmd];
            }
        }
    }
}
