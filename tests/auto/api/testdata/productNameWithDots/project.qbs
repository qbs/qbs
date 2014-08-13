import qbs
Project {
    Application {
        Depends { name: "cpp" }
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
