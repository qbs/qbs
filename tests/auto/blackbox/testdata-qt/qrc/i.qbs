Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        consoleApplication: true
        type: "application"
        name: "i"

        Depends {
            name: "Qt.core"
        }

        files: [
            "bla.cpp",
            "bla.qrc",
            //"test.cpp",
        ]
    }
}

