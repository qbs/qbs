import qbs

Project {
    CppApplication {
        name: "app1"
        type: ["application"]
        files: ["main.cpp"]
        cpp.separateDebugInformation: true
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo1"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        cpp.separateDebugInformation: true
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar1"
        files: ["foo.cpp"]
        cpp.separateDebugInformation: true
    }
    CppApplication {
        name: "app2"
        type: ["application"]
        files: ["main.cpp"]
        cpp.separateDebugInformation: false
    }
    DynamicLibrary {
        Depends { name: "cpp" }
        name: "foo2"
        type: ["dynamiclibrary"]
        files: ["foo.cpp"]
        cpp.separateDebugInformation: false
    }
    LoadableModule {
        Depends { name: "cpp" }
        name: "bar2"
        files: ["foo.cpp"]
        cpp.separateDebugInformation: false
    }
}
