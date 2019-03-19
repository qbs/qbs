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
        consoleApplication: true

        // HACK: cpp.entryPoint currently not working 100% with gcc
        Properties {
            condition: qbs.toolchain.contains("msvc")
            cpp.entryPoint: "main"
            cpp.dynamicLibraries: ["ucrt", "kernel32"]
        }
        cpp.entryPoint: undefined

        Depends { name: "a" }
        Depends { name: "b" }
    }
}
