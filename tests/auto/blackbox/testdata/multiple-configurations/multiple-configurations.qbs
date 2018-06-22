Project {
    StaticLibrary {
        name: "lib"
        Depends { name: "cpp" }
        files: ["lib.cpp", "lib.h"]
    }
    CppApplication {
        name: "app"
        Depends { name: "lib" }
        cpp.includePaths: project.sourceDirectory
        files: ["file.cpp", "file.h", "main.cpp"]
    }
}
