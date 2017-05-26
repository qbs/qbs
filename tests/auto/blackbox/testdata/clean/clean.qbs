import qbs 1.0

Project {
    DynamicLibrary {
        Depends { name: "cpp" }
        version: "1.1.0"
        name: "dep"
        files: "dep.cpp"
        bundle.isBundle: false
    }

    CppApplication {
        consoleApplication: true
        name: "app"
        Depends { name: "dep" }
        files: "main.cpp"
    }
}
