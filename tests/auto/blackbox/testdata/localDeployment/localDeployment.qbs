import qbs.Host

Project {
    Product {
        condition: {
            var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
            if (!result)
                console.info("target platform/arch differ from host platform/arch");
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

