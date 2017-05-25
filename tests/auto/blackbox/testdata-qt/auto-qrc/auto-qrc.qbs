import qbs

Project {
    QtApplication {
        name: "app"
        files: ["main.cpp"]

        Group {
            prefix: "qrc-base/"

            Qt.core.resourcePrefix: "/thePrefix"
            Qt.core.resourceSourceBase: "qrc-base"

            files: ["resource1.txt"]
            fileTags: ["qt.core.resource_data"]

            Group {
                prefix: "qrc-base/subdir/"

                Qt.core.resourceSourceBase: "qrc-base/subdir"

                files: ["resource2.txt"]

                Group {
                    prefix: "qrc-base/subdir/"

                    Qt.core.resourcePrefix: "/theOtherPrefix"

                    files: ["resource3.txt"]
                }
            }
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
