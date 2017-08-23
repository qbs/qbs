import qbs 1.0

Project {
    Product {
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

