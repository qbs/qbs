import qbs
Project {
    Product {
        Depends { name: "cpp" }
        type: ["application"]
        name: "myapp"
        Depends { name: "foo.bar.bla" }
        files: ["app.cpp"]
    }
    StaticLibrary {
        Depends { name: "cpp" }
        name: "foo.bar.bla"
        files: ["lib.cpp"]
    }
}
