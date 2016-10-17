import qbs

Project {
    QtApplication {
        name: "app"
        files: ["main.cpp"]

        // TODO: Use two groups with different base dirs once QBS-1005 is fixed.
        Qt.core.resourceSourceBase: "qrc-base"
        Qt.core.resourcePrefix: "/thePrefix"
        Group {
            prefix: "qrc-base/"
            files: [
                "resource1.txt",
                "subdir/resource2.txt",
            ]
            fileTags: ["qt.core.resource_data"]
        }
    }

    Product {
        name: "runner"
        type: ["runner"]
        Depends { name: "app" }
        Rule {
            inputsFromDependencies: ["application"]
            Artifact {
                filePath: "dummy"
                fileTags: ["runner"]
            }
            prepare: {
                var cmd = new Command(input.filePath);
                cmd.description = "running " + input.filePath;
                return [cmd];
            }
        }
    }
}
