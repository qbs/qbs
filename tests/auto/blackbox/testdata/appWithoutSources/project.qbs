import qbs 1.0

Project {
    StaticLibrary {
        name: "a"

        Depends { name: "cpp" }

        files: [
            "a.cpp",
        ]
    }

    StaticLibrary {
        name: "b"

        Depends { name: "a" }
        Depends { name: "cpp" }

        files: [
            "b.cpp",
        ]
    }

    CppApplication {
        name: "appWithoutSources"
        Depends { name: "a" }
        Depends { name: "b" }
    }
}
