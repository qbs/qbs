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

        Export {
            Depends { name: "cpp" }
            Properties {
                condition: qbs.targetOS.contains("unix")
                cpp.staticLibraries: ["pthread"]
            }
            Properties {
                condition: qbs.toolchain.contains("msvc")
                cpp.staticLibraries: ["setupapi"]
            }
        }
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
        consoleApplication: true

        Properties {
            condition: qbs.targetOS.contains("unix")
            cpp.defines: ["WITH_PTHREAD"]
            cpp.linkerFlags: ["-static"]
        }
        Properties {
            condition: qbs.toolchain.contains("msvc")
            cpp.defines: ["WITH_SETUPAPI"]
        }

        Depends { name: "e" }

        files: [
            "main.cpp",
        ]
    }
}
