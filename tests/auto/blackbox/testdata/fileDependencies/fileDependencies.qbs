import qbs 1.0

Project {
    Product {
        type: "application"
        consoleApplication: true
        name: "myapp"
        Depends { name: "cpp" }
        cpp.includePaths: ["awesomelib"]
        files: ["src/narf.h", "src/narf.cpp", "src/zort.cpp"]
    }
}

