Project {
    CppApplication {
        name: "myapp"
        consoleApplication: true
        Depends { name: "foo.bar.bla" }
        files: ["app.cpp"]
    }
    StaticLibrary {
        Depends { name: "cpp" }
        name: "foo.bar.bla"
        files: ["lib.cpp"]
    }
}
