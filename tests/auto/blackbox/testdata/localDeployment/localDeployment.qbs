Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        type: ["application"]
        consoleApplication: true
        name: "HelloWorld"
        destinationDirectory: "bin"

        Depends { name: "cpp" }
        cpp.cxxLanguageVersion: "c++11"

        Group {
            fileTagsFilter: product.type
            qbs.install: true
            qbs.installDir: "bin"
        }

        Group {
            qbs.install: true
            qbs.installDir: "share"
            files: ['main.cpp']
        }
    }
}

