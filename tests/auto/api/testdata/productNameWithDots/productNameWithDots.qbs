Project {
    CppApplication {
        name: "myapp"
        consoleApplication: true
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
        Depends { name: "foo.bar.bla" }
        files: ["app.cpp"]
    }
    StaticLibrary {
        Depends { name: "cpp" }
        name: "foo.bar.bla"
        files: ["lib.cpp"]
    }
}
