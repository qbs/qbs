Project {
    property bool useExport: true
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

        Group {
            condition: qbs.targetOS.includes("darwin")
            files: ["d.mm"]
        }

        Properties {
            condition: qbs.targetOS.includes("windows")
            cpp.defines: ["WITH_SETUPAPI"]
            cpp.staticLibraries: !project.useExport ? ["setupapi"] : []
        }
        Properties {
            condition: qbs.targetOS.includes("darwin")
            cpp.defines: ["WITH_ZLIB"]
            cpp.staticLibraries: !project.useExport ? ["z"] : []
            cpp.frameworks: !project.useExport ? ["Foundation"] : []
        }
        Properties {
            condition: {
                console.info(qbs.targetOS);
                return qbs.targetOS.includes("linux") || qbs.targetOS.includes("hurd")
            }
            cpp.defines: ["WITH_PTHREAD", "WITH_ZLIB"]
            cpp.staticLibraries: !project.useExport ? ["pthread", "z"] : []
        }
        Properties {
            condition: qbs.toolchain.contains("emscripten")
            cpp.defines: ["WITH_WEBSOCK"]
            cpp.staticLibraries: !project.useExport ? ["websocket.js"] : []
        }
        Export {
            condition : project.useExport
            Depends { name: "cpp" }
            Properties {
                condition: qbs.targetOS.contains("linux")
                    || qbs.targetOS.includes("hurd")
                cpp.staticLibraries: ["pthread", "z"]
            }
            Properties {
                condition: qbs.targetOS.contains("darwin")
                cpp.staticLibraries: ["z"]
                cpp.frameworks: ["Foundation"]
            }
            Properties {
                condition: qbs.targetOS.contains("windows")
                cpp.staticLibraries: ["setupapi"]
            }
            Properties {
                condition: qbs.toolchain.contains("emscripten")
                cpp.staticLibraries: ["websocket.js"]
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
        Export {
            Depends { name: "d" }
        }
    }
    CppApplication {
        name: "staticLibDeps"
        type: "application"
        consoleApplication: true

        Depends { name: "e" }

        Properties {
            condition: qbs.targetOS.includes("linux")
                || qbs.targetOS.includes("hurd")
            cpp.driverFlags: ["-static"]
        }

        files: [
            "main.cpp",
        ]
    }
}
