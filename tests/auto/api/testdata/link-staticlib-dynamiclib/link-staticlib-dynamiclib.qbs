Project {
    CppApplication {
        Depends { name: "mystaticlib" }
        name: "app"
        consoleApplication: true
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
        files: ["main.cpp"]
    }
    StaticLibrary {
        Depends { name: "cpp" }
        Depends { name: "mydynamiclib" }
        name: "mystaticlib"
        files: ["mystaticlib.cpp"]
    }
    DynamicLibrary {
        name: "mydynamiclib"
        Depends { name: "cpp" }
        files: ["mydynamiclib.cpp"]
    }
}
