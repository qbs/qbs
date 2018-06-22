Project {
    CppApplication {
        Depends { name: "other" }
        files: ["main.c"]
        cpp.includePaths: ["."]
    }

    DynamicLibrary {
        name: "other"
        files: ["main.c"]

        property stringList includePaths: []
        Export {
            Depends { name: "cpp" }
            cpp.includePaths: includePaths
        }
    }
}
