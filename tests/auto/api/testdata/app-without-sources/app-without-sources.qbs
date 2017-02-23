import qbs 1.0

Project {
    StaticLibrary {
        name: "a"

        Depends { name: "cpp" }

        files: [
            "a.c",
        ]
    }

    StaticLibrary {
        name: "b"

        Depends { name: "a" }
        Depends { name: "cpp" }

        files: [
            "b.c",
        ]
    }

    CppApplication {
        name: "appWithoutSources"

        // HACK: cpp.entryPoint currently not working 100% with gcc
        Properties {
            condition: qbs.toolchain.contains("msvc")
            cpp.entryPoint: "main"
        }
        cpp.entryPoint: undefined
        bundle.isBundle: false

        Depends { name: "a" }
        Depends { name: "b" }
    }
}
