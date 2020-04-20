Project {
    QtApplication {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }

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
}
