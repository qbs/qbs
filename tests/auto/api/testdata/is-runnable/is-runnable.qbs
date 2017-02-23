import qbs

Project {
    CppApplication {
        name: "app"
    }
    DynamicLibrary {
        name: "lib"
        bundle.isBundle: false
    }
}
