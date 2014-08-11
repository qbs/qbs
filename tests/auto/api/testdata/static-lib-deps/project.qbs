import qbs 1.0

Project {
    StaticLibrary {
        name: "a"

        Depends { name: "cpp" }

        files: [
            "a1.cpp",
            "a2.cpp",
        ]
    }
    StaticLibrary {
        name: "b"

        Depends { name: "cpp" }

        Depends { name: "a" }

        files: [
            "b.cpp",
        ]
    }
    StaticLibrary {
        name: "c"

        Depends { name: "cpp" }

        Depends { name: "a" }

        files: [
            "c.cpp",
        ]
    }
    StaticLibrary {
        name: "d"

        Depends { name: "cpp" }

        Depends { name: "b" }
        Depends { name: "c" }

        files: [
            "d.cpp",
        ]
    }
    StaticLibrary {
        name: "e"

        Depends { name: "cpp" }

        Depends { name: "d" }

        files: [
            "e.cpp",
        ]
    }
    CppApplication {
        name: "staticLibDeps"
        type: "application"

        Depends { name: "e" }

        files: [
            "main.cpp",
        ]
    }
}
