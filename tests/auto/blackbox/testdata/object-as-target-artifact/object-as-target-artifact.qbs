import qbs

Project {
    Product {
        type: ["obj"]
        name: "object"
        Depends { name: "cpp" }
        files: ["object.cpp"]
    }

    CppApplication {
        name: "app"
        Depends { name:"object" }
        files: ["main.cpp"]
    }
    DynamicLibrary {
        name: "lib"
        Depends { name:"object" }
        Depends { name: "cpp" }
        files: ["lib.cpp"]
    }
}
