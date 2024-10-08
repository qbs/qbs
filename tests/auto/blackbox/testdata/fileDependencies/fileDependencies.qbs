Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "myapp"
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
        Depends { name: "cpp" }
        cpp.includePaths: ["awesomelib"]
        files: ["src/narf.h", "src/narf.cpp", "src/zort.cpp"]
        //     Group {
        //     fileTagsFilter: product.type
        //     qbs.install: true
        // }
    }
}

