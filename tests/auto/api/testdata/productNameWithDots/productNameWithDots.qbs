import qbs
Project {
    Product {
        Depends { name: "cpp" }
        Depends { name: "bundle" }
        type: ["application"]
        name: "myapp"
        Depends { name: "foo.bar.bla" }
        files: ["app.cpp"]
        bundle.isBundle: false
    }
    StaticLibrary {
        Depends { name: "cpp" }
        name: "foo.bar.bla"
        files: ["lib.cpp"]
    }
}
