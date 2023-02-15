Project {
    StaticLibrary {
        name: "theLib"
        destinationDirectory: project.buildDirectory
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.includes("darwin")
            bundle.isBundle: false
        }
        files: ["lib.cpp"]
    }
    CppApplication {
        name: "theApp"
        cpp.libraryPaths: project.buildDirectory
        files: ["main.cpp"]
        cpp.staticLibraries: ['@' + sourceDirectory + '/'
                + (qbs.toolchain.includes("msvc") ? "list.msvc" : "list.gcc")]
    }
}
