import qbs.TextFile

Project {
    property string libDir: sourceDirectory + "/libs"
    StaticLibrary {
        name: "lib1"
        destinationDirectory: project.libDir
        Depends { name: "cpp" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["lib1.cpp"]
    }
    StaticLibrary {
        name: "lib2"
        destinationDirectory: project.libDir
        Depends { name: "cpp" }
        Depends { name: "lib1" }
        Properties {
            condition: qbs.targetOS.contains("darwin")
            bundle.isBundle: false
        }
        files: ["lib2.cpp"]
    }
    CppApplication {
        Depends { name: "lib1"; cpp.link: false }
        Depends { name: "lib2"; cpp.link: false }
        files: ["main.cpp"]
        cpp.libraryPaths: [project.libDir]
        cpp.staticLibraries: ["lib1", "lib2", "lib1"]
    }
}
