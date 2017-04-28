import qbs

Project {
    StaticLibrary {
        name: "theLib"
        destinationDirectory: project.buildDirectory
        Depends { name: "cpp" }
        bundle.isBundle: false
        files: ["lib.cpp"]
    }
    CppApplication {
        name: "theApp"
        cpp.libraryPaths: project.buildDirectory
        files: ["main.cpp"]
        cpp.staticLibraries: ['@' + sourceDirectory + '/'
                + (qbs.toolchain.contains("msvc") ? "list.msvc" : "list.gcc")]
    }
}
