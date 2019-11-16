import qbs 1.0

Project {
    DynamicLibrary {
        id: idLib1
        name: "lib1"
        Depends { name: "cpp" }
        files: ["lib1.cpp"]
        Depends { name: "bundle" }
        bundle.isBundle: false
    }

    DynamicLibrary {
        id: idLib2
        name: "lib2"
        Depends { name: "cpp" }
        files: ["lib2.cpp"]
        Depends { name: "bundle" }
        bundle.isBundle: false
    }

    DynamicLibrary {
        id: idLib3
        name: "lib3"
        Depends { name: "cpp" }
        files: ["lib3.cpp"]
        Depends { name: "bundle" }
        bundle.isBundle: false
    }

    CppApplication {
        name: "main"
        files: "main.cpp"

        Depends { name: "lib1"; cpp.link: false }
        Depends { name: "lib2"; cpp.link: false }
        Depends { name: "lib3"; cpp.link: false }

        cpp.libraryPaths: [
            idLib1.buildDirectory,
            idLib2.buildDirectory,
            idLib3.buildDirectory
        ]
        cpp.dynamicLibraries: [
            "lib1", "lib2", "lib1", "lib3", "lib2", "lib1"
        ]
    }
}
